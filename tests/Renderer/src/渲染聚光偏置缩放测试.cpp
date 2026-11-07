/*
渲染聚光偏置缩放测试（第六百五十八轮；深研方向八 A2）
覆盖：默认缩放 1 与既有口径一致、数据侧缩放生效、门面钳制与越界拒绝。
口径：1 盏聚光灯（单盏阴影模式）＋ 遮挡面 / 接收面；读取逐瓦片有效偏置做数值断言。
*/
#include "渲染场景测试辅助.h"

namespace
{
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

	//构造「1 盏聚光灯 ＋ 单盏聚光阴影」的光照组
	engine::Light_Data one_spot_light(void)
	{
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
		light.spot_shadow_index = 0;
		return light;
	}

	//推进一帧（绘制遮挡面与接收面）
	void render_once_with(engine::Renderer& renderer, engine::Mesh_Handle occluder, engine::Mesh_Handle receiver)
	{
		EXPECT_TRUE(renderer.mesh_draw(occluder, occluder_model(0.0f, 0.0f, -1.0f).data()));
		EXPECT_TRUE(renderer.mesh_draw(receiver, occluder_model(0.0f, 0.0f, 0.0f).data()));
		EXPECT_TRUE(renderer.frame_render());
	}
}

/*
默认缩放口径（第六百五十八轮）
口径：默认数据（缩放全 1）下逐瓦片偏置等于全局偏置（与引入前一致）。
*/
TEST_F(Renderer_Scene_Test, 聚光偏置缩放默认口径)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle occluder = 0;
	engine::Mesh_Handle receiver = 0;
	ASSERT_TRUE(renderer.mesh_load(occluder, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
	ASSERT_TRUE(renderer.mesh_load(receiver, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
	renderer.texture_bind_set(texture);

	engine::Camera_Data camera{};
	engine::Fog_Data fog{};
	renderer.scene_apply(camera, one_spot_light(), fog);
	render_once_with(renderer, occluder, receiver);

	float base_bias = 0.0f;
	float base_out[8] = {};
	renderer.spot_shadow_params_get(&base_bias, nullptr, nullptr);
	renderer.spot_shadow_bias_tiles_get(base_out);
	ASSERT_GT(base_bias, 0.0f);
	EXPECT_FLOAT_EQ(base_out[0], base_bias) << "默认缩放不是 1（逐瓦片偏置与全局不一致）";

	renderer.mesh_unload(occluder);
	renderer.mesh_unload(receiver);
	renderer.texture_unload(texture);
}

/*
数据侧缩放生效（第六百五十八轮）
口径：spot_shadow_bias_scale[0] = 4 → 逐瓦片偏置放大 4 倍；恢复 1 → 回到基准。
*/
TEST_F(Renderer_Scene_Test, 聚光偏置缩放数据侧生效)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle occluder = 0;
	engine::Mesh_Handle receiver = 0;
	ASSERT_TRUE(renderer.mesh_load(occluder, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
	ASSERT_TRUE(renderer.mesh_load(receiver, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
	renderer.texture_bind_set(texture);
	engine::Camera_Data camera{};
	engine::Fog_Data fog{};

	//基准
	engine::Light_Data light = one_spot_light();
	renderer.scene_apply(camera, light, fog);
	render_once_with(renderer, occluder, receiver);
	float base_bias = 0.0f;
	float tiles[8] = {};
	renderer.spot_shadow_params_get(&base_bias, nullptr, nullptr);

	//缩放 4
	light.spot_shadow_bias_scale[0] = 4.0f;
	renderer.scene_apply(camera, light, fog);
	render_once_with(renderer, occluder, receiver);
	renderer.spot_shadow_bias_tiles_get(tiles);
	RecordProperty("scaled_bias", tiles[0]);
	EXPECT_FLOAT_EQ(tiles[0], base_bias * 4.0f);

	//恢复 1
	light.spot_shadow_bias_scale[0] = 1.0f;
	renderer.scene_apply(camera, light, fog);
	render_once_with(renderer, occluder, receiver);
	renderer.spot_shadow_bias_tiles_get(tiles);
	EXPECT_FLOAT_EQ(tiles[0], base_bias);

	renderer.mesh_unload(occluder);
	renderer.mesh_unload(receiver);
	renderer.texture_unload(texture);
}

/*
门面钳制与越界（第六百五十八轮）
口径：spot_shadow_bias_scale_set(0, 20) 钳到 10；index = 9 返回假且配置不变。
*/
TEST_F(Renderer_Scene_Test, 聚光偏置缩放门面钳制)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle occluder = 0;
	engine::Mesh_Handle receiver = 0;
	ASSERT_TRUE(renderer.mesh_load(occluder, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
	ASSERT_TRUE(renderer.mesh_load(receiver, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
	renderer.texture_bind_set(texture);
	engine::Camera_Data camera{};
	engine::Fog_Data fog{};
	renderer.scene_apply(camera, one_spot_light(), fog);
	render_once_with(renderer, occluder, receiver);
	float base_bias = 0.0f;
	renderer.spot_shadow_params_get(&base_bias, nullptr, nullptr);

	//越界输入钳到 10
	ASSERT_TRUE(renderer.spot_shadow_bias_scale_set(0, 20.0f));
	render_once_with(renderer, occluder, receiver);
	float tiles[8] = {};
	renderer.spot_shadow_bias_tiles_get(tiles);
	EXPECT_FLOAT_EQ(tiles[0], base_bias * 10.0f);

	//越界槽位拒绝且配置不变
	EXPECT_FALSE(renderer.spot_shadow_bias_scale_set(9, 2.0f));
	render_once_with(renderer, occluder, receiver);
	renderer.spot_shadow_bias_tiles_get(tiles);
	EXPECT_FLOAT_EQ(tiles[0], base_bias * 10.0f);

	renderer.mesh_unload(occluder);
	renderer.mesh_unload(receiver);
	renderer.texture_unload(texture);
}
