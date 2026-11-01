/*
渲染后处理体积光测试（第三百八十二轮 · 方向三：屏幕空间光束）
覆盖：强度 0 原样输出（与直通逐通道差 ≤ 2）/ 强度 > 0 时天空漏光使画面整体变亮（结构性差异）。
口径：场景为「覆盖视口中央的四边形（z = 0）」＋天空；光源屏幕位置取顶部中心（uv y = 0.9），
      光束颜色取暖白，故开启后整帧平均亮度上升。通道序无关：只比较逐通道最大差与像素计数。
*/
#include "渲染后处理链测试辅助.h"

namespace
{
	//光束颜色（暖白）
	const std::array<float, 3> SHAFT_COLOR = { 1.0f, 0.92f, 0.75f };

	//渲染一帧：单级体积光链 ＋ 中央四边形（返回回读像素；params = 强度 / 衰减 / 采样缩放 / 光源 uv y）
	std::vector<uint8_t> render_shaft(const std::array<float, 4>& params)
	{
		engine::Renderer renderer;
		EXPECT_TRUE(renderer.init(hidden_config()));
		uint64_t scene_target = 0;
		EXPECT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
		uint64_t pass = post_pipeline_register(renderer, "体积光.frag.spv");
		EXPECT_NE(pass, 0u);
		EXPECT_TRUE(renderer.post_chain_set(&pass, 1));
		//通用四参入口（不做调色范围校验）
		renderer.post_stage_vec4_set(0, params[0], params[1], params[2], params[3]);
		renderer.post_stage_color_set(0, SHAFT_COLOR[0], SHAFT_COLOR[1], SHAFT_COLOR[2]);
		renderer.render_target_present_set(scene_target);
		engine::Mesh_Handle plane = 0;
		EXPECT_TRUE(renderer.mesh_load(plane, quad_data()));
		EXPECT_TRUE(renderer.mesh_draw(plane, nullptr));
		renderer.mesh_unload(plane);
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
		renderer.pipeline_unload(pass);
		renderer.render_target_destroy(scene_target);
		return pixels;
	}

	//整帧平均亮度
	float average_luma(const std::vector<uint8_t>& pixels)
	{
		double sum = 0.0;
		for (uint32_t y = 0; y < WINDOW_HEIGHT; ++y)
			for (uint32_t x = 0; x < WINDOW_WIDTH; ++x)
				sum += luma_of(pixel_get(pixels, x, y));
		return static_cast<float>(sum / static_cast<double>(WINDOW_WIDTH * WINDOW_HEIGHT));
	}

	//统计两帧之间逐通道差达到 threshold 的像素数（通道序无关）
	uint32_t differing_pixels(const std::vector<uint8_t>& left, const std::vector<uint8_t>& right, int threshold)
	{
		uint32_t count = 0;
		for (uint32_t y = 0; y < WINDOW_HEIGHT; ++y)
			for (uint32_t x = 0; x < WINDOW_WIDTH; ++x)
			{
				std::array<uint8_t, 4> a = pixel_get(left, x, y);
				std::array<uint8_t, 4> b = pixel_get(right, x, y);
				int maximum = 0;
				for (std::size_t index = 0; index < 3; ++index)
				{
					int delta = static_cast<int>(a[index]) - static_cast<int>(b[index]);
					if (delta < 0)
						delta = -delta;
					if (maximum < delta)
						maximum = delta;
				}
				if (maximum >= threshold)
					++count;
			}
		return count;
	}
}

//强度 0 时体积光阶段原样输出：与直通画面逐通道差不超过 2
TEST_F(Renderer_Post_Chain_Test, 体积光关闭原样输出)
{
	engine::Renderer probe;
	ASSERT_TRUE(probe.init(hidden_config()));
	if (!probe.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	std::vector<uint8_t> reference = render_shaft({ 0.0f, 0.0f, 0.0f, 0.0f });
	//直通链对照（同一场景与目标）
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	uint64_t scene_target = 0;
	ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
	uint64_t pass = post_pipeline_register(renderer, "后处理.frag.spv");
	ASSERT_NE(pass, 0u);
	ASSERT_TRUE(renderer.post_chain_set(&pass, 1));
	renderer.render_target_present_set(scene_target);
	engine::Mesh_Handle plane = 0;
	ASSERT_TRUE(renderer.mesh_load(plane, quad_data()));
	ASSERT_TRUE(renderer.mesh_draw(plane, nullptr));
	renderer.mesh_unload(plane);
	std::vector<uint8_t> direct(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(direct.data(), static_cast<uint32_t>(direct.size())));
	renderer.pipeline_unload(pass);
	renderer.render_target_destroy(scene_target);

	int difference = max_channel_diff(reference, direct);
	RecordProperty("shaft_disabled_max_diff", difference);
	EXPECT_LE(difference, 2) << "强度 0 未原样输出（最大差 " << difference << "）";
}

/*
强度 > 0 时天空漏光（第三百八十二轮）
口径：光源屏幕位置在顶部中心，天空（四边形以外的区域）向光源方向采样会命中天空，故整帧变亮。
断言：差异像素数 > 20 且整帧平均亮度上升 > 3。
*/
TEST_F(Renderer_Post_Chain_Test, 体积光天空漏光)
{
	engine::Renderer probe;
	ASSERT_TRUE(probe.init(hidden_config()));
	if (!probe.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	//强度 0.6、每步衰减 0.9、采样缩放 1（24 步）、光源 uv y = 0.9
	std::vector<uint8_t> reference = render_shaft({ 0.0f, 0.0f, 0.0f, 0.0f });
	std::vector<uint8_t> shafted = render_shaft({ 0.6f, 0.9f, 1.0f, 0.9f });
	uint32_t changed = differing_pixels(reference, shafted, 4);
	float luma_before = average_luma(reference);
	float luma_after = average_luma(shafted);
	RecordProperty("shaft_changed_pixels", static_cast<int>(changed));
	RecordProperty("shaft_luma_delta", static_cast<double>(luma_after - luma_before));
	std::cout << "[体积光] 差异像素=" << changed << " 亮度 " << luma_before << " → " << luma_after << std::endl;
	EXPECT_GT(changed, 20u);
	EXPECT_GT(luma_after, luma_before + 3.0f);
}
