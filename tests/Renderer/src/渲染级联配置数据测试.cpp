/*
渲染级联配置数据测试（第六百五十四轮；深研方向八 A2 数据化）
覆盖：Light_Data 的级联数 / 远区倍数 / 切换距离经 scene_apply 生效、非法值忽略、
     默认配置不触发阴影重建（静止复用时序不变）、扁平入口透传。
*/
#include "渲染场景测试辅助.h"

namespace
{
	//构造空雾
	engine::Fog_Data empty_fog(void)
	{
		engine::Fog_Data fog{};
		fog.density = 0.0f;
		return fog;
	}
}

/*
数据侧级联配置（第六百五十四轮）
口径：默认（1 / 3 / 12）与初始一致；设为 3 级 / 2.5 / 8 后门面回读一致；
     非法级联数（4）被 setter 忽略（保持 3）；恢复默认后回到（1 / 3 / 12）。
*/
TEST_F(Renderer_Scene_Test, 级联配置数据侧生效)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Camera_Data camera{};
	engine::Light_Data light{};

	//默认与初始一致
	renderer.scene_apply(camera, light, empty_fog());
	uint32_t count = 0;
	float far_scale = 0.0f;
	float split = 0.0f;
	renderer.shadow_cascade_get(&count, &far_scale, &split);
	EXPECT_EQ(count, 1u);
	EXPECT_FLOAT_EQ(far_scale, 3.0f);
	EXPECT_FLOAT_EQ(split, 12.0f);

	//设为 3 级 / 2.5 / 8
	light.shadow_cascade_count = 3;
	light.shadow_cascade_far_scale = 2.5f;
	light.shadow_cascade_split = 8.0f;
	renderer.scene_apply(camera, light, empty_fog());
	renderer.shadow_cascade_get(&count, &far_scale, &split);
	EXPECT_EQ(count, 3u);
	EXPECT_FLOAT_EQ(far_scale, 2.5f);
	EXPECT_FLOAT_EQ(split, 8.0f);

	//非法级联数被忽略
	light.shadow_cascade_count = 4;
	renderer.scene_apply(camera, light, empty_fog());
	renderer.shadow_cascade_get(&count, &far_scale, &split);
	EXPECT_EQ(count, 3u) << "非法级联数未被忽略";

	//恢复默认（1 级）
	light.shadow_cascade_count = 1;
	light.shadow_cascade_far_scale = 3.0f;
	light.shadow_cascade_split = 12.0f;
	renderer.scene_apply(camera, light, empty_fog());
	renderer.shadow_cascade_get(&count, &far_scale, &split);
	EXPECT_EQ(count, 1u);
	EXPECT_FLOAT_EQ(far_scale, 3.0f);
	EXPECT_FLOAT_EQ(split, 12.0f);
}

/*
默认配置不触发阴影重建（第六百五十四轮）
口径：默认级联字段连续两次 scene_apply 后绘制两帧：第二帧阴影 pass 归 0（复用时序未被破坏）。
*/
TEST_F(Renderer_Scene_Test, 级联默认配置不破坏复用)
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
	engine::Light_Data light{};
	renderer.scene_apply(camera, light, empty_fog());
	renderer.shadow_enable_set(true);
	std::array<float, 16> model = {
		0.6f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.6f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.6f, 0.0f,
		0.0f, 0.0f, -1.0f, 1.0f,
	};
	//首帧重绘
	EXPECT_TRUE(renderer.mesh_draw(occluder, model.data()));
	EXPECT_TRUE(renderer.frame_render());
	const uint32_t first_passes = renderer.frame_stats_get().shadow_passes;

	//再次应用默认数据（值未变 → 不应重建）后绘制
	renderer.scene_apply(camera, light, empty_fog());
	renderer.shadow_enable_set(true);
	EXPECT_TRUE(renderer.mesh_draw(occluder, model.data()));
	EXPECT_TRUE(renderer.frame_render());
	EXPECT_EQ(first_passes, 1u);
	EXPECT_EQ(renderer.frame_stats_get().shadow_passes, 0u) << "默认级联字段触发了阴影重建";

	renderer.mesh_unload(occluder);
	renderer.texture_unload(texture);
}

/*
扁平入口透传（第六百五十四轮）
口径：Scene_Data.shadow_cascade_count = 2 经兼容入口转发后与 Light_Data 路径一致。
*/
TEST_F(Renderer_Scene_Test, 级联配置扁平入口透传)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Scene_Data scene{};
	scene.shadow_cascade_count = 2;
	scene.shadow_cascade_far_scale = 3.0f;
	scene.shadow_cascade_split = 12.0f;
	renderer.scene_apply(scene);
	uint32_t count = 0;
	float far_scale = 0.0f;
	float split = 0.0f;
	renderer.shadow_cascade_get(&count, &far_scale, &split);
	EXPECT_EQ(count, 2u) << "扁平入口未透传级联数";
}
