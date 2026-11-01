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

	//渲染一帧：单级体积雾链 ＋ 中央四边形（返回回读像素）
	std::vector<uint8_t> render_fog(const std::array<float, 4>& params)
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
		renderer.post_stage_color_set(0, FOG_COLOR[0], FOG_COLOR[1], FOG_COLOR[2]);
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
