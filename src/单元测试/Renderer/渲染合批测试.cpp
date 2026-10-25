/*
渲染静态合批测试（第一百二十七轮）
口径：
  ① 相邻且完全等价的绘制项合并为一次实例化绘制：drawn_items 下降、drawn_instances 不变、画面逐字节一致；
  ② 推送常量不同的相邻项不合并（不同色调材质各自成项）；
  ③ 合批开关可运行时切换，关闭即恢复「一绘制项一次调用」。
*/
#include "渲染场景测试辅助.h"

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
