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

/*
体积光采样开销（第三百八十五轮）
同一场景（覆盖视口的四边形 ＋ 天空），单级体积光链；分辨率与采样步数三档对照，
记录稳定段（第 3..8 帧）的主机录制与 GPU 帧均值；不设硬上限，只输出与粗检。
*/
TEST_F(Renderer_Post_Chain_Test, 体积光采样开销)
{
	engine::Renderer probe;
	ASSERT_TRUE(probe.init(hidden_config()));
	if (!probe.gpu_timestamp_supported())
		GTEST_SKIP() << "本机不支持 GPU 时间戳";

	//单次测量（分辨率 / 采样缩放 / 分辨率标称进 / 录制与 GPU 帧出）
	auto measure = [&](uint32_t width, uint32_t height, float sample_scale,
		double& record_out, double& gpu_out) -> void
	{
		record_out = 0.0;
		gpu_out = 0.0;
		engine::Renderer_Config config;
		config.width = width;
		config.height = height;
		config.hidden = true;
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(config));
		uint64_t scene_target = 0;
		ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
		uint64_t pass = post_pipeline_register(renderer, "体积光.frag.spv");
		ASSERT_NE(pass, 0u);
		ASSERT_TRUE(renderer.post_chain_set(&pass, 1));
		renderer.post_stage_vec4_set(0, 0.6f, 0.9f, sample_scale, 0.9f);
		renderer.post_stage_color_set(0, SHAFT_COLOR[0], SHAFT_COLOR[1], SHAFT_COLOR[2]);
		renderer.render_target_present_set(scene_target);
		engine::Mesh_Handle plane = 0;
		ASSERT_TRUE(renderer.mesh_load(plane, quad_data()));
		//八帧：前两帧预热，取第 3..8 帧均值
		double record_sum = 0.0;
		double gpu_sum = 0.0;
		uint32_t samples = 0;
		for (uint32_t frame = 0; frame < 8; ++frame)
		{
			ASSERT_TRUE(renderer.mesh_draw(plane, nullptr));
			ASSERT_TRUE(renderer.frame_render());
			if (frame >= 2)
			{
				const engine::Frame_Stats stats = renderer.frame_stats_get();
				record_sum += stats.host_record_ms;
				gpu_sum += stats.gpu_frame_ms;
				++samples;
			}
		}
		record_out = record_sum / samples;
		gpu_out = gpu_sum / samples;
		renderer.mesh_unload(plane);
		renderer.pipeline_unload(pass);
		renderer.render_target_destroy(scene_target);
	};
	double small_12_record = 0.0, small_12_gpu = 0.0;
	double full_12_record = 0.0, full_12_gpu = 0.0;
	double full_24_record = 0.0, full_24_gpu = 0.0;
	double full_48_record = 0.0, full_48_gpu = 0.0;
	measure(64, 36, 0.25f, small_12_record, small_12_gpu);
	//采样缩放 0.25 / 0.5 / 1.0 → 12 / 24 / 48 步（上限 48）
	measure(1280, 720, 0.25f, full_12_record, full_12_gpu);
	measure(1280, 720, 0.5f, full_24_record, full_24_gpu);
	measure(1280, 720, 1.0f, full_48_record, full_48_gpu);
	std::cout << "[体积光开销] 64×36 12步 录制=" << small_12_record << " GPU帧=" << small_12_gpu
		<< " ms；1280×720 12步 录制=" << full_12_record << " GPU帧=" << full_12_gpu
		<< " ms；24步 录制=" << full_24_record << " GPU帧=" << full_24_gpu
		<< " ms；48步 录制=" << full_48_record << " GPU帧=" << full_48_gpu << " ms" << std::endl;
	//粗检：主机录制非零；GPU 帧耗时随采样数方向合理（24 步 ≥ 12 步的一半）
	EXPECT_GT(small_12_record, 0.0);
	EXPECT_GT(full_12_record, 0.0);
	EXPECT_GT(full_24_gpu, full_12_gpu * 0.5);
	EXPECT_GT(full_48_gpu, full_24_gpu * 0.5);
}

/*
体积光光源位置可配（第三百九十轮）
颜色第四分量作为光源屏幕 uv 的 x（≤ 0 回落 0.5）；同一强度下光源左右移动应产生不同光束（差异像素 > 20）。
*/
TEST_F(Renderer_Post_Chain_Test, 体积光光源位置可配)
{
	engine::Renderer probe;
	ASSERT_TRUE(probe.init(hidden_config()));
	if (!probe.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	//光源 x 进 / 像素出（其余参数固定：强度 0.6、衰减 0.9、24 步、光源 y 0.9）
	auto render_light_x = [&](float light_x) -> std::vector<uint8_t>
	{
		engine::Renderer renderer;
		EXPECT_TRUE(renderer.init(hidden_config()));
		uint64_t scene_target = 0;
		EXPECT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
		uint64_t pass = post_pipeline_register(renderer, "体积光.frag.spv");
		EXPECT_NE(pass, 0u);
		EXPECT_TRUE(renderer.post_chain_set(&pass, 1));
		renderer.post_stage_vec4_set(0, 0.6f, 0.9f, 0.5f, 0.9f);
		renderer.post_stage_color4_set(0, SHAFT_COLOR[0], SHAFT_COLOR[1], SHAFT_COLOR[2], light_x);
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
	};
	std::vector<uint8_t> left = render_light_x(0.2f);
	std::vector<uint8_t> right = render_light_x(0.8f);
	uint32_t changed = differing_pixels(left, right, 4);
	RecordProperty("shaft_light_x_changed_pixels", static_cast<int>(changed));
	std::cout << "[体积光光源] 左 vs 右 差异像素=" << changed << std::endl;
	EXPECT_GT(changed, 20u) << "光源屏幕 x 未生效（左右两帧几乎一致）";
}

namespace
{
	/*
	渲染一帧（阴影版）：单级体积光链 ＋ 「地面四边形 ＋ 上方挡板」场景 ＋ 阴影图。
	on = 是否写 post_extra.x（阴影开关）；strength = 光照明影强度（0 = 阴影无效，用于对照）。
	*/
	std::vector<uint8_t> render_shaft_shadow(bool on, float strength)
	{
		engine::Renderer renderer;
		EXPECT_TRUE(renderer.init(hidden_config()));
		uint64_t scene_target = 0;
		EXPECT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
		uint64_t pass = post_pipeline_register(renderer, "体积光.frag.spv");
		EXPECT_NE(pass, 0u);
		EXPECT_TRUE(renderer.post_chain_set(&pass, 1));
		renderer.post_stage_vec4_set(0, 0.6f, 0.9f, 1.0f, 0.9f);
		renderer.post_stage_color_set(0, SHAFT_COLOR[0], SHAFT_COLOR[1], SHAFT_COLOR[2]);
		//第四百零二轮：逐级 extra（x = 阴影开关）
		renderer.post_stage_extra_set(0, on ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f);
		renderer.render_target_present_set(scene_target);

		//场景：相机在 +Z 3 处看向原点；方向光从左上前方斜射；开启阴影
		engine::Camera_Data camera{};
		camera.position[2] = 3.0f;
		camera.target[1] = 0.0f;
		camera.fov = 1.2f;
		camera.far_plane = 40.0f;
		engine::Light_Data light{};
		light.direction[0] = 0.45f;
		light.direction[1] = -0.85f;
		light.direction[2] = -0.28f;
		light.intensity = 1.0f;
		light.ambient = 0.05f;
		light.shadow_enabled = 0;
		light.shadow_strength = strength;
		engine::Fog_Data fog{};
		renderer.scene_apply(camera, light, fog);
		renderer.shadow_enable_set(true);
		renderer.shadow_range_set(3.0f, 1.0f, 12.0f);
		renderer.shadow_cascade_set(1, 3.0f, 8.0f);

		//地面四边形（受光面）
		engine::Mesh_Handle plane = 0;
		EXPECT_TRUE(renderer.mesh_load(plane, quad_data()));
		EXPECT_TRUE(renderer.mesh_draw(plane, nullptr));
		//上方挡板（平移 ＋ 非等比缩放的列主序矩阵）
		const float blocker[16] = {
			0.62f, 0.0f, 0.0f, 0.0f,
			0.0f, 0.42f, 0.0f, 0.0f,
			0.0f, 0.0f, 0.02f, 0.0f,
			-0.35f, 0.85f, -0.5f, 1.0f,
		};
		EXPECT_TRUE(renderer.mesh_draw(plane, blocker));
		renderer.mesh_unload(plane);

		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
		renderer.pipeline_unload(pass);
		renderer.render_target_destroy(scene_target);
		return pixels;
	}
}

/*
体积光接阴影图（第四百零二轮）
口径：同一「地面 ＋ 挡板」场景下，仅改 post_extra.x 与阴影强度。
1. 阴影强度 1：开 / 关阴影开关画面差异像素 > 20（几何采样点的受光判定与截断生效）；
2. 阴影强度 0（lit 恒为 1）与强度 1 相比，整帧平均亮度下降 > 1（被挡板遮蔽的光路更暗）。
*/
TEST_F(Renderer_Post_Chain_Test, 体积光接阴影图)
{
	engine::Renderer probe;
	ASSERT_TRUE(probe.init(hidden_config()));
	if (!probe.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	std::vector<uint8_t> off = render_shaft_shadow(false, 1.0f);
	std::vector<uint8_t> on = render_shaft_shadow(true, 1.0f);
	uint32_t changed = differing_pixels(off, on, 4);
	EXPECT_GT(changed, 20u) << "阴影开关未改变画面";

	//强度 0：lit 混合为 1，等同于「阴影无效」对照
	std::vector<uint8_t> no_shadow = render_shaft_shadow(true, 0.0f);
	float luma_full = average_luma(on);
	float luma_none = average_luma(no_shadow);
	RecordProperty("shaft_shadow_changed_pixels", static_cast<int>(changed));
	RecordProperty("shaft_shadow_luma_full", static_cast<double>(luma_full));
	RecordProperty("shaft_shadow_luma_none", static_cast<double>(luma_none));
	std::cout << "[体积光·阴影] 差异像素=" << changed << " 亮度（强度1）" << luma_full
		<< " / （强度0）" << luma_none << std::endl;
	EXPECT_LT(luma_full, luma_none - 1.0f) << "阴影强度未使光路变暗";
}
