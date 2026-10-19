/*
渲染交换链重建测试（L2 隐藏窗口）
覆盖：未初始化 / 非法尺寸守卫、设置尺寸后重建在下一帧兑现、交换链范围与回读尺寸随新尺寸、
      连续多次「改尺寸 → 重建 → 出帧」后帧计数持续推进、重建后几何与回读内容仍正确。
说明：
  1. 断言一律按「通道序无关 ＋ 容差」编写，不依赖具体表面格式（B8G8R8A8 / R8G8B8A8、
     以及 sRGB 与非 sRGB 附件的取值差异均可通过）；不涉及 Vulkan 细节。
  2. 重建路径本次改为「旧资源延迟销毁（无整帧停顿）」，本用例即为其验收：Debug 构建下
     渲染器默认开启校验层，若延迟销毁与在途帧错配（销毁仍被引用对象）会由校验层报出，
     用例随渲染流程一并实跑判定。
*/
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

//获取渲染器门面与网格数据
#include "Application/Renderer/src/render/Renderer/渲染器.h"

namespace
{
	//初始窗口尺寸（与像素回读用例一致，小尺寸便于逐像素遍历）
	constexpr uint32_t INIT_WIDTH = 64;
	constexpr uint32_t INIT_HEIGHT = 36;

	/*
	清屏色候选（渲染器清屏色为 0.05 / 0.05 / 0.10 / 1.0）
	同一清屏色在「非 sRGB（UNORM）」与「sRGB」附件下编码不同：
	UNORM ≈ 13 / 13 / 26；sRGB ≈ 63 / 63 / 89；两者排序后均为「两低一高 ＋ 不透明」。
	*/
	constexpr std::array<std::array<uint8_t, 3>, 2> CLEAR_CANDIDATES = { {
		{ { 13, 13, 26 } },
		{ { 63, 63, 89 } },
	} };
	//清屏色通道容差（涵盖取整与线性 / sRGB 编码的细微差异）
	constexpr int CLEAR_TOLERANCE = 4;

	//把像素写成可读文本（失败诊断用）
	std::string pixel_text(const std::array<uint8_t, 4>& value)
	{
		return std::to_string(value[0]) + "/" + std::to_string(value[1]) + "/"
			+ std::to_string(value[2]) + "/" + std::to_string(value[3]);
	}

	//构造隐藏窗口配置
	engine::Renderer_Config hidden_config(void)
	{
		engine::Renderer_Config config;
		config.width = INIT_WIDTH;
		config.height = INIT_HEIGHT;
		config.hidden = true;
		return config;
	}

	//取指定像素的四个字节（width 为该帧回读宽度，用于计算行内偏移）
	std::array<uint8_t, 4> pixel_get(const std::vector<uint8_t>& pixels, uint32_t width, uint32_t x, uint32_t y)
	{
		//行内偏移（每行宽 × 4 字节）
		std::size_t offset = (static_cast<std::size_t>(y) * width + x) * 4;
		std::array<uint8_t, 4> value{};
		for (std::size_t index = 0; index < 4; ++index)
			value[index] = pixels[offset + index];
		return value;
	}

	//升序排序后的四通道值（消除通道序差异）
	std::array<uint8_t, 4> pixel_sorted(std::array<uint8_t, 4> value)
	{
		std::sort(value.begin(), value.end());
		return value;
	}

	//判断像素是否近似等于清屏色（排序后与两种编码候选逐一比对，兼容通道序与传输函数差异）
	bool pixel_is_clear(std::array<uint8_t, 4> value)
	{
		//排序后的四通道
		std::array<uint8_t, 4> sorted = pixel_sorted(value);
		//末位须为不透明
		if (sorted[3] < 250)
			return false;
		//逐候选比对前三个通道
		for (const std::array<uint8_t, 3>& candidate : CLEAR_CANDIDATES)
		{
			//三个通道均落在容差内即命中共色
			bool matched = true;
			for (std::size_t index = 0; index < 3; ++index)
			{
				//本通道与候选值的差
				int delta = static_cast<int>(sorted[index]) - static_cast<int>(candidate[index]);
				//若超出容差
				if (delta < -CLEAR_TOLERANCE || delta > CLEAR_TOLERANCE)
					matched = false;
			}
			//命中则返回真
			if (matched)
				return true;
		}
		return false;
	}

	//构造覆盖视口中央的四边形（NDC 尺度 1，位于 z = 0；默认相机在 +Z 3 单位处）
	engine::Mesh_Data quad_data(void)
	{
		engine::Mesh_Data data;
		data.vertices = {
			{ { -1.0f, -1.0f, 0.0f }, { 0.0f, 1.0f } },
			{ { 1.0f, -1.0f, 0.0f }, { 1.0f, 1.0f } },
			{ { 1.0f, 1.0f, 0.0f }, { 1.0f, 0.0f } },
			{ { -1.0f, 1.0f, 0.0f }, { 0.0f, 0.0f } },
		};
		data.indices = { 0, 1, 2, 0, 2, 3 };
		return data;
	}
}

//交换链重建测试夹具
class Renderer_Swapchain_Rebuild_Test : public ::testing::Test
{
};

//未初始化设置尺寸失败
TEST_F(Renderer_Swapchain_Rebuild_Test, 未初始化设置尺寸失败)
{
	engine::Renderer renderer;
	EXPECT_FALSE(renderer.window_size_set(160, 120));
}

//尺寸为零被拒且不改动当前交换链范围
TEST_F(Renderer_Swapchain_Rebuild_Test, 尺寸为零被拒)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//零尺寸被拒
	EXPECT_FALSE(renderer.window_size_set(0, 100));
	EXPECT_FALSE(renderer.window_size_set(100, 0));
	//被拒不改动当前交换链范围（未触发重建）
	EXPECT_EQ(renderer.extent_width(), INIT_WIDTH);
	EXPECT_EQ(renderer.extent_height(), INIT_HEIGHT);
}

//设置尺寸后重建在下一帧兑现（范围与回读尺寸随新尺寸）
TEST_F(Renderer_Swapchain_Rebuild_Test, 设置尺寸后下一帧兑现重建)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//设置新尺寸（重建尚未发生，范围暂不变）
	ASSERT_TRUE(renderer.window_size_set(160, 120));
	EXPECT_EQ(renderer.extent_width(), INIT_WIDTH);
	EXPECT_EQ(renderer.extent_height(), INIT_HEIGHT);
	//出帧一帧即兑现重建
	std::vector<uint8_t> pixels(static_cast<std::size_t>(160) * 120 * 4);
	uint32_t width = 0;
	uint32_t height = 0;
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size()), &width, &height));
	//交换链范围与回读尺寸均为新尺寸
	EXPECT_EQ(renderer.extent_width(), 160u);
	EXPECT_EQ(renderer.extent_height(), 120u);
	EXPECT_EQ(width, 160u);
	EXPECT_EQ(height, 120u);
	//帧计数推进
	EXPECT_EQ(renderer.frame_count(), 1u);
	//内容为清屏色（重建后画面正确）
	std::array<uint8_t, 4> center = pixel_get(pixels, width, width / 2, height / 2);
	EXPECT_TRUE(pixel_is_clear(center)) << "重建后中心非清屏色：像素 " << pixel_text(center);
}

//连续三次「改尺寸 → 重建 → 出帧」后帧计数持续推进且回读正常
TEST_F(Renderer_Swapchain_Rebuild_Test, 连续三次改尺寸重建出帧)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//三次目标尺寸（逐次不同，覆盖放大 / 缩小两向）
	const std::array<std::pair<uint32_t, uint32_t>, 3> sizes = { {
		{ 160, 120 },
		{ 200, 150 },
		{ 128, 96 },
	} };
	//期望帧计数（逐次递增）
	uint64_t expected_frames = 0;
	//逐次改尺寸 → 重建 → 出帧
	for (const std::pair<uint32_t, uint32_t>& size : sizes)
	{
		//改尺寸
		ASSERT_TRUE(renderer.window_size_set(size.first, size.second));
		//出帧（回读目标按新尺寸准备）
		std::vector<uint8_t> pixels(static_cast<std::size_t>(size.first) * size.second * 4);
		uint32_t width = 0;
		uint32_t height = 0;
		ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size()), &width, &height));
		//帧计数持续推进
		++expected_frames;
		EXPECT_EQ(renderer.frame_count(), expected_frames);
		//交换链范围与回读尺寸均为本次新尺寸
		EXPECT_EQ(renderer.extent_width(), size.first);
		EXPECT_EQ(renderer.extent_height(), size.second);
		EXPECT_EQ(width, size.first);
		EXPECT_EQ(height, size.second);
		//四角与中心均为清屏色（重建后画面一致）
		EXPECT_TRUE(pixel_is_clear(pixel_get(pixels, width, 1, 1))) << "左上非清屏色（第 " << expected_frames << " 次）";
		EXPECT_TRUE(pixel_is_clear(pixel_get(pixels, width, width - 2, height - 2))) << "右下非清屏色（第 " << expected_frames << " 次）";
		EXPECT_TRUE(pixel_is_clear(pixel_get(pixels, width, width / 2, height / 2))) << "中心非清屏色（第 " << expected_frames << " 次）";
	}
}

//重建后几何绘制仍生效（中心纹理色、角落留空）
TEST_F(Renderer_Swapchain_Rebuild_Test, 重建后几何仍生效)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//先改尺寸并出帧一次，使新尺寸交换链生效
	ASSERT_TRUE(renderer.window_size_set(160, 120));
	std::vector<uint8_t> warmup(static_cast<std::size_t>(160) * 120 * 4);
	ASSERT_TRUE(renderer.frame_render(warmup.data(), static_cast<uint32_t>(warmup.size())));
	//回落内置默认白纹理，加载覆盖视口中央的四边形并登记绘制
	renderer.texture_bind_set(0);
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.mesh_load(handle, quad_data()));
	ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
	//再次出帧并回读
	std::vector<uint8_t> pixels(static_cast<std::size_t>(160) * 120 * 4);
	uint32_t width = 0;
	uint32_t height = 0;
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size()), &width, &height));
	EXPECT_EQ(width, 160u);
	EXPECT_EQ(height, 120u);
	//中心像素为白（四通道均接近满值，与通道序无关）
	std::array<uint8_t, 4> center = pixel_get(pixels, width, width / 2, height / 2);
	EXPECT_GE(*std::min_element(center.begin(), center.end()), 250) << "中心像素非纹理色：" << pixel_text(center);
	//左上角在四边形之外，仍为清屏色
	std::array<uint8_t, 4> corner = pixel_get(pixels, width, 1, 1);
	EXPECT_TRUE(pixel_is_clear(corner)) << "角落被意外覆盖：像素 " << pixel_text(corner);
	renderer.mesh_unload(handle);
}
