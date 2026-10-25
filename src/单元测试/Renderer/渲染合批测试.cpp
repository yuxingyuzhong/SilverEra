/*
渲染静态合批测试（第一百二十七轮；第一百三十轮补充透明项口径）
口径：
  ① 相邻且完全等价的绘制项合并为一次实例化绘制：drawn_items 下降、drawn_instances 不变、画面逐字节一致；
  ② 推送常量不同的相邻项不合并（不同色调材质各自成项）；
  ③ 合批开关可运行时切换，关闭即恢复「一绘制项一次调用」；
  ④ 透明混合绘制项（文字 / Alpha 档几何）不参与合批，避免混叠顺序改变；恢复不透明后照常合批。
*/
#include "渲染场景测试辅助.h"

#include <filesystem>

/*
相邻同网格同状态绘制合并（第一百二十七轮）
同一网格连续登记 5 次：开启合批时实际绘制 1 项 / 5 实例；关闭时 5 项 / 5 实例；
两种配置的整帧像素逐字节一致（合批不改变绘制结果）。
*/
TEST_F(Renderer_Scene_Test, 相邻同等绘制自动合批)
{
	engine::Mesh_Handle mesh = 0;
	//开启合批的渲染器
	std::vector<uint8_t> batched_pixels;
	engine::Frame_Stats batched_stats{};
	{
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(hidden_config()));
		if (!renderer.readback_supported())
			GTEST_SKIP() << "本机交换链图像不具备传输源用法";
		ASSERT_TRUE(renderer.static_batching()) << "合批默认应开启";
		ASSERT_TRUE(renderer.mesh_load(mesh, quad_data_with_normal(0.0f, 0.0f, 1.0f)));
		for (uint32_t index = 0; index < 5; ++index)
			ASSERT_TRUE(renderer.mesh_draw(mesh, nullptr));
		batched_pixels.resize(pixel_bytes());
		ASSERT_TRUE(renderer.frame_render(batched_pixels.data(), static_cast<uint32_t>(batched_pixels.size())));
		batched_stats = renderer.frame_stats_get();
		renderer.mesh_unload(mesh);
	}
	//关闭合批的渲染器（对照）
	std::vector<uint8_t> plain_pixels;
	engine::Frame_Stats plain_stats{};
	{
		engine::Renderer renderer;
		engine::Renderer_Config config = hidden_config();
		config.static_batching = false;
		ASSERT_TRUE(renderer.init(config));
		ASSERT_FALSE(renderer.static_batching());
		ASSERT_TRUE(renderer.mesh_load(mesh, quad_data_with_normal(0.0f, 0.0f, 1.0f)));
		for (uint32_t index = 0; index < 5; ++index)
			ASSERT_TRUE(renderer.mesh_draw(mesh, nullptr));
		plain_pixels.resize(pixel_bytes());
		ASSERT_TRUE(renderer.frame_render(plain_pixels.data(), static_cast<uint32_t>(plain_pixels.size())));
		plain_stats = renderer.frame_stats_get();
		renderer.mesh_unload(mesh);
	}

	RecordProperty("batching_drawn_items", static_cast<int>(batched_stats.drawn_items));
	RecordProperty("plain_drawn_items", static_cast<int>(plain_stats.drawn_items));
	RecordProperty("batching_drawn_instances", static_cast<int>(batched_stats.drawn_instances));

	//记录列表只剩合并后的项，故 draw_items 与实际绘制量同步下降
	EXPECT_EQ(batched_stats.draw_items, 1u) << "合批后绘制列表未收敛";
	EXPECT_EQ(plain_stats.draw_items, 5u);
	EXPECT_EQ(plain_stats.drawn_items, 5u);
	EXPECT_EQ(batched_stats.drawn_items, 1u) << "相邻同状态绘制未合并";
	//实例数不变
	EXPECT_EQ(batched_stats.instances, 5u);
	EXPECT_EQ(batched_stats.drawn_instances, 5u);
	//画面逐字节一致
	EXPECT_EQ(batched_pixels, plain_pixels) << "合批改变了画面";
}

/*
推送常量不同的相邻项不合并（第一百二十七轮）
红色调材质连续两次、蓝色调材质连续两次：两次色组内部各合并为 1 项，色组之间不合并（共 2 项）。
*/
TEST_F(Renderer_Scene_Test, 不同色调材质不合并)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data_with_normal(0.0f, 0.0f, 1.0f)));

	//红色调材质
	engine::Material_Data red{};
	red.tint_override = true;
	red.tint[0] = 1.0f;
	red.tint[1] = 0.0f;
	red.tint[2] = 0.0f;
	red.tint[3] = 1.0f;
	engine::Material_Handle material_red = 0;
	ASSERT_TRUE(renderer.material_load(material_red, red));
	//蓝色调材质
	engine::Material_Data blue = red;
	blue.tint[0] = 0.0f;
	blue.tint[2] = 1.0f;
	engine::Material_Handle material_blue = 0;
	ASSERT_TRUE(renderer.material_load(material_blue, blue));

	//红 ×2 → 蓝 ×2
	renderer.material_bind_set(material_red);
	ASSERT_TRUE(renderer.mesh_draw(mesh, nullptr));
	ASSERT_TRUE(renderer.mesh_draw(mesh, nullptr));
	renderer.material_bind_set(material_blue);
	ASSERT_TRUE(renderer.mesh_draw(mesh, nullptr));
	ASSERT_TRUE(renderer.mesh_draw(mesh, nullptr));

	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	engine::Frame_Stats stats = renderer.frame_stats_get();

	RecordProperty("tint_drawn_items", static_cast<int>(stats.drawn_items));
	EXPECT_EQ(stats.draw_items, 2u) << "同色调内部未合并或跨色调被误合并";
	EXPECT_EQ(stats.drawn_items, 2u);
	EXPECT_EQ(stats.drawn_instances, 4u);
	//画面：中心应为蓝色调（后登记覆盖先登记）
	EXPECT_GT(center_brightness(pixels), 0u) << "未绘制出内容";

	renderer.material_unload(material_red);
	renderer.material_unload(material_blue);
	renderer.mesh_unload(mesh);
}

/*
合批开关可运行时切换（第一百二十七轮）
同一渲染器：关闭时连画 3 次 → 实际绘制 +3；开启时连画 3 次 → 实际绘制仅 +1。
*/
TEST_F(Renderer_Scene_Test, 合批开关运行时可切换)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data_with_normal(0.0f, 0.0f, 1.0f)));

	//关闭合批：3 项全部实际绘制
	renderer.static_batching_set(false);
	ASSERT_FALSE(renderer.static_batching());
	uint64_t before = renderer.frame_stats_get().drawn_items;
	for (uint32_t index = 0; index < 3; ++index)
		ASSERT_TRUE(renderer.mesh_draw(mesh, nullptr));
	ASSERT_TRUE(renderer.frame_render());
	const uint64_t plain_delta = renderer.frame_stats_get().drawn_items - before;

	//开启合批：3 项合并为 1 次实例化绘制
	renderer.static_batching_set(true);
	ASSERT_TRUE(renderer.static_batching());
	before = renderer.frame_stats_get().drawn_items;
	for (uint32_t index = 0; index < 3; ++index)
		ASSERT_TRUE(renderer.mesh_draw(mesh, nullptr));
	ASSERT_TRUE(renderer.frame_render());
	const uint64_t batched_delta = renderer.frame_stats_get().drawn_items - before;

	RecordProperty("batching_toggle_plain", static_cast<int>(plain_delta));
	RecordProperty("batching_toggle_batched", static_cast<int>(batched_delta));
	EXPECT_EQ(plain_delta, 3u) << "关闭合批时实际绘制量异常";
	EXPECT_EQ(batched_delta, 1u) << "开启合批后未合并";

	renderer.mesh_unload(mesh);
}

/*
阴影列表同样参与合批（第一百二十九轮）
同一遮挡面连续两次 ＋ 接收面：开合批时主 pass 与阴影 pass 的绘制项均减少，
两配置整帧像素逐字节一致（阴影图与主画面不变）。
*/
TEST_F(Renderer_Scene_Test, 阴影列表同样合批)
{
	//开合批
	std::vector<uint8_t> batched_pixels;
	engine::Frame_Stats batched_stats{};
	{
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(hidden_config()));
		if (!renderer.readback_supported())
			GTEST_SKIP() << "本机交换链图像不具备传输源用法";
		engine::Mesh_Handle receiver = 0;
		engine::Mesh_Handle occluder = 0;
		ASSERT_TRUE(renderer.mesh_load(receiver, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
		ASSERT_TRUE(renderer.mesh_load(occluder, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
		//场景：方向光沿 +Z、强度 1
		engine::Camera_Data camera{};
		engine::Light_Data light{};
		light.direction[2] = 1.0f;
		light.intensity = 1.0f;
		light.ambient = 0.0f;
		engine::Fog_Data fog{};
		renderer.scene_apply(camera, light, fog);
		renderer.shadow_enable_set(true);
		//遮挡面缩放 0.6、平移到 z = -0.5（同一变换连续两次）
		float transform[16] = {};
		transform[0] = 0.6f;
		transform[5] = 0.6f;
		transform[10] = 0.6f;
		transform[14] = -0.5f;
		transform[15] = 1.0f;
		ASSERT_TRUE(renderer.mesh_draw(occluder, transform));
		ASSERT_TRUE(renderer.mesh_draw(occluder, transform));
		ASSERT_TRUE(renderer.mesh_draw(receiver, nullptr));
		batched_pixels.resize(pixel_bytes());
		ASSERT_TRUE(renderer.frame_render(batched_pixels.data(), static_cast<uint32_t>(batched_pixels.size())));
		batched_stats = renderer.frame_stats_get();
		renderer.mesh_unload(receiver);
		renderer.mesh_unload(occluder);
	}
	//关合批（对照）
	std::vector<uint8_t> plain_pixels;
	engine::Frame_Stats plain_stats{};
	{
		engine::Renderer renderer;
		engine::Renderer_Config config = hidden_config();
		config.static_batching = false;
		ASSERT_TRUE(renderer.init(config));
		engine::Mesh_Handle receiver = 0;
		engine::Mesh_Handle occluder = 0;
		ASSERT_TRUE(renderer.mesh_load(receiver, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
		ASSERT_TRUE(renderer.mesh_load(occluder, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
		engine::Camera_Data camera{};
		engine::Light_Data light{};
		light.direction[2] = 1.0f;
		light.intensity = 1.0f;
		light.ambient = 0.0f;
		engine::Fog_Data fog{};
		renderer.scene_apply(camera, light, fog);
		renderer.shadow_enable_set(true);
		float transform[16] = {};
		transform[0] = 0.6f;
		transform[5] = 0.6f;
		transform[10] = 0.6f;
		transform[14] = -0.5f;
		transform[15] = 1.0f;
		ASSERT_TRUE(renderer.mesh_draw(occluder, transform));
		ASSERT_TRUE(renderer.mesh_draw(occluder, transform));
		ASSERT_TRUE(renderer.mesh_draw(receiver, nullptr));
		plain_pixels.resize(pixel_bytes());
		ASSERT_TRUE(renderer.frame_render(plain_pixels.data(), static_cast<uint32_t>(plain_pixels.size())));
		plain_stats = renderer.frame_stats_get();
		renderer.mesh_unload(receiver);
		renderer.mesh_unload(occluder);
	}

	RecordProperty("shadow_batching_drawn", static_cast<int>(batched_stats.drawn_items));
	RecordProperty("shadow_plain_drawn", static_cast<int>(plain_stats.drawn_items));
	//阴影 pass 三项 → 合批后两项；主 pass 同理
	EXPECT_LT(batched_stats.drawn_items, plain_stats.drawn_items) << "阴影列表未参与合批";
	EXPECT_EQ(batched_pixels, plain_pixels) << "阴影合批改变了画面";
}

/*
文字绘制项不参与合批（第一百三十轮）
同一文本在同一位置连续登记两次：开启合批时仍为 2 项 / 2 实例（文字为透明混合，合批会改变混叠顺序），
两种配置的整帧像素逐字节一致，且确实画出了墨迹。
*/
TEST_F(Renderer_Scene_Test, 文字绘制项不参与合批)
{
	//测试用系统字体路径
	const char* const font_path = "C:\\Windows\\Fonts\\consola.ttf";
	//若本机没有测试用字体则跳过
	std::error_code code;
	if (!std::filesystem::exists(std::filesystem::path(font_path), code))
		GTEST_SKIP() << "本机无 " << font_path;
	//探针渲染器：确认本机支持像素回读
	{
		engine::Renderer probe;
		ASSERT_TRUE(probe.init(hidden_config()));
		if (!probe.readback_supported())
			GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	}
	/*
	按合批开关渲染一帧「同一文本重复两次」
	成功回填像素与帧统计并返回真。
	*/
	auto render_frame = [&font_path](bool batching, std::vector<uint8_t>& pixels, engine::Frame_Stats& stats)
	{
		engine::Renderer renderer;
		engine::Renderer_Config config = hidden_config();
		config.static_batching = batching;
		//初始化失败直接返回
		if (!renderer.init(config))
			return false;
		//加载测试字体
		uint64_t font = 0;
		if (!renderer.font_load_file(font, font_path, 16))
			return false;
		//同一文本、同一位置连续两次
		if (!renderer.text_draw(font, "AB", 4.0f, 4.0f) || !renderer.text_draw(font, "AB", 4.0f, 4.0f))
			return false;
		pixels.resize(pixel_bytes());
		//整帧渲染并回读
		if (!renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())))
			return false;
		stats = renderer.frame_stats_get();
		return true;
	};

	std::vector<uint8_t> batched_pixels;
	std::vector<uint8_t> plain_pixels;
	engine::Frame_Stats batched_stats{};
	engine::Frame_Stats plain_stats{};
	ASSERT_TRUE(render_frame(true, batched_pixels, batched_stats));
	ASSERT_TRUE(render_frame(false, plain_pixels, plain_stats));

	RecordProperty("text_drawn_items", static_cast<int>(batched_stats.drawn_items));
	//文字项虽状态全等，但为透明混合，须逐项保留
	EXPECT_EQ(batched_stats.draw_items, 2u) << "文字项被误合批";
	EXPECT_EQ(batched_stats.drawn_items, 2u) << "文字项被误合批";
	EXPECT_EQ(batched_stats.drawn_instances, 2u);
	//画面与关闭合批时逐字节一致
	EXPECT_EQ(batched_pixels, plain_pixels) << "文字合批改变了画面";

	//统计与左下角背景像素不同的像素数，确认文字确实画出内容
	std::array<uint8_t, 4> background = pixel_get(batched_pixels, 0, WINDOW_HEIGHT - 1);
	uint32_t ink = 0;
	for (uint32_t y = 0; y < WINDOW_HEIGHT; ++y)
		for (uint32_t x = 0; x < WINDOW_WIDTH; ++x)
			if (pixel_get(batched_pixels, x, y) != background)
				++ink;
	EXPECT_GT(ink, 0u) << "文字未画出任何墨迹";
}

/*
透明几何不参与合批、恢复不透明后照常合批（第一百三十轮）
同一网格在 Alpha 混合档下连续两次登记 → 2 项；切回不透明档后连续两次登记 → 合并为 1 项。
*/
TEST_F(Renderer_Scene_Test, 透明档几何不参与合批)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data_with_normal(0.0f, 0.0f, 1.0f)));

	//Alpha 混合档：同网格连续两次不合并
	renderer.blend_mode_set(engine::Blend_Mode::Alpha);
	uint64_t before = renderer.frame_stats_get().draw_items;
	ASSERT_TRUE(renderer.mesh_draw(mesh, nullptr));
	ASSERT_TRUE(renderer.mesh_draw(mesh, nullptr));
	ASSERT_TRUE(renderer.frame_render());
	const uint64_t transparent_delta = renderer.frame_stats_get().draw_items - before;

	//恢复不透明档：同网格连续两次合并为一项
	renderer.blend_mode_set(engine::Blend_Mode::Opaque);
	before = renderer.frame_stats_get().draw_items;
	ASSERT_TRUE(renderer.mesh_draw(mesh, nullptr));
	ASSERT_TRUE(renderer.mesh_draw(mesh, nullptr));
	ASSERT_TRUE(renderer.frame_render());
	const uint64_t opaque_delta = renderer.frame_stats_get().draw_items - before;

	RecordProperty("transparent_delta", static_cast<int>(transparent_delta));
	RecordProperty("opaque_delta", static_cast<int>(opaque_delta));
	EXPECT_EQ(transparent_delta, 2u) << "透明混合项被误合批";
	EXPECT_EQ(opaque_delta, 1u) << "恢复不透明后未合批";

	renderer.mesh_unload(mesh);
}

/*
合批收益可观测（第一百三十五轮）
同一渲染器：关闭合批时 5 次同状态绘制不产生合并；开启后 5 次合并为 1 项（本帧合并 4、累计 4），
且实例数不变（5）。
*/
TEST_F(Renderer_Scene_Test, 合批收益可观测)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data_with_normal(0.0f, 0.0f, 1.0f)));

	//关闭合批：不产生合并
	renderer.static_batching_set(false);
	for (uint32_t index = 0; index < 5; ++index)
		ASSERT_TRUE(renderer.mesh_draw(mesh, nullptr));
	ASSERT_TRUE(renderer.frame_render());
	engine::Frame_Stats plain = renderer.frame_stats_get();
	EXPECT_EQ(plain.last_merged_items, 0u) << "关闭合批时不应有合并";
	EXPECT_EQ(plain.merged_items, 0u);

	//开启合批：5 项合并为 1 项
	renderer.static_batching_set(true);
	for (uint32_t index = 0; index < 5; ++index)
		ASSERT_TRUE(renderer.mesh_draw(mesh, nullptr));
	ASSERT_TRUE(renderer.frame_render());
	engine::Frame_Stats batched = renderer.frame_stats_get();
	RecordProperty("merged_last_frame", static_cast<int>(batched.last_merged_items));
	EXPECT_EQ(batched.last_merged_items, 4u) << "本帧合并项数不符";
	EXPECT_EQ(batched.merged_items, 4u) << "累计合并项数不符";
	EXPECT_EQ(batched.last_instances, 5u) << "合并后实例数应保持";

	renderer.mesh_unload(mesh);
}
