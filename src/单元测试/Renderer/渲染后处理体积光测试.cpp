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

/*
体积雾与体积光组合（第三百八十三轮）
口径：两级链「体积雾 → 体积光」，靶为两块 64×36 中间目标。
1. 雾 0 ＋ 光 0 时两级均恒等，整链与直通差 ≤ 4；
2. 雾 0.2 ＋ 光 0.6 相对前者差异像素 > 20、整帧亮度上升，且 post_stages = 2。
*/
TEST_F(Renderer_Post_Chain_Test, 体积雾与体积光组合)
{
	engine::Renderer probe;
	ASSERT_TRUE(probe.init(hidden_config()));
	if (!probe.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	//两级链渲染（fog / shaft 为各自强度；任何一者为 0 都保持该级恒等）
	auto render_pair = [&](float fog, float shaft, uint32_t* stages_out) -> std::vector<uint8_t>
	{
		engine::Renderer renderer;
		EXPECT_TRUE(renderer.init(hidden_config()));
		uint64_t target0 = 0;
		uint64_t target1 = 0;
		EXPECT_TRUE(renderer.render_target_create(target0, renderer.extent_width(), renderer.extent_height()));
		EXPECT_TRUE(renderer.render_target_create(target1, renderer.extent_width(), renderer.extent_height()));
		uint64_t fog_pass = post_pipeline_register(renderer, "体积雾.frag.spv");
		uint64_t shaft_pass = post_pipeline_register(renderer, "体积光.frag.spv");
		EXPECT_NE(fog_pass, 0u);
		EXPECT_NE(shaft_pass, 0u);
		uint64_t chain[2] = { fog_pass, shaft_pass };
		EXPECT_TRUE(renderer.post_chain_set(chain, 2));
		renderer.post_stage_vec4_set(0, fog, 0.0f, 20.0f, 1.0f);
		renderer.post_stage_color4_set(0, SHAFT_COLOR[0] * 0.6f, SHAFT_COLOR[1] * 0.7f, SHAFT_COLOR[2] * 0.9f, 0.0f);
		renderer.post_stage_vec4_set(1, shaft, 0.9f, 1.0f, 0.9f);
		renderer.post_stage_color_set(1, SHAFT_COLOR[0], SHAFT_COLOR[1], SHAFT_COLOR[2]);
		uint64_t targets[2] = { target0, target1 };
		EXPECT_TRUE(renderer.render_target_present_chain_set(targets, 2));
		engine::Mesh_Handle plane = 0;
		EXPECT_TRUE(renderer.mesh_load(plane, quad_data()));
		EXPECT_TRUE(renderer.mesh_draw(plane, nullptr));
		renderer.mesh_unload(plane);
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
		if (stages_out != nullptr)
			*stages_out = renderer.frame_stats_get().post_stages;
		renderer.pipeline_unload(fog_pass);
		renderer.pipeline_unload(shaft_pass);
		renderer.render_target_destroy(target1);
		renderer.render_target_destroy(target0);
		return pixels;
	};
	uint32_t stages = 0;
	std::vector<uint8_t> off = render_pair(0.0f, 0.0f, nullptr);
	std::vector<uint8_t> on = render_pair(0.2f, 0.6f, &stages);
	//关闭态：与直通链（同为两级但首级直通）作对照的差值由「两级编码取整」上界约束
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	uint64_t target0 = 0;
	uint64_t target1 = 0;
	ASSERT_TRUE(renderer.render_target_create(target0, renderer.extent_width(), renderer.extent_height()));
	ASSERT_TRUE(renderer.render_target_create(target1, renderer.extent_width(), renderer.extent_height()));
	uint64_t passthrough = post_pipeline_register(renderer, "后处理.frag.spv");
	uint64_t shaft_pass = post_pipeline_register(renderer, "体积光.frag.spv");
	ASSERT_NE(passthrough, 0u);
	ASSERT_NE(shaft_pass, 0u);
	uint64_t chain[2] = { passthrough, shaft_pass };
	ASSERT_TRUE(renderer.post_chain_set(chain, 2));
	renderer.post_stage_vec4_set(1, 0.0f, 0.9f, 1.0f, 0.9f);
	uint64_t targets[2] = { target0, target1 };
	ASSERT_TRUE(renderer.render_target_present_chain_set(targets, 2));
	engine::Mesh_Handle plane = 0;
	ASSERT_TRUE(renderer.mesh_load(plane, quad_data()));
	ASSERT_TRUE(renderer.mesh_draw(plane, nullptr));
	renderer.mesh_unload(plane);
	std::vector<uint8_t> direct(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(direct.data(), static_cast<uint32_t>(direct.size())));
	renderer.pipeline_unload(passthrough);
	renderer.pipeline_unload(shaft_pass);
	renderer.render_target_destroy(target1);
	renderer.render_target_destroy(target0);

	int identity_diff = max_channel_diff(direct, off);
	RecordProperty("fog_shaft_identity_diff", identity_diff);
	EXPECT_LE(identity_diff, CHAIN_TOLERANCE) << "两级关闭态未恒等（最大差 " << identity_diff << "）";
	uint32_t changed = differing_pixels(off, on, 8);
	float luma_off = average_luma(off);
	float luma_on = average_luma(on);
	RecordProperty("fog_shaft_changed_pixels", static_cast<int>(changed));
	std::cout << "[雾＋体积光] 差异像素=" << changed << " 亮度 " << luma_off << " → " << luma_on
		<< " 级数=" << stages << std::endl;
	EXPECT_EQ(stages, 2u);
	EXPECT_GT(changed, 20u) << "雾与体积光组合未生效";
	EXPECT_GT(luma_on, luma_off + 2.0f);
}
