/*
渲染阴影纹素钳制与自动淡化测试（第六百六十一轮；深研方向八 B1 剩余课题）
覆盖：纹素上限默认不干预 / 钳制后纹素不超过上限且覆盖面收缩 / 级联切换带自动淡化建议 /
     显式比例优先与自动关闭 / 扁平入口透传。
口径：方向光沿 +Z、环境光 0；遮挡面 z = -10、接收面 z = -9（间距 1.0，避开深度偏置的等效世界偏移）。
*/
#include "渲染场景测试辅助.h"

namespace
{
	//构造沿 X、Y、Z 平移的模型矩阵（列主序）
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

	//绘制固定场景并推进一帧（返回画面像素；遮挡面 z = -10、接收面 z = -9，间距 1.0）
	std::vector<uint8_t> draw_scene(engine::Renderer& renderer, engine::Mesh_Handle occluder, engine::Mesh_Handle receiver)
	{
		EXPECT_TRUE(renderer.mesh_draw(occluder, occluder_model(0.0f, 0.0f, -10.0f).data()));
		EXPECT_TRUE(renderer.mesh_draw(receiver, translate_model(0.0f, 0.0f, -9.0f).data()));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return pixels;
	}
}

/*
纹素上限默认不干预（第六百六十一轮）
口径：shadow_texel_max = 0（默认）时纹素 = 2 × 半宽 / 边长；钳到 0.05 后再关回 0，
     画面与统计完全回到默认（逐字节一致）。
*/
TEST_F(Renderer_Scene_Test, 纹素上限默认不干预)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle receiver = 0;
	engine::Mesh_Handle occluder = 0;
	ASSERT_TRUE(renderer.mesh_load(receiver, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
	ASSERT_TRUE(renderer.mesh_load(occluder, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
	renderer.texture_bind_set(texture);

	//单级、半宽 48、边长 512（纹素 = 96 / 512 = 0.1875）
	renderer.shadow_range_set(48.0f, 1.0f, 60.0f);
	engine::Camera_Data camera{};
	engine::Light_Data light = positive_z_light();
	light.shadow_resolution = 512;
	engine::Fog_Data fog{};
	renderer.scene_apply(camera, light, fog);
	renderer.shadow_enable_set(true);
	const std::vector<uint8_t> reference = draw_scene(renderer, occluder, receiver);
	const engine::Frame_Stats base = renderer.frame_stats_get();
	EXPECT_FLOAT_EQ(base.shadow_texel_world, 0.1875f) << "默认纹素世界尺寸不等于 2 × 半宽 / 边长";
	EXPECT_FLOAT_EQ(base.cascade_blend_applied, 0.0f) << "默认级联淡化比例应为 0";

	//钳到 0.05：纹素被收紧
	light.shadow_texel_max = 0.05f;
	renderer.scene_apply(camera, light, fog);
	renderer.shadow_enable_set(true);
	draw_scene(renderer, occluder, receiver);
	EXPECT_FLOAT_EQ(renderer.frame_stats_get().shadow_texel_world, 0.05f) << "钳制后的纹素未等于上限";

	//关回 0：画面与统计回到默认
	light.shadow_texel_max = 0.0f;
	renderer.scene_apply(camera, light, fog);
	renderer.shadow_enable_set(true);
	const std::vector<uint8_t> again = draw_scene(renderer, occluder, receiver);
	EXPECT_FLOAT_EQ(renderer.frame_stats_get().shadow_texel_world, 0.1875f) << "关闭钳制后纹素未回到默认";
	EXPECT_TRUE(reference == again) << "关闭纹素钳制后画面与默认不一致";

	renderer.mesh_unload(occluder);
	renderer.mesh_unload(receiver);
	renderer.texture_unload(texture);
}

/*
纹素钳制覆盖面收缩（第六百六十一轮）
口径：半宽 48 / 边长 512 下把纹素上限压到 0.02（半径收到 5.12），遮挡面（z = -10）
     落到视锥外，画面中心由阴影变为受光（亮度上升）。
*/
TEST_F(Renderer_Scene_Test, 纹素钳制覆盖面收缩)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_gray();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle receiver = 0;
	engine::Mesh_Handle occluder = 0;
	ASSERT_TRUE(renderer.mesh_load(receiver, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
	ASSERT_TRUE(renderer.mesh_load(occluder, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
	renderer.texture_bind_set(texture);

	renderer.shadow_range_set(48.0f, 1.0f, 60.0f);
	engine::Camera_Data camera{};
	engine::Light_Data light = positive_z_light();
	light.shadow_resolution = 512;
	engine::Fog_Data fog{};

	//基准：大半径覆盖遮挡面 → 中心为阴影
	renderer.scene_apply(camera, light, fog);
	renderer.shadow_enable_set(true);
	const uint32_t base = center_brightness(draw_scene(renderer, occluder, receiver));

	//钳到 0.02 → 半径 5.12，遮挡面在视锥外 → 中心受光
	light.shadow_texel_max = 0.02f;
	renderer.scene_apply(camera, light, fog);
	renderer.shadow_enable_set(true);
	const uint32_t lit = center_brightness(draw_scene(renderer, occluder, receiver));
	EXPECT_FLOAT_EQ(renderer.frame_stats_get().shadow_texel_world, 0.02f) << "钳制后的纹素未等于上限";
	RecordProperty("texel_base_brightness", base);
	RecordProperty("texel_clamp_brightness", lit);
	EXPECT_GT(lit, base) << "钳制后覆盖面未收缩（遮挡面仍在视锥内）";

	renderer.mesh_unload(occluder);
	renderer.mesh_unload(receiver);
	renderer.texture_unload(texture);
}

/*
级联切换带自动淡化建议（第六百六十一轮）
口径：2 级 / 边长 1024（分片 512）时建议比例 = 4 / 512 = 0.0078；
     显式比例 0.05 时以显式值为准；自动关闭时为 0。
*/
TEST_F(Renderer_Scene_Test, 级联切换带自动淡化建议)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	renderer.shadow_resolution_set(1024);
	engine::Camera_Data camera{};
	engine::Light_Data light = positive_z_light();
	light.shadow_cascade_count = 2;
	light.shadow_cascade_blend_auto = 1;
	engine::Fog_Data fog{};

	//自动 + 显式 0 → 建议值
	renderer.scene_apply(camera, light, fog);
	renderer.frame_render();
	EXPECT_FLOAT_EQ(renderer.frame_stats_get().cascade_blend_applied, 0.0078125f) << "自动淡化建议值不等于 4 / 分片边长";

	//自动 + 显式 0.05 → 显式优先
	light.shadow_cascade_blend = 0.05f;
	renderer.scene_apply(camera, light, fog);
	renderer.frame_render();
	EXPECT_FLOAT_EQ(renderer.frame_stats_get().cascade_blend_applied, 0.05f) << "显式淡化比例未优先";

	//自动关闭 + 显式 0 → 0
	light.shadow_cascade_blend = 0.0f;
	light.shadow_cascade_blend_auto = 0;
	renderer.scene_apply(camera, light, fog);
	renderer.frame_render();
	EXPECT_FLOAT_EQ(renderer.frame_stats_get().cascade_blend_applied, 0.0f) << "自动关闭后仍未回到硬切换";
}

/*
扁平入口透传（第六百六十一轮）
口径：Scene_Data 的 shadow_texel_max / shadow_cascade_blend_auto 经 scene_apply 生效；
     2 级 / 边长 1024 下纹素被钳到 0.05、淡化比例取自动建议。
*/
TEST_F(Renderer_Scene_Test, 纹素钳制与自动淡化扁平透传)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Scene_Data scene{};
	scene.shadow_resolution = 1024;
	scene.shadow_cascade_count = 2;
	scene.shadow_texel_max = 0.05f;
	scene.shadow_cascade_blend_auto = 1;
	renderer.scene_apply(scene);
	EXPECT_FLOAT_EQ(renderer.shadow_texel_max(), 0.05f);
	EXPECT_TRUE(renderer.shadow_cascade_blend_auto());
	renderer.frame_render();
	const engine::Frame_Stats stats = renderer.frame_stats_get();
	EXPECT_FLOAT_EQ(stats.shadow_texel_world, 0.05f) << "扁平入口的纹素上限未生效";
	EXPECT_FLOAT_EQ(stats.cascade_blend_applied, 0.0078125f) << "扁平入口的自动淡化未生效";
}
