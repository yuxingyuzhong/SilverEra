/*
渲染同帧多 pass 后处理测试（L2 隐藏窗口）
覆盖：未初始化守卫、无效句柄回落、单帧内完成「渲染到目标 → 全屏后处理 → 呈现到交换链」，
      并以「与同场景直接走交换链路径的回读画面一致」作为后处理正确性的强断言。
说明：
  1. 断言一律按「通道序无关 ＋ 容差」编写，不依赖具体表面格式；不涉及 Vulkan 细节。
  2. Debug 构建下渲染器默认开启校验层，本路径较新，若屏障 / 描述符集有误会由校验层报出，
     用例随渲染流程一并实跑判定。
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
	//窗口尺寸（小尺寸便于逐像素遍历；目标与交换链同尺寸以满足后处理 1:1 取源）
	constexpr uint32_t WINDOW_WIDTH = 64;
	constexpr uint32_t WINDOW_HEIGHT = 36;

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
	//两路径画面逐通道最大允许差（后处理为 1:1 取回，仅容取整误差）
	constexpr int POST_TOLERANCE = 2;

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
		config.width = WINDOW_WIDTH;
		config.height = WINDOW_HEIGHT;
		config.hidden = true;
		return config;
	}

	//本帧像素字节数（RGBA8 紧排）
	uint32_t pixel_bytes(void)
	{
		return WINDOW_WIDTH * WINDOW_HEIGHT * 4;
	}

	//取指定像素的四个字节
	std::array<uint8_t, 4> pixel_get(const std::vector<uint8_t>& pixels, uint32_t x, uint32_t y)
	{
		std::size_t offset = (static_cast<std::size_t>(y) * WINDOW_WIDTH + x) * 4;
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

	//判断像素是否近似等于清屏色（排序后与两种编码候选逐一比对）
	bool pixel_is_clear(std::array<uint8_t, 4> value)
	{
		std::array<uint8_t, 4> sorted = pixel_sorted(value);
		//末位须为不透明
		if (sorted[3] < 250)
			return false;
		//逐候选比对前三个通道
		for (const std::array<uint8_t, 3>& candidate : CLEAR_CANDIDATES)
		{
			bool matched = true;
			for (std::size_t index = 0; index < 3; ++index)
			{
				int delta = static_cast<int>(sorted[index]) - static_cast<int>(candidate[index]);
				if (delta < -CLEAR_TOLERANCE || delta > CLEAR_TOLERANCE)
					matched = false;
			}
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

	//登记「白纹理四边形」场景（供两条路径复用，保证输入一致）
	void scene_register(engine::Renderer& renderer, engine::Mesh_Handle& handle_out)
	{
		//回落内置默认白纹理
		renderer.texture_bind_set(0);
		//加载四边形并登记绘制
		ASSERT_TRUE(renderer.mesh_load(handle_out, quad_data()));
		ASSERT_TRUE(renderer.mesh_draw(handle_out, nullptr));
	}

	//两帧回读画面的最大逐通道差
	int max_channel_diff(const std::vector<uint8_t>& left, const std::vector<uint8_t>& right)
	{
		int maximum = 0;
		for (std::size_t index = 0; index < left.size(); ++index)
		{
			int delta = static_cast<int>(left[index]) - static_cast<int>(right[index]);
			if (delta < 0)
				delta = -delta;
			if (delta > maximum)
				maximum = delta;
		}
		return maximum;
	}
}

//同帧后处理测试夹具
class Renderer_Post_Process_Test : public ::testing::Test
{
};

//未初始化设定后处理呈现目标无副作用
TEST_F(Renderer_Post_Process_Test, 未初始化设置无副作用)
{
	engine::Renderer renderer;
	//未初始化时调用应静默返回（不崩溃）
	renderer.render_target_present_set(1);
	EXPECT_FALSE(renderer.valid());
}

//无效句柄被回落（关闭该路径，仍可正常出帧）
TEST_F(Renderer_Post_Process_Test, 无效句柄回落)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//设定一个未登记的目标句柄（应关闭该路径）
	renderer.render_target_present_set(999);
	std::vector<uint8_t> pixels(pixel_bytes());
	uint32_t width = 0;
	uint32_t height = 0;
	//仍应走默认交换链路径并成功出帧
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size()), &width, &height));
	EXPECT_EQ(width, WINDOW_WIDTH);
	EXPECT_EQ(height, WINDOW_HEIGHT);
	EXPECT_EQ(renderer.frame_count(), 1u);
}

//单帧内完成「渲染到目标 → 全屏后处理 → 呈现到交换链」，且画面与目标内容一致
TEST_F(Renderer_Post_Process_Test, 同帧后处理呈现且画面正确)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//创建与交换链同尺寸的渲染目标
	uint64_t target = 0;
	ASSERT_TRUE(renderer.render_target_create(target, renderer.extent_width(), renderer.extent_height()));
	ASSERT_NE(target, 0u);
	//登记场景并设定「同帧后处理呈现」
	engine::Mesh_Handle handle = 0;
	scene_register(renderer, handle);
	renderer.render_target_present_set(target);
	std::vector<uint8_t> pixels(pixel_bytes());
	uint32_t width = 0;
	uint32_t height = 0;
	//一帧内完成全部（帧计数仅推进 1）
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size()), &width, &height));
	EXPECT_EQ(renderer.frame_count(), 1u);
	EXPECT_EQ(width, WINDOW_WIDTH);
	EXPECT_EQ(height, WINDOW_HEIGHT);
	//中心为白色（证明后处理确实取回了目标中绘制出的四边形，而非仅清屏）
	std::array<uint8_t, 4> center = pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	EXPECT_GE(*std::min_element(center.begin(), center.end()), 250) << "中心像素非纹理色：" << pixel_text(center);
	//左上角在四边形之外，仍为清屏色
	std::array<uint8_t, 4> corner = pixel_get(pixels, 1, 1);
	EXPECT_TRUE(pixel_is_clear(corner)) << "角落被意外覆盖：像素 " << pixel_text(corner);
	renderer.mesh_unload(handle);
	renderer.render_target_destroy(target);
}

//同场景下「同帧后处理呈现」与「直接走交换链路径」回读画面一致（后处理为 1:1 取回）
TEST_F(Renderer_Post_Process_Test, 与交换链路径画面一致)
{
	//参考帧：直接走交换链路径
	std::vector<uint8_t> reference(pixel_bytes());
	{
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(hidden_config()));
		if (!renderer.readback_supported())
			GTEST_SKIP() << "本机交换链图像不具备传输源用法";
		engine::Mesh_Handle handle = 0;
		scene_register(renderer, handle);
		ASSERT_TRUE(renderer.frame_render(reference.data(), static_cast<uint32_t>(reference.size())));
		renderer.mesh_unload(handle);
	}
	//对照帧：同帧多 pass 后处理路径
	std::vector<uint8_t> processed(pixel_bytes());
	{
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(hidden_config()));
		if (!renderer.readback_supported())
			GTEST_SKIP() << "本机交换链图像不具备传输源用法";
		uint64_t target = 0;
		ASSERT_TRUE(renderer.render_target_create(target, renderer.extent_width(), renderer.extent_height()));
		engine::Mesh_Handle handle = 0;
		scene_register(renderer, handle);
		renderer.render_target_present_set(target);
		ASSERT_TRUE(renderer.frame_render(processed.data(), static_cast<uint32_t>(processed.size())));
		renderer.mesh_unload(handle);
		renderer.render_target_destroy(target);
	}
	//两路径画面逐通道一致（仅容取整误差）
	int difference = max_channel_diff(reference, processed);
	EXPECT_LE(difference, POST_TOLERANCE) << "同帧后处理画面与交换链路径画面不一致（最大通道差 " << difference << "）";
}
