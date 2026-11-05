/*
渲染阴影诊断测试（第六百四十七轮；深研方向八 A4 补齐）
覆盖：方向光级联阴影 pass 数（单级 1 / 双级 2）、静止帧复用（pass 数归 0）、
     聚光瓦片 pass 数与阴影图边长（shadow_resolution）。
口径：默认阴影分辨率 1024；阴影 pass 计数为「最近一帧」语义，复用帧为 0。
*/
#include "渲染场景测试辅助.h"

namespace
{
	//构造沿 X、Z 平移的模型矩阵（列主序）
	std::array<float, 16> translate_model(float x, float y, float z)
	{
		return {
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			x, y, z, 1.0f,
		};
	}

	//构造缩放并平移到遮挡位置的模型矩阵（列主序）
	std::array<float, 16> occluder_model(float x, float y, float z)
	{
		return {
			0.6f, 0.0f, 0.0f, 0.0f,
			0.0f, 0.6f, 0.0f, 0.0f,
			0.0f, 0.0f, 0.6f, 0.0f,
			x, y, z, 1.0f,
		};
	}

	//构造「方向光沿 +Z、环境光 0」的光照组
	engine::Light_Data positive_z_light(void)
	{
		engine::Light_Data light{};
		light.direction[2] = 1.0f;
		light.intensity = 1.0f;
		light.ambient = 0.0f;
		return light;
	}

	//应用相机与光照（雾保持默认；scene_apply 会按 Light_Data 重置阴影，故随后补开）
	void scene_apply(engine::Renderer& renderer, const engine::Camera_Data& camera)
	{
		engine::Fog_Data fog{};
		renderer.scene_apply(camera, positive_z_light(), fog);
		renderer.shadow_enable_set(true);
	}

	//绘制固定场景并推进一帧（遮挡面 z = -10、接收面 z = -9.5）
	void draw_scene(engine::Renderer& renderer, engine::Mesh_Handle occluder, engine::Mesh_Handle receiver)
	{
		EXPECT_TRUE(renderer.mesh_draw(occluder, occluder_model(0.0f, 0.0f, -10.0f).data()));
		EXPECT_TRUE(renderer.mesh_draw(receiver, translate_model(0.0f, 0.0f, -9.5f).data()));
		EXPECT_TRUE(renderer.frame_render());
	}
}

/*
方向光级联阴影 pass 数（第六百四十七轮）
口径：单级一帧执行 1 次阴影 pass；切双级后一帧执行 2 次（每片一次）；
     阴影图边长为默认 1024；聚光 pass 数为 0。
*/
TEST_F(Renderer_Scene_Test, 阴影pass数随级联数变化)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle receiver = 0;
	engine::Mesh_Handle occluder = 0;
	ASSERT_TRUE(renderer.mesh_load(receiver, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
	ASSERT_TRUE(renderer.mesh_load(occluder, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
	renderer.texture_bind_set(texture);

	//单级
	engine::Camera_Data camera{};
	scene_apply(renderer, camera);
	draw_scene(renderer, occluder, receiver);
	engine::Frame_Stats stats = renderer.frame_stats_get();
	RecordProperty("single_passes", stats.shadow_passes);
	RecordProperty("single_resolution", stats.shadow_resolution);
	EXPECT_EQ(stats.shadow_passes, 1u) << "单级阴影 pass 数应为 1";
	EXPECT_EQ(stats.spot_shadow_passes, 0u) << "未开启聚光阴影时聚光 pass 数应为 0";
	EXPECT_EQ(stats.shadow_resolution, 1024u) << "默认阴影图边长应为 1024";

	//切双级（矩阵变化触发重绘）
	renderer.shadow_cascade_set(2, 3.0f, 12.0f);
	draw_scene(renderer, occluder, receiver);
	stats = renderer.frame_stats_get();
	RecordProperty("dual_passes", stats.shadow_passes);
	EXPECT_EQ(stats.shadow_passes, 2u) << "双级阴影 pass 数应为 2";

	renderer.mesh_unload(occluder);
	renderer.mesh_unload(receiver);
	renderer.texture_unload(texture);
}

/*
静止帧阴影复用（第六百四十七轮）
口径：同一相机与场景再推进一帧，签名未变 → 不执行阴影 pass（shadow_passes = 0），复用计数递增。
*/
TEST_F(Renderer_Scene_Test, 静止帧阴影pass归零)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle receiver = 0;
	engine::Mesh_Handle occluder = 0;
	ASSERT_TRUE(renderer.mesh_load(receiver, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
	ASSERT_TRUE(renderer.mesh_load(occluder, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
	renderer.texture_bind_set(texture);

	engine::Camera_Data camera{};
	scene_apply(renderer, camera);
	renderer.shadow_cascade_set(2, 3.0f, 12.0f);

	//首帧重绘
	draw_scene(renderer, occluder, receiver);
	const uint32_t first_passes = renderer.frame_stats_get().shadow_passes;
	const uint64_t reuses_before = renderer.frame_stats_get().shadow_reuses;
	EXPECT_EQ(first_passes, 2u);

	//第二帧复用
	draw_scene(renderer, occluder, receiver);
	const engine::Frame_Stats stats = renderer.frame_stats_get();
	RecordProperty("reuse_passes", stats.shadow_passes);
	RecordProperty("reuse_count", stats.shadow_reuses);
	EXPECT_EQ(stats.shadow_passes, 0u) << "静止帧不应再次执行阴影 pass";
	EXPECT_GE(stats.shadow_reuses, reuses_before + 1u) << "静止帧未计入阴影复用";

	renderer.mesh_unload(occluder);
	renderer.mesh_unload(receiver);
	renderer.texture_unload(texture);
}

/*
聚光瓦片 pass 数（第六百四十七轮）
口径：开启多盏聚光阴影并只登记 1 盏聚光灯 → 一帧执行 1 次聚光瓦片 pass；
     方向光阴影关闭 → 方向光级联 pass 为 0，总 pass 数 = 1。
*/
TEST_F(Renderer_Scene_Test, 聚光瓦片pass计数)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle occluder = 0;
	ASSERT_TRUE(renderer.mesh_load(occluder, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
	renderer.texture_bind_set(texture);

	//1 盏聚光灯（位于相机上方、朝下照射场景）＋ 多盏聚光阴影 ＋ 方向光阴影关闭
	engine::Light_Data light{};
	light.spot_count = 1;
	light.spot_position[0][1] = 2.0f;
	light.spot_position[0][2] = 3.0f;
	light.spot_direction[0][1] = -1.0f;
	light.spot_direction[0][2] = 0.0f;
	light.spot_radius[0] = 10.0f;
	light.spot_intensity[0] = 1.0f;
	light.spot_inner_deg[0] = 15.0f;
	light.spot_outer_deg[0] = 30.0f;
	light.spot_shadow_enabled = 1;
	light.spot_shadow_all = 1;
	light.spot_shadow_index = 0;
	engine::Camera_Data camera{};
	engine::Fog_Data fog{};
	renderer.scene_apply(camera, light, fog);

	//方向光阴影保持关闭
	EXPECT_TRUE(renderer.mesh_draw(occluder, occluder_model(0.0f, 0.0f, -1.0f).data()));
	EXPECT_TRUE(renderer.frame_render());
	const engine::Frame_Stats stats = renderer.frame_stats_get();
	RecordProperty("spot_passes", stats.spot_shadow_passes);
	RecordProperty("spot_total_passes", stats.shadow_passes);
	EXPECT_GE(stats.spot_shadow_passes, 1u) << "聚光瓦片 pass 未执行";
	EXPECT_EQ(stats.shadow_passes, stats.spot_shadow_passes) << "方向光阴影关闭时总 pass 数应等于聚光 pass 数";

	renderer.mesh_unload(occluder);
	renderer.texture_unload(texture);
}

/*
阴影提交次数等于 pass 数（第六百六十五轮；C2 基线）
口径：当前「一片一次提交」，单级 1 次、双级 2 次，且与 shadow_passes 相等；
     C2 单次提交合并落地后本用例改为「提交恒 1、pass 数不变」。
*/
TEST_F(Renderer_Scene_Test, 阴影提交数等于pass数)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle occluder = 0;
	ASSERT_TRUE(renderer.mesh_load(occluder, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
	renderer.texture_bind_set(texture);

	//方向光沿 +Z、阴影开启（单级）
	engine::Camera_Data camera{};
	engine::Light_Data light{};
	light.direction[2] = 1.0f;
	light.intensity = 1.0f;
	light.ambient = 0.0f;
	light.shadow_enabled = 1;
	light.shadow_cascade_count = 1;
	engine::Fog_Data fog{};
	renderer.scene_apply(camera, light, fog);
	renderer.shadow_enable_set(true);
	EXPECT_TRUE(renderer.mesh_draw(occluder, occluder_model(0.0f, 0.0f, -1.0f).data()));
	EXPECT_TRUE(renderer.frame_render());
	const engine::Frame_Stats single = renderer.frame_stats_get();
	RecordProperty("single_submits", single.shadow_submits);
	RecordProperty("single_passes", single.shadow_passes);
	EXPECT_EQ(single.shadow_submits, 1u) << "单级阴影提交次数不为 1";
	EXPECT_EQ(single.shadow_submits, single.shadow_passes) << "提交次数与 pass 数不一致";

	//双级级联
	light.shadow_cascade_count = 2;
	renderer.scene_apply(camera, light, fog);
	renderer.shadow_enable_set(true);
	EXPECT_TRUE(renderer.mesh_draw(occluder, occluder_model(0.0f, 0.0f, -1.0f).data()));
	EXPECT_TRUE(renderer.frame_render());
	const engine::Frame_Stats dual = renderer.frame_stats_get();
	RecordProperty("dual_submits", dual.shadow_submits);
	RecordProperty("dual_passes", dual.shadow_passes);
	EXPECT_EQ(dual.shadow_submits, 2u) << "双级阴影提交次数不为 2";
	EXPECT_EQ(dual.shadow_submits, dual.shadow_passes) << "双级提交次数与 pass 数不一致";

	renderer.mesh_unload(occluder);
	renderer.texture_unload(texture);
}
