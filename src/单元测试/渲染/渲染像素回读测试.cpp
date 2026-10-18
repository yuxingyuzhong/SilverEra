/*
渲染像素回读测试（L2 隐藏窗口）
覆盖：未初始化守卫、容量不足被拒且不推进帧、清屏色回读、白纹理四边形局部覆盖、
      棋盘格纹理回读（多色调），以及回读尺寸与能力查询的语义。
说明：断言一律按「通道序无关 ＋ 容差」编写，不依赖具体表面格式（B8G8R8A8 / R8G8B8A8、
      以及 sRGB 与非 sRGB 附件的取值差异均可通过）；不涉及 Vulkan 细节。
*/
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

//获取渲染器门面与网格数据
#include "Application/Renderer/src/render/Renderer/渲染器.h"

namespace
{
	//回读窗口尺寸（小尺寸便于逐像素遍历）
	constexpr uint32_t READBACK_WIDTH = 64;
	constexpr uint32_t READBACK_HEIGHT = 36;
	/*
	清屏色候选（渲染器清屏色为 0.05 / 0.05 / 0.10 / 1.0）
	同一清屏色在「非 sRGB（UNORM）」与「sRGB」附件下编码不同：
	UNORM ≈ 13 / 13 / 26；sRGB ≈ 63 / 63 / 89（线性值经 sRGB 传输函数编码后）。
	两者排序后均为「两低一高 ＋ 不透明」，故本用例的断言与表面格式无关。
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
		config.width = READBACK_WIDTH;
		config.height = READBACK_HEIGHT;
		config.hidden = true;
		return config;
	}

	//本帧像素字节数（RGBA8 紧排）
	uint32_t pixel_bytes(void)
	{
		return READBACK_WIDTH * READBACK_HEIGHT * 4;
	}

	//取指定像素的四个字节
	std::array<uint8_t, 4> pixel_get(const std::vector<uint8_t>& pixels, uint32_t x, uint32_t y)
	{
		//行内偏移（每行宽 × 4 字节）
		std::size_t offset = (static_cast<std::size_t>(y) * READBACK_WIDTH + x) * 4;
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

	//统计回读结果中的不同颜色数（按四通道去重）
	uint32_t distinct_color_count(const std::vector<uint8_t>& pixels)
	{
		//收集到的颜色（每色四字节）
		std::vector<std::array<uint8_t, 4>> colors;
		//逐像素收集并去重
		for (uint32_t y = 0; y < READBACK_HEIGHT; ++y)
			for (uint32_t x = 0; x < READBACK_WIDTH; ++x)
			{
				//当前像素
				std::array<uint8_t, 4> value = pixel_get(pixels, x, y);
				//若尚未收集该颜色
				if (std::find(colors.begin(), colors.end(), value) == colors.end())
					colors.push_back(value);
			}
		return static_cast<uint32_t>(colors.size());
	}
}

//像素回读测试夹具
class Renderer_Readback_Test : public ::testing::Test
{
};

//未初始化时回读失败，能力查询亦为假
TEST_F(Renderer_Readback_Test, 未初始化回读失败)
{
	engine::Renderer renderer;
	std::vector<uint8_t> pixels(pixel_bytes());
	uint32_t width = 0;
	uint32_t height = 0;
	//未初始化即扩展回读参数，应失败
	EXPECT_FALSE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size()), &width, &height));
	//能力查询为假
	EXPECT_FALSE(renderer.readback_supported());
}

//回读目标容量不足时被拒，且本帧不推进
TEST_F(Renderer_Readback_Test, 容量不足被拒且不推进帧)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//若本机交换链不支持传输源用法则跳过（能力查询已在用例句内断言）
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//远小于所需字节数的目标缓冲
	std::vector<uint8_t> pixels(16);
	uint32_t width = 0;
	uint32_t height = 0;
	EXPECT_FALSE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size()), &width, &height));
	//被拒的帧不推进帧计数（帧未渲染）
	EXPECT_EQ(renderer.frame_count(), 0u);
	//尺寸出参保持为零
	EXPECT_EQ(width, 0u);
	EXPECT_EQ(height, 0u);
}

//无障碍缓存回读清屏色，尺寸回填正确
TEST_F(Renderer_Readback_Test, 回读清屏色正确)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	std::vector<uint8_t> pixels(pixel_bytes());
	uint32_t width = 0;
	uint32_t height = 0;
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size()), &width, &height));
	//尺寸回填为窗口尺寸
	EXPECT_EQ(width, READBACK_WIDTH);
	EXPECT_EQ(height, READBACK_HEIGHT);
	//采样点：左上 / 右下 / 中心
	std::array<uint8_t, 4> top_left = pixel_get(pixels, 1, 1);
	std::array<uint8_t, 4> bottom_right = pixel_get(pixels, READBACK_WIDTH - 2, READBACK_HEIGHT - 2);
	std::array<uint8_t, 4> center = pixel_get(pixels, READBACK_WIDTH / 2, READBACK_HEIGHT / 2);
	//三点均为清屏色
	EXPECT_TRUE(pixel_is_clear(top_left)) << "左上角非清屏色：像素 " << pixel_text(top_left);
	EXPECT_TRUE(pixel_is_clear(bottom_right)) << "右下角非清屏色：像素 " << pixel_text(bottom_right);
	EXPECT_TRUE(pixel_is_clear(center)) << "中心非清屏色：像素 " << pixel_text(center);
	//整帧颜色一致（清屏为纯填充，无残留内容）
	EXPECT_TRUE(top_left == center && center == bottom_right) << "整帧颜色不一致";
}

//白纹理四边形：中心为纹理色，角落仍为清屏色（几何与投影真实生效）
TEST_F(Renderer_Readback_Test, 白纹理四边形中心覆盖角落留空)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//回落内置默认白纹理
	renderer.texture_bind_set(0);
	//加载覆盖视口中央的四边形并登记绘制
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.mesh_load(handle, quad_data()));
	ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	//中心像素为白（四通道均接近满值，与通道序无关）
	std::array<uint8_t, 4> center = pixel_get(pixels, READBACK_WIDTH / 2, READBACK_HEIGHT / 2);
	EXPECT_GE(*std::min_element(center.begin(), center.end()), 250) << "中心像素非纹理色：" << pixel_text(center);
	//左上角在四边形之外，仍为清屏色
	std::array<uint8_t, 4> corner = pixel_get(pixels, 1, 1);
	EXPECT_TRUE(pixel_is_clear(corner)) << "角落被意外覆盖：像素 " << pixel_text(corner);
	renderer.mesh_unload(handle);
}

//棋盘格纹理回读出现多色调：证纹理采样与网格绘制真实发生
TEST_F(Renderer_Readback_Test, 棋盘格纹理回读呈现多色调)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//程序生成棋盘格并绑定
	uint64_t texture = 0;
	ASSERT_TRUE(renderer.texture_load_checkerboard(texture, 64, 64, 8));
	renderer.texture_bind_set(texture);
	//加载四边形并登记绘制
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.mesh_load(handle, quad_data()));
	ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	/*
	颜色计数
	清屏色 ＋ 棋盘格深 / 浅两色 ⇒ 至少 3 种不同颜色（深色在非 sRGB 附件下可能仅与清屏色差一个通道，
	故用「去重计数」而非「与清屏色颜色值比对」来判定）。
	*/
	EXPECT_GE(distinct_color_count(pixels), 3u) << "未见多色调，纹理或绘制未生效";
	//存在明显亮于清屏色的像素（纹理浅色落地）
	bool bright_found = false;
	for (uint32_t y = 0; y < READBACK_HEIGHT && !bright_found; ++y)
		for (uint32_t x = 0; x < READBACK_WIDTH && !bright_found; ++x)
		{
			//当前像素
			std::array<uint8_t, 4> value = pixel_get(pixels, x, y);
			//四通道最小值即该像素最暗通道，明显高于清屏色的高通道即视为纹理亮色
			if (*std::min_element(value.begin(), value.end()) >= 60)
				bright_found = true;
		}
	EXPECT_TRUE(bright_found) << "未出现亮于清屏色的纹理色像素";
	renderer.mesh_unload(handle);
	renderer.texture_unload(texture);
}
