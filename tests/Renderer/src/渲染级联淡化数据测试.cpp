/*
渲染级联淡化数据测试（第六百五十一轮；深研方向八 A2 / A3 数据化）
覆盖：Light_Data.shadow_cascade_blend 经 scene_apply 生效、默认 0 不变、非法值忽略、
     Scene_Data 扁平入口透传。
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
数据侧淡化比例（第六百五十一轮）
口径：默认 0；设为 0.3 后门面回读一致；非法 1.0 被忽略（保持 0.3）；恢复 0 后归零。
*/
TEST_F(Renderer_Scene_Test, 级联淡化数据侧生效)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Camera_Data camera{};
	engine::Light_Data light{};
	//默认 0
	EXPECT_FLOAT_EQ(renderer.shadow_cascade_blend(), 0.0f);
	renderer.scene_apply(camera, light, empty_fog());
	EXPECT_FLOAT_EQ(renderer.shadow_cascade_blend(), 0.0f) << "默认场景不应改变淡化比例";

	//0.3 生效
	light.shadow_cascade_blend = 0.3f;
	renderer.scene_apply(camera, light, empty_fog());
	EXPECT_FLOAT_EQ(renderer.shadow_cascade_blend(), 0.3f);

	//非法值（> 0.5）被 setter 忽略
	light.shadow_cascade_blend = 1.0f;
	renderer.scene_apply(camera, light, empty_fog());
	EXPECT_FLOAT_EQ(renderer.shadow_cascade_blend(), 0.3f) << "非法淡化比例未被忽略";

	//恢复 0
	light.shadow_cascade_blend = 0.0f;
	renderer.scene_apply(camera, light, empty_fog());
	EXPECT_FLOAT_EQ(renderer.shadow_cascade_blend(), 0.0f);
}

/*
扁平入口透传（第六百五十一轮）
口径：Scene_Data.shadow_cascade_blend = 0.2 经兼容入口转发后与 Light_Data 路径一致。
*/
TEST_F(Renderer_Scene_Test, 级联淡化扁平入口透传)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Scene_Data scene{};
	scene.shadow_cascade_blend = 0.2f;
	renderer.scene_apply(scene);
	EXPECT_FLOAT_EQ(renderer.shadow_cascade_blend(), 0.2f) << "扁平入口未透传淡化比例";
}
