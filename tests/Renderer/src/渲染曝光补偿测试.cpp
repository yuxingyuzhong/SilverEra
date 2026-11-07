/*
渲染曝光补偿测试（第六百六十轮；深研方向八 A3）
覆盖：默认 EV 0 与显式 0 逐字节一致、+1 EV 变亮、−1 EV 变暗、钳制与非法值忽略、扁平入口透传。
口径：白色纹理四边形（法线 -Z、方向光 +Z、环境光 0）；读取画面中心亮度做单调性断言。
*/
#include "渲染场景测试辅助.h"

namespace
{
	//构造「方向光沿 +Z、环境光 0」的光照组
	engine::Light_Data positive_z_light(void)
	{
		engine::Light_Data light{};
		light.direction[2] = 1.0f;
		light.intensity = 1.0f;
		light.ambient = 0.0f;
		return light;
	}

	//绘制单面并推进一帧，返回画面像素
	std::vector<uint8_t> draw_face(engine::Renderer& renderer, engine::Mesh_Handle mesh)
	{
		std::array<float, 16> identity = {
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f,
		};
		EXPECT_TRUE(renderer.mesh_draw(mesh, identity.data()));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return pixels;
	}
}

/*
默认曝光口径（第六百六十轮）
口径：默认 EV 0 与显式 0 两帧逐字节一致（乘性恒等）。
*/
TEST_F(Renderer_Scene_Test, 曝光默认口径逐字节一致)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
	renderer.texture_bind_set(texture);

	engine::Camera_Data camera{};
	engine::Light_Data light = positive_z_light();
	engine::Fog_Data fog{};
	renderer.scene_apply(camera, light, fog);
	const std::vector<uint8_t> reference = draw_face(renderer, mesh);
	ASSERT_FALSE(reference.empty());

	//显式 0
	light.exposure = 0.0f;
	renderer.scene_apply(camera, light, fog);
	const std::vector<uint8_t> again = draw_face(renderer, mesh);
	EXPECT_TRUE(reference == again) << "显式 EV 0 后画面与默认不一致";

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

/*
曝光单调性（第六百六十轮）
口径：+1 EV 中心亮度高于基准，−1 EV 低于基准（亮度读数单调）。
*/
TEST_F(Renderer_Scene_Test, 曝光单调性)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_gray();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
	renderer.texture_bind_set(texture);
	engine::Camera_Data camera{};
	engine::Fog_Data fog{};

	//基准
	engine::Light_Data light = positive_z_light();
	renderer.scene_apply(camera, light, fog);
	const uint32_t base = center_brightness(draw_face(renderer, mesh));

	//+1 EV
	light.exposure = 1.0f;
	renderer.scene_apply(camera, light, fog);
	const uint32_t brighter = center_brightness(draw_face(renderer, mesh));

	//−1 EV
	light.exposure = -1.0f;
	renderer.scene_apply(camera, light, fog);
	const uint32_t darker = center_brightness(draw_face(renderer, mesh));

	RecordProperty("exposure_base", base);
	RecordProperty("exposure_plus", brighter);
	RecordProperty("exposure_minus", darker);
	EXPECT_GT(brighter, base) << "+1 EV 未变亮";
	EXPECT_LT(darker, base) << "−1 EV 未变暗";

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

/*
曝光钳制与非法值（第六百六十轮）
口径：+10 钳到 +4、−10 钳到 −4；NaN 忽略（保持上次值）；扁平入口透传。
*/
TEST_F(Renderer_Scene_Test, 曝光钳制与非法值)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	ASSERT_TRUE(renderer.exposure_set(10.0f));
	EXPECT_FLOAT_EQ(renderer.exposure_get(), 4.0f);
	ASSERT_TRUE(renderer.exposure_set(-10.0f));
	EXPECT_FLOAT_EQ(renderer.exposure_get(), -4.0f);
	ASSERT_FALSE(renderer.exposure_set(std::nanf("")));
	EXPECT_FLOAT_EQ(renderer.exposure_get(), -4.0f);

	//扁平入口透传
	engine::Scene_Data scene{};
	scene.exposure = 2.0f;
	renderer.scene_apply(scene);
	EXPECT_FLOAT_EQ(renderer.exposure_get(), 2.0f);
}
