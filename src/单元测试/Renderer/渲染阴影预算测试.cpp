/*
渲染阴影预算测试（第六百五十九轮；深研方向八 B3）
覆盖：预算为 0（默认）不干预、预算折算边长（4 字节/像素、256 倍数、钳制）、多单位（双级）分摊、
     扁平入口透传。
口径：方向光阴影开启，单级 / 双级对照；读取 Frame_Stats.shadow_resolution。
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

	//构造「方向光沿 +Z、环境光 0」的光照组
	engine::Light_Data positive_z_light(void)
	{
		engine::Light_Data light{};
		light.direction[2] = 1.0f;
		light.intensity = 1.0f;
		light.ambient = 0.0f;
		light.shadow_enabled = 1;
		return light;
	}

	//应用光照并推进一帧（返回帧统计）
	engine::Frame_Stats render_frame(engine::Renderer& renderer, engine::Mesh_Handle mesh,
		const engine::Camera_Data& camera, const engine::Light_Data& light)
	{
		engine::Fog_Data fog{};
		renderer.scene_apply(camera, light, fog);
		renderer.shadow_enable_set(true);
		EXPECT_TRUE(renderer.mesh_draw(mesh, translate_model(0.0f, 0.0f, 0.0f).data()));
		EXPECT_TRUE(renderer.frame_render());
		return renderer.frame_stats_get();
	}
}

/*
预算关闭不干预（第六百五十九轮）
口径：budget = 0 → 分辨率保持请求值 1024。
*/
TEST_F(Renderer_Scene_Test, 阴影预算关闭不干预)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
	renderer.texture_bind_set(texture);

	engine::Camera_Data camera{};
	engine::Light_Data light = positive_z_light();
	light.shadow_budget_bytes = 0;
	EXPECT_EQ(render_frame(renderer, mesh, camera, light).shadow_resolution, 1024u);

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

/*
预算折算边长（第六百五十九轮）
口径：单级（1 单位）预算 = 512² × 4 → 边长 512；预算 = 1024² × 4 → 1024；
     双级（2 单位）预算 = 512² × 4 × 2 → 每单位 512² → 边长 512；
     极小预算（256² × 4 以下）取最低支持档 512。
*/
TEST_F(Renderer_Scene_Test, 阴影预算折算边长)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
	renderer.texture_bind_set(texture);
	engine::Camera_Data camera{};

	//单级 512² × 4
	engine::Light_Data light = positive_z_light();
	light.shadow_budget_bytes = 512u * 512u * 4u;
	const uint32_t single = render_frame(renderer, mesh, camera, light).shadow_resolution;
	RecordProperty("budget_single", single);
	EXPECT_EQ(single, 512u);

	//单级 1024² × 4 → 不缩
	light.shadow_budget_bytes = 1024u * 1024u * 4u;
	EXPECT_EQ(render_frame(renderer, mesh, camera, light).shadow_resolution, 1024u);

	//双级：预算翻倍，每单位仍 512²
	light.shadow_cascade_count = 2;
	light.shadow_budget_bytes = 512u * 512u * 4u * 2u;
	const uint32_t dual = render_frame(renderer, mesh, camera, light).shadow_resolution;
	RecordProperty("budget_dual", dual);
	EXPECT_EQ(dual, 512u);

	//极小预算取最低支持档 512（门面仅接受 512 / 1024 / 2048）
	light.shadow_budget_bytes = 256u * 256u * 4u / 2u;
	EXPECT_EQ(render_frame(renderer, mesh, camera, light).shadow_resolution, 512u);

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

/*
扁平入口透传（第六百五十九轮）
口径：Scene_Data.shadow_budget_bytes = 512² × 4 经兼容入口转发后与 Light_Data 路径一致。
*/
TEST_F(Renderer_Scene_Test, 阴影预算扁平入口透传)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
	renderer.texture_bind_set(texture);

	engine::Scene_Data scene{};
	scene.shadow_enabled = 1;
	scene.shadow_budget_bytes = 512u * 512u * 4u;
	engine::Fog_Data fog{};
	renderer.scene_apply(scene);
	renderer.shadow_enable_set(true);
	EXPECT_TRUE(renderer.mesh_draw(mesh, translate_model(0.0f, 0.0f, 0.0f).data()));
	EXPECT_TRUE(renderer.frame_render());
	EXPECT_EQ(renderer.frame_stats_get().shadow_resolution, 512u);

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}
