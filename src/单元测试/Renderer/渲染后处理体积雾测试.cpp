/*
渲染后处理体积雾测试（第三百七十三轮 · 方向三：屏幕空间体积雾）
覆盖：密度 0 原样输出（与直通逐通道差 ≤ 2）/ 密度 > 0 时远景与天空被雾色覆盖（结构性差异）/ 高度衰减参数生效。
口径：场景为「覆盖视口中央的四边形（z = 0，距相机 3）」＋天空（深度 1）；
      雾色取亮蓝（约 0.6 / 0.7 / 0.9），清屏色为深蓝，故开启后整体亮度上升、差异像素数显著。
      通道序无关：只比较逐通道最大差与像素计数，不假定 R/G/B 顺序。
*/
#include "渲染后处理链测试辅助.h"

namespace
{
	//雾色（亮蓝；开启后整体亮度上升）
	const std::array<float, 3> FOG_COLOR = { 0.6f, 0.7f, 0.9f };

	//渲染一帧：单级体积雾链 ＋ 中央四边形（返回回读像素；noise 为密度噪声强度 0..1）
	std::vector<uint8_t> render_fog(const std::array<float, 4>& params, float noise = 0.0f)
	{
		engine::Renderer renderer;
		EXPECT_TRUE(renderer.init(hidden_config()));
		uint64_t scene_target = 0;
		EXPECT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
		uint64_t pass = post_pipeline_register(renderer, "体积雾.frag.spv");
		EXPECT_NE(pass, 0u);
		//链先于 present_set 设置（与既有反射用例同序）
		EXPECT_TRUE(renderer.post_chain_set(&pass, 1));
		//体积雾参数走通用四参入口（第三百七十三轮；密度 / 高度衰减 / 最大距离 / 步数缩放）
		renderer.post_stage_vec4_set(0, params[0], params[1], params[2], params[3]);
		renderer.post_stage_color4_set(0, FOG_COLOR[0], FOG_COLOR[1], FOG_COLOR[2], noise);
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
				//逐通道比较（前三个通道）
				int maximum = 0;
				for (std::size_t index = 0; index < 3; ++index)
				{
					int delta = static_cast<int>(a[index]) - static_cast<int>(b[index]);
					if (delta < 0)
						delta = -delta;
					if (delta > maximum)
						maximum = delta;
				}
				if (maximum >= threshold)
					++count;
			}
		return count;
	}
}

//密度 0 时体积雾阶段原样输出：与直通画面逐通道差不超过 2
TEST_F(Renderer_Post_Chain_Test, 体积雾关闭原样输出)
{
	engine::Renderer probe;
	ASSERT_TRUE(probe.init(hidden_config()));
	if (!probe.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	std::vector<uint8_t> reference = render_fog({ 0.0f, 0.0f, 0.0f, 0.0f });
	//密度 0：与直通等价（后处理.frag 直通由 render_fog 的同一链路径对照，故此处另一组用直通管线）
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
	RecordProperty("fog_disabled_max_diff", difference);
	EXPECT_LE(difference, 2) << "密度 0 未原样输出（最大差 " << difference << "）";
}

/*
密度 > 0 时远景被雾覆盖（第三百七十三轮）
口径：同一场景（中央四边形 ＋ 天空），雾色亮于清屏色；断言差异像素数 > 20 且整帧平均亮度上升。
*/
TEST_F(Renderer_Post_Chain_Test, 体积雾覆盖远景)
{
	engine::Renderer probe;
	ASSERT_TRUE(probe.init(hidden_config()));
	if (!probe.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	//密度 0.2、无高度衰减、最大 20 世界单位、步数缩放 1（24 步）
	std::vector<uint8_t> reference = render_fog({ 0.0f, 0.0f, 0.0f, 0.0f });
	std::vector<uint8_t> fogged = render_fog({ 0.2f, 0.0f, 20.0f, 1.0f });
	uint32_t changed = differing_pixels(reference, fogged, 8);
	float luma_before = average_luma(reference);
	float luma_after = average_luma(fogged);
	RecordProperty("fog_changed_pixels", static_cast<int>(changed));
	RecordProperty("fog_luma_delta", static_cast<double>(luma_after - luma_before));
	std::cout << "[体积雾] 差异像素=" << changed << " 亮度 " << luma_before << " → " << luma_after << std::endl;
	EXPECT_GT(changed, 20u);
	EXPECT_GT(luma_after, luma_before + 5.0f);
}

//高度衰减参数生效：同一密度下开启衰减与关闭衰减的画面存在差异（世界 Y > 0 处密度下降）
TEST_F(Renderer_Post_Chain_Test, 体积雾高度衰减)
{
	engine::Renderer probe;
	ASSERT_TRUE(probe.init(hidden_config()));
	if (!probe.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	std::vector<uint8_t> flat = render_fog({ 0.3f, 0.0f, 20.0f, 1.0f });
	std::vector<uint8_t> height = render_fog({ 0.3f, 2.0f, 20.0f, 1.0f });
	uint32_t changed = differing_pixels(flat, height, 2);
	RecordProperty("fog_height_changed_pixels", static_cast<int>(changed));
	EXPECT_GT(changed, 0u) << "高度衰减未生效（两帧完全一致）";
}

/*
体积雾在多级链中取源正确（第三百七十四轮）
口径：两级链「体积雾 → 反色」，靶为两块中间目标（ping-pong）。
1. 密度 0 时首级恒等，故结果应与「直通 → 反色」几乎一致（逐通道差 ≤ 4，含两级编码取整）；
2. 密度 0.2 时首级叠加雾，再经反色后仍与前者显著不同（差异像素 > 20），且两级都执行（post_stages = 2）。
*/
TEST_F(Renderer_Post_Chain_Test, 体积雾多级链取源)
{
	engine::Renderer probe;
	ASSERT_TRUE(probe.init(hidden_config()));
	if (!probe.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	//两级链渲染（首级管线由 fog_shader 指定、参数由 density 控制；次级恒为反色）
	auto render_chain = [&](const char* fog_shader, float density, uint32_t* stages_out) -> std::vector<uint8_t>
	{
		engine::Renderer renderer;
		EXPECT_TRUE(renderer.init(hidden_config()));
		uint64_t target0 = 0;
		uint64_t target1 = 0;
		EXPECT_TRUE(renderer.render_target_create(target0, renderer.extent_width(), renderer.extent_height()));
		EXPECT_TRUE(renderer.render_target_create(target1, renderer.extent_width(), renderer.extent_height()));
		uint64_t fog_pass = post_pipeline_register(renderer, fog_shader);
		uint64_t invert_pass = post_pipeline_register(renderer, "反色.frag.spv");
		EXPECT_NE(fog_pass, 0u);
		EXPECT_NE(invert_pass, 0u);
		uint64_t chain[2] = { fog_pass, invert_pass };
		EXPECT_TRUE(renderer.post_chain_set(chain, 2));
		renderer.post_stage_vec4_set(0, density, 0.0f, 20.0f, 1.0f);
		renderer.post_stage_color_set(0, FOG_COLOR[0], FOG_COLOR[1], FOG_COLOR[2]);
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
		renderer.pipeline_unload(invert_pass);
		renderer.render_target_destroy(target1);
		renderer.render_target_destroy(target0);
		return pixels;
	};
	uint32_t stages_identity = 0;
	uint32_t stages_fogged = 0;
	std::vector<uint8_t> identity = render_chain("后处理.frag.spv", 0.0f, &stages_identity);
	std::vector<uint8_t> fog_off = render_chain("体积雾.frag.spv", 0.0f, nullptr);
	std::vector<uint8_t> fog_on = render_chain("体积雾.frag.spv", 0.2f, &stages_fogged);
	//两级都执行
	EXPECT_EQ(stages_identity, 2u);
	EXPECT_EQ(stages_fogged, 2u);
	//密度 0 时首级恒等：与「直通 → 反色」几乎一致（两级编码取整容差 4）
	int identity_diff = max_channel_diff(identity, fog_off);
	RecordProperty("fog_chain_identity_diff", identity_diff);
	EXPECT_LE(identity_diff, CHAIN_TOLERANCE) << "密度 0 在多级链中未恒等（最大差 " << identity_diff << "）";
	//密度 0.2 时结果显著不同
	uint32_t changed = differing_pixels(fog_off, fog_on, 8);
	RecordProperty("fog_chain_changed_pixels", static_cast<int>(changed));
	EXPECT_GT(changed, 20u) << "多级链中体积雾未生效";
}

/*
体积雾步数开销（第三百七十五轮）
同一场景（覆盖视口的四边形 ＋ 天空），单级体积雾链；分辨率与步数三档对照，
记录稳定段（第 3..8 帧）的主机录制与 GPU 帧均值；不设硬上限，只输出与粗检。
*/
TEST_F(Renderer_Post_Chain_Test, 体积雾步数开销)
{
	engine::Renderer probe;
	ASSERT_TRUE(probe.init(hidden_config()));
	if (!probe.gpu_timestamp_supported())
		GTEST_SKIP() << "本机不支持 GPU 时间戳";

	//单次测量（分辨率 / 步数缩放进 / 录制与 GPU 帧出）
	auto measure = [&](uint32_t width, uint32_t height, float step_scale, double& record_out, double& gpu_out) -> void
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
		uint64_t pass = post_pipeline_register(renderer, "体积雾.frag.spv");
		ASSERT_NE(pass, 0u);
		ASSERT_TRUE(renderer.post_chain_set(&pass, 1));
		renderer.post_stage_vec4_set(0, 0.1f, 0.0f, 30.0f, step_scale);
		renderer.post_stage_color_set(0, FOG_COLOR[0], FOG_COLOR[1], FOG_COLOR[2]);
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
	double small_24_record = 0.0, small_24_gpu = 0.0;
	double small_48_record = 0.0, small_48_gpu = 0.0;
	double large_24_record = 0.0, large_24_gpu = 0.0;
	measure(64, 36, 0.5f, small_12_record, small_12_gpu);
	measure(64, 36, 1.0f, small_24_record, small_24_gpu);
	measure(64, 36, 2.0f, small_48_record, small_48_gpu);
	measure(320, 180, 1.0f, large_24_record, large_24_gpu);
	//全高清三档（步数效应在该分辨率下才可辨）
	double full_12_record = 0.0, full_12_gpu = 0.0;
	double full_24_record = 0.0, full_24_gpu = 0.0;
	double full_48_record = 0.0, full_48_gpu = 0.0;
	measure(1280, 720, 0.5f, full_12_record, full_12_gpu);
	measure(1280, 720, 1.0f, full_24_record, full_24_gpu);
	measure(1280, 720, 2.0f, full_48_record, full_48_gpu);
	std::cout << "[体积雾开销] 64×36 12步 录制=" << small_12_record << " GPU帧=" << small_12_gpu
		<< " ms；24步 录制=" << small_24_record << " GPU帧=" << small_24_gpu
		<< " ms；48步 录制=" << small_48_record << " GPU帧=" << small_48_gpu
		<< " ms；320×180 24步 录制=" << large_24_record << " GPU帧=" << large_24_gpu
		<< " ms；1280×720 12步 录制=" << full_12_record << " GPU帧=" << full_12_gpu
		<< " ms；24步 录制=" << full_24_record << " GPU帧=" << full_24_gpu
		<< " ms；48步 录制=" << full_48_record << " GPU帧=" << full_48_gpu << " ms" << std::endl;
	//粗检：主机录制非零；GPU 帧耗时随步数单调方向合理（24 步 ≥ 12 步的一半）
	EXPECT_GT(small_12_record, 0.0);
	EXPECT_GT(large_24_record, 0.0);
	EXPECT_GT(small_24_gpu, small_12_gpu * 0.5);
	EXPECT_GT(small_48_gpu, small_24_gpu * 0.5);
}

/*
体积雾密度噪声（第三百七十六轮）
第三组推送 alpha 作为噪声强度：0 = 均匀雾（与引入噪声前一致）、0.8 = 絮状浓淡（画面出现可见差异）；
同时对照 720p 下噪声开关的 GPU 帧耗时（解析式噪声，预期增幅有限）。
*/
TEST_F(Renderer_Post_Chain_Test, 体积雾密度噪声)
{
	engine::Renderer probe;
	ASSERT_TRUE(probe.init(hidden_config()));
	if (!probe.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	std::vector<uint8_t> smooth = render_fog({ 0.2f, 0.0f, 20.0f, 1.0f }, 0.0f);
	std::vector<uint8_t> noisy = render_fog({ 0.2f, 0.0f, 20.0f, 1.0f }, 0.8f);
	uint32_t changed = differing_pixels(smooth, noisy, 2);
	RecordProperty("fog_noise_changed_pixels", static_cast<int>(changed));
	std::cout << "[体积雾噪声] 差异像素=" << changed << std::endl;
	EXPECT_GT(changed, 20u) << "密度噪声未生效（噪声 0 与 0.8 画面几乎一致）";

	//全高清 24 步下噪声开关的 GPU 帧对照
	auto measure_noise = [&](float noise, double& gpu_out) -> void
	{
		gpu_out = 0.0;
		engine::Renderer_Config config;
		config.width = 1280;
		config.height = 720;
		config.hidden = true;
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(config));
		uint64_t scene_target = 0;
		ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
		uint64_t pass = post_pipeline_register(renderer, "体积雾.frag.spv");
		ASSERT_NE(pass, 0u);
		ASSERT_TRUE(renderer.post_chain_set(&pass, 1));
		renderer.post_stage_vec4_set(0, 0.1f, 0.0f, 30.0f, 1.0f);
		renderer.post_stage_color4_set(0, FOG_COLOR[0], FOG_COLOR[1], FOG_COLOR[2], noise);
		renderer.render_target_present_set(scene_target);
		engine::Mesh_Handle plane = 0;
		ASSERT_TRUE(renderer.mesh_load(plane, quad_data()));
		double gpu_sum = 0.0;
		uint32_t samples = 0;
		for (uint32_t frame = 0; frame < 8; ++frame)
		{
			ASSERT_TRUE(renderer.mesh_draw(plane, nullptr));
			ASSERT_TRUE(renderer.frame_render());
			if (frame >= 2)
			{
				gpu_sum += renderer.frame_stats_get().gpu_frame_ms;
				++samples;
			}
		}
		gpu_out = gpu_sum / samples;
		renderer.mesh_unload(plane);
		renderer.pipeline_unload(pass);
		renderer.render_target_destroy(scene_target);
	};
	double smooth_gpu = 0.0;
	double noisy_gpu = 0.0;
	measure_noise(0.0f, smooth_gpu);
	measure_noise(0.8f, noisy_gpu);
	std::cout << "[体积雾噪声开销] 720p 24步 均匀 GPU帧=" << smooth_gpu
		<< " ms 噪声 GPU帧=" << noisy_gpu << " ms" << std::endl;
	EXPECT_GT(smooth_gpu, 0.0);
	//解析式噪声的增量有限（不超过均匀雾的 4 倍＋0.5 ms）
	EXPECT_LT(noisy_gpu, smooth_gpu * 4.0 + 0.5);
}

/*
体积雾与深度描边组合（第三百七十八轮）
口径：两级链「体积雾 → 深度描边」，靶为两块中间目标。
1. 雾 0（首级恒等）时，与「直通 → 深度描边(强度 0)」逐通道差 ≤ 4（两级编码取整）；
2. 雾 0.2 ＋ 描边 0.5 相对前者差异像素 > 20，且 post_stages = 2（两自定义阶段串联正确）。
*/
TEST_F(Renderer_Post_Chain_Test, 体积雾与深度描边组合)
{
	engine::Renderer probe;
	ASSERT_TRUE(probe.init(hidden_config()));
	if (!probe.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	//两级链渲染（fog_density = 0 表示首级恒等；outline > 0 时次级做深度描边）
	auto render_pair = [&](const char* first_shader, float fog_density, float outline,
		uint32_t* stages_out) -> std::vector<uint8_t>
	{
		engine::Renderer renderer;
		EXPECT_TRUE(renderer.init(hidden_config()));
		uint64_t target0 = 0;
		uint64_t target1 = 0;
		EXPECT_TRUE(renderer.render_target_create(target0, renderer.extent_width(), renderer.extent_height()));
		EXPECT_TRUE(renderer.render_target_create(target1, renderer.extent_width(), renderer.extent_height()));
		uint64_t first = post_pipeline_register(renderer, first_shader);
		uint64_t outline_pass = post_pipeline_register(renderer, "深度描边.frag.spv");
		EXPECT_NE(first, 0u);
		EXPECT_NE(outline_pass, 0u);
		uint64_t chain[2] = { first, outline_pass };
		EXPECT_TRUE(renderer.post_chain_set(chain, 2));
		//首级：体积雾四参（通用入口）
		renderer.post_stage_vec4_set(0, fog_density, 0.0f, 20.0f, 1.0f);
		renderer.post_stage_color4_set(0, FOG_COLOR[0], FOG_COLOR[1], FOG_COLOR[2], 0.0f);
		//次级：深度描边（强度 / 保留 / 深度阈值 / 半径；调色范围校验内）
		renderer.post_stage_param4_set(1, outline, 1.0f, 1.0f, 1.0f);
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
		renderer.pipeline_unload(first);
		renderer.pipeline_unload(outline_pass);
		renderer.render_target_destroy(target1);
		renderer.render_target_destroy(target0);
		return pixels;
	};
	uint32_t stages = 0;
	std::vector<uint8_t> reference = render_pair("后处理.frag.spv", 0.0f, 0.0f, nullptr);
	std::vector<uint8_t> fog_off = render_pair("体积雾.frag.spv", 0.0f, 0.0f, nullptr);
	std::vector<uint8_t> combined = render_pair("体积雾.frag.spv", 0.2f, 0.5f, &stages);
	//雾关闭时首级恒等
	int identity_diff = max_channel_diff(reference, fog_off);
	RecordProperty("fog_outline_identity_diff", identity_diff);
	EXPECT_LE(identity_diff, CHAIN_TOLERANCE) << "雾关闭时首级未恒等（最大差 " << identity_diff << "）";
	//组合生效且两级都执行
	uint32_t changed = differing_pixels(fog_off, combined, 8);
	RecordProperty("fog_outline_changed_pixels", static_cast<int>(changed));
	std::cout << "[雾＋描边] 差异像素=" << changed << " 级数=" << stages << std::endl;
	EXPECT_EQ(stages, 2u);
	EXPECT_GT(changed, 20u) << "雾与描边组合未生效";
}

/*
四级链目标轮转（第三百八十一轮）
口径：四级链「体积雾 → 深度描边 → 灰度 → 反色」，靶只用两块中间目标（ping-pong 轮转两圈）。
1. 四级全部执行（post_stages = 4）；
2. 雾关闭 ＋ 描边强度 0 时首两级恒等，整链应与「直通 → 直通 → 灰度 → 反色」逐通道差 ≤ 4；
3. 雾 0.2 ＋ 描边 0.5 时与前者差异像素 > 20。
*/
TEST_F(Renderer_Post_Chain_Test, 四级链目标轮转)
{
	engine::Renderer probe;
	ASSERT_TRUE(probe.init(hidden_config()));
	if (!probe.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	//四级链渲染（fog 为密度；outline 为描边强度；outline > 0 才设描边参数）
	auto render_four = [&](const char* first_shader, float fog, float outline, uint32_t* stages_out) -> std::vector<uint8_t>
	{
		engine::Renderer renderer;
		EXPECT_TRUE(renderer.init(hidden_config()));
		uint64_t target0 = 0;
		uint64_t target1 = 0;
		EXPECT_TRUE(renderer.render_target_create(target0, renderer.extent_width(), renderer.extent_height()));
		EXPECT_TRUE(renderer.render_target_create(target1, renderer.extent_width(), renderer.extent_height()));
		uint64_t first = post_pipeline_register(renderer, first_shader);
		uint64_t outline_pass = post_pipeline_register(renderer, "深度描边.frag.spv");
		uint64_t gray = post_pipeline_register(renderer, "灰度.frag.spv");
		uint64_t invert = post_pipeline_register(renderer, "反色.frag.spv");
		EXPECT_NE(first, 0u);
		EXPECT_NE(outline_pass, 0u);
		EXPECT_NE(gray, 0u);
		EXPECT_NE(invert, 0u);
		uint64_t chain[4] = { first, outline_pass, gray, invert };
		EXPECT_TRUE(renderer.post_chain_set(chain, 4));
		renderer.post_stage_vec4_set(0, fog, 0.0f, 20.0f, 1.0f);
		renderer.post_stage_color4_set(0, FOG_COLOR[0], FOG_COLOR[1], FOG_COLOR[2], 0.0f);
		//描边参数只在需要时设置（默认强度 0 与「不设」在首两级恒等的判据内等价）
		if (outline > 0.0f)
			renderer.post_stage_param4_set(1, outline, 1.0f, 1.0f, 1.0f);
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
		renderer.pipeline_unload(first);
		renderer.pipeline_unload(outline_pass);
		renderer.pipeline_unload(gray);
		renderer.pipeline_unload(invert);
		renderer.render_target_destroy(target1);
		renderer.render_target_destroy(target0);
		return pixels;
	};
	uint32_t stages = 0;
	std::vector<uint8_t> reference = render_four("后处理.frag.spv", 0.0f, 0.0f, &stages);
	std::vector<uint8_t> fog_off = render_four("体积雾.frag.spv", 0.0f, 0.0f, nullptr);
	std::vector<uint8_t> combined = render_four("体积雾.frag.spv", 0.2f, 0.5f, nullptr);
	EXPECT_EQ(stages, 4u);
	//四轮编码取整：容差按级数放宽（每级 ±1）
	int identity_diff = max_channel_diff(reference, fog_off);
	RecordProperty("chain4_identity_diff", identity_diff);
	EXPECT_LE(identity_diff, CHAIN_TOLERANCE) << "前两级未恒等（最大差 " << identity_diff << "）";
	uint32_t changed = differing_pixels(fog_off, combined, 8);
	RecordProperty("chain4_changed_pixels", static_cast<int>(changed));
	std::cout << "[四级链] 恒等差=" << identity_diff << " 组合差异像素=" << changed << std::endl;
	EXPECT_GT(changed, 20u) << "四级链中雾与描边未生效";
}
