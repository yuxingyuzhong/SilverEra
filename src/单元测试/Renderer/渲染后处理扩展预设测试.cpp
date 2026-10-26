/*
渲染层后处理扩展预设测试（第一百一十七～一百一十九轮）
覆盖：色调分离抖动产生画面差异、桶形畸变产生画面差异、双色调的暖色方向。
*/
#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <vector>

//后处理链测试辅助（窗口尺寸、像素读取、四边形与最大通道差）
#include "渲染后处理链测试辅助.h"

namespace
{
	//登记四边形场景
	void quad_register(engine::Renderer& renderer, engine::Mesh_Handle& handle_out)
	{
		ASSERT_TRUE(renderer.mesh_load(handle_out, quad_data()));
		ASSERT_TRUE(renderer.mesh_draw(handle_out, nullptr));
	}

	//直接渲染到交换链并回读
	std::vector<uint8_t> render_direct(engine::Renderer& renderer)
	{
		std::vector<uint8_t> pixels(pixel_bytes(), 0);
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
		return pixels;
	}

	//经单级后处理链渲染并回读
	std::vector<uint8_t> render_stage(engine::Renderer& renderer, const std::string& frag,
		float first, float second, float third)
	{
		uint64_t target = 0;
		EXPECT_TRUE(renderer.render_target_create(target, WINDOW_WIDTH, WINDOW_HEIGHT));
		uint64_t pipeline = post_pipeline_register(renderer, frag);
		EXPECT_NE(pipeline, 0u);
		EXPECT_TRUE(renderer.post_chain_set(&pipeline, 1));
		renderer.post_stage_param_set(0, first, second, third);
		renderer.render_target_present_set(target);
		std::vector<uint8_t> pixels(pixel_bytes(), 0);
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
		renderer.render_target_present_set(0);
		renderer.pipeline_unload(pipeline);
		renderer.render_target_destroy(target);
		return pixels;
	}
}

//扩展预设测试夹具
class Renderer_Post_Extra_Test : public ::testing::Test
{
};

//色调分离抖动：量化 ＋ 抖动应与原画面明显不同
TEST_F(Renderer_Post_Extra_Test, 色调分离抖动)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	uint64_t texture = 0;
	ASSERT_TRUE(renderer.texture_load_checkerboard(texture, 32, 32, 4));
	renderer.texture_bind_set(texture);
	engine::Mesh_Handle quad = 0;
	quad_register(renderer, quad);
	std::vector<uint8_t> reference = render_direct(renderer);

	//3 级量化 ＋ 完全混入
	quad_register(renderer, quad);
	std::vector<uint8_t> posterized = render_stage(renderer, "色调分离抖动.frag.spv", 3.0f, 0.0f, 1.0f);
	EXPECT_GT(max_channel_diff(reference, posterized), 5) << "色调分离未产生可见变化";

	renderer.mesh_unload(quad);
	renderer.texture_unload(texture);
}

//桶形畸变：径向缩放应改变边缘区域采样
TEST_F(Renderer_Post_Extra_Test, 桶形畸变)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	uint64_t texture = 0;
	ASSERT_TRUE(renderer.texture_load_checkerboard(texture, 32, 32, 4));
	renderer.texture_bind_set(texture);
	engine::Mesh_Handle quad = 0;
	quad_register(renderer, quad);
	std::vector<uint8_t> reference = render_direct(renderer);

	//强畸变（阈值 0.8 → 强度 0.24）
	quad_register(renderer, quad);
	std::vector<uint8_t> distorted = render_stage(renderer, "桶形畸变.frag.spv", 0.8f, 0.0f, 1.0f);
	EXPECT_GT(max_channel_diff(reference, distorted), 10) << "桶形畸变未产生可见变化";

	renderer.mesh_unload(quad);
	renderer.texture_unload(texture);
}

//双色调：中灰画面映射到暖色（红多于蓝）
TEST_F(Renderer_Post_Extra_Test, 双色调暖色方向)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	const uint8_t gray[4] = { 200, 200, 200, 255 };
	uint64_t texture = 0;
	ASSERT_TRUE(renderer.texture_load(texture, gray, 1, 1));
	renderer.texture_bind_set(texture);
	engine::Mesh_Handle quad = 0;

	//对比度 1、完全混入
	quad_register(renderer, quad);
	std::vector<uint8_t> mapped = render_stage(renderer, "双色调.frag.spv", 1.0f, 0.0f, 1.0f);
	std::array<uint8_t, 4> pixel = pixel_get(mapped, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	//交换链为 BGRA：下标 2 红、下标 0 蓝
	EXPECT_GT(static_cast<int>(pixel[2]) - static_cast<int>(pixel[0]), 20) << "双色调亮部应偏暖";

	renderer.mesh_unload(quad);
	renderer.texture_unload(texture);
}

/*
FXAA 平滑斜边（第一百四十一轮）
45° 旋转的白色四边形在暗背景上形成硬斜边：FXAA 后「中间亮度像素数」应增加，
低对比区域（例如四边形内部）保持原样。
*/
TEST_F(Renderer_Post_Extra_Test, FXAA平滑斜边)
{
	//统计中间亮度像素（通道均值落在 100..230，避开清屏色与纯白）
	auto mid_count = [](const std::vector<uint8_t>& pixels)
	{
		uint32_t count = 0;
		for (std::size_t offset = 0; offset + 3 < pixels.size(); offset += 4)
		{
			uint32_t average = (pixels[offset] + pixels[offset + 1] + pixels[offset + 2]) / 3;
			if (average >= 100 && average <= 230)
				++count;
		}
		return count;
	};

	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	engine::Mesh_Handle quad = 0;
	ASSERT_TRUE(renderer.mesh_load(quad, quad_data()));
	//绕 Z 轴 45°（列主序）
	const float cosine = 0.70710678f;
	const float sine = 0.70710678f;
	float transform[16] = {};
	transform[0] = cosine;
	transform[1] = sine;
	transform[4] = -sine;
	transform[5] = cosine;
	transform[10] = 1.0f;
	transform[15] = 1.0f;

	//参考帧（直通）
	ASSERT_TRUE(renderer.mesh_draw(quad, transform));
	std::vector<uint8_t> reference = render_direct(renderer);
	const uint32_t plain_mid = mid_count(reference);

	//单级 FXAA
	ASSERT_TRUE(renderer.mesh_draw(quad, transform));
	std::vector<uint8_t> smoothed = render_stage(renderer, "FXAA.frag.spv", 0.0f, 0.0f, 0.0f);
	const uint32_t fxaa_mid = mid_count(smoothed);

	RecordProperty("fxaa_plain_mid", static_cast<int>(plain_mid));
	RecordProperty("fxaa_smoothed_mid", static_cast<int>(fxaa_mid));
	EXPECT_GT(fxaa_mid, plain_mid) << "FXAA 未产生中间亮度过渡";
	EXPECT_GT(max_channel_diff(reference, smoothed), 5) << "FXAA 未改变画面";

	renderer.mesh_unload(quad);
}
