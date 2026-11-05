/*
渲染聚光逐盏阴影测试（第六百四十八轮；深研方向八 A2 数据化）
覆盖：逐盏聚光阴影开关（Light_Data.spot_shadow_on 与门面 spot_shadow_light_set）、
     关闭某盏后瓦片 / pass 数下降、重新开启恢复、越界拒绝、默认全开逐字节不变。
口径：2 盏聚光灯 ＋ 多盏聚光阴影 ＋ 方向光阴影关闭；瓦片数取 Frame_Stats.spot_shadow_tiles。
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

	//构造「2 盏聚光灯 ＋ 多盏聚光阴影 ＋ 方向光阴影关闭」的光照组
	engine::Light_Data two_spot_light(void)
	{
		engine::Light_Data light{};
		light.spot_count = 2;
		for (uint32_t index = 0; index < 2; ++index)
		{
			light.spot_position[index][0] = index == 0 ? -1.0f : 1.0f;
			light.spot_position[index][1] = 2.0f;
			light.spot_position[index][2] = 3.0f;
			light.spot_direction[index][1] = -1.0f;
			light.spot_direction[index][2] = 0.0f;
			light.spot_radius[index] = 10.0f;
			light.spot_intensity[index] = 1.0f;
			light.spot_inner_deg[index] = 15.0f;
			light.spot_outer_deg[index] = 30.0f;
		}
		light.spot_shadow_enabled = 1;
		light.spot_shadow_all = 1;
		light.spot_shadow_index = 0;
		return light;
	}

	//推进一帧并返回本帧参与投影的聚光瓦片数
	uint32_t render_tiles(engine::Renderer& renderer, engine::Mesh_Handle occluder)
	{
		EXPECT_TRUE(renderer.mesh_draw(occluder, occluder_model(0.0f, 0.0f, -1.0f).data()));
		EXPECT_TRUE(renderer.frame_render());
		return renderer.frame_stats_get().spot_shadow_tiles;
	}
}

/*
默认全开：2 盏聚光灯各占瓦片（第六百四十八轮）
口径：默认 spot_shadow_on 全为 1 → 2 张瓦片、2 次聚光 pass（与引入该字段前一致）。
*/
TEST_F(Renderer_Scene_Test, 聚光逐盏阴影默认全开)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle occluder = 0;
	ASSERT_TRUE(renderer.mesh_load(occluder, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
	renderer.texture_bind_set(texture);

	engine::Camera_Data camera{};
	engine::Fog_Data fog{};
	renderer.scene_apply(camera, two_spot_light(), fog);
	const uint32_t tiles = render_tiles(renderer, occluder);
	const engine::Frame_Stats stats = renderer.frame_stats_get();
	RecordProperty("default_tiles", tiles);
	RecordProperty("default_spot_passes", stats.spot_shadow_passes);
	EXPECT_EQ(tiles, 2u) << "默认全开时应有 2 张聚光瓦片";
	EXPECT_EQ(stats.spot_shadow_passes, 2u) << "默认全开时应有 2 次聚光 pass";

	renderer.mesh_unload(occluder);
	renderer.texture_unload(texture);
}

/*
数据侧逐盏关闭与恢复（第六百四十八轮）
口径：spot_shadow_on[0] = 0 → 只剩 1 张瓦片；恢复为 1 后回到 2 张；其余盏不受影响。
*/
TEST_F(Renderer_Scene_Test, 聚光逐盏阴影数据侧开关)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle occluder = 0;
	ASSERT_TRUE(renderer.mesh_load(occluder, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
	renderer.texture_bind_set(texture);
	engine::Camera_Data camera{};
	engine::Fog_Data fog{};

	//关闭第 1 盏的阴影
	engine::Light_Data light = two_spot_light();
	light.spot_shadow_on[0] = 0;
	renderer.scene_apply(camera, light, fog);
	uint32_t tiles = render_tiles(renderer, occluder);
	RecordProperty("disabled_tiles", tiles);
	EXPECT_EQ(tiles, 1u) << "关闭 1 盏后应只剩 1 张瓦片";
	EXPECT_EQ(renderer.frame_stats_get().spot_shadow_passes, 1u);

	//恢复全开
	light.spot_shadow_on[0] = 1;
	renderer.scene_apply(camera, light, fog);
	tiles = render_tiles(renderer, occluder);
	RecordProperty("restored_tiles", tiles);
	EXPECT_EQ(tiles, 2u) << "恢复后应回到 2 张瓦片";

	renderer.mesh_unload(occluder);
	renderer.texture_unload(texture);
}

/*
门面逐盏开关与越界拒绝（第六百四十八轮）
口径：spot_shadow_light_set(0, false) → 1 张瓦片；再置真 → 2 张；index ≥ 8 返回假且配置不变。
*/
TEST_F(Renderer_Scene_Test, 聚光逐盏阴影门面开关)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle occluder = 0;
	ASSERT_TRUE(renderer.mesh_load(occluder, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
	renderer.texture_bind_set(texture);
	engine::Camera_Data camera{};
	engine::Fog_Data fog{};
	renderer.scene_apply(camera, two_spot_light(), fog);

	//经门面关闭第 1 盏
	ASSERT_TRUE(renderer.spot_shadow_light_set(0, false));
	EXPECT_EQ(render_tiles(renderer, occluder), 1u);

	//重新开启
	ASSERT_TRUE(renderer.spot_shadow_light_set(0, true));
	EXPECT_EQ(render_tiles(renderer, occluder), 2u);

	//越界槽位拒绝且配置不变
	EXPECT_FALSE(renderer.spot_shadow_light_set(9, false));
	EXPECT_EQ(render_tiles(renderer, occluder), 2u);

	renderer.mesh_unload(occluder);
	renderer.texture_unload(texture);
}
