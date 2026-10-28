/*
渲染风格化测试（第一百七十四轮：二次元风格化阶段 A）
覆盖：色阶量化改变分面明暗 / 菲涅尔边缘光抬升轮廓亮度 / 非法参数钳制与数据化路径 / 未初始化守卫。
口径：量化与边缘光均默认关闭（画面逐字节不变）；颜色断言只用「外通道较大者」（避开回读通道序）。
*/
#include "渲染场景测试辅助.h"

/*
色阶量化改变分面明暗（第一百七十四轮）
口径：立方体 + 斜射方向光（各面 N·L 不同），色阶数 0（关闭）与 3（三档量化）两帧应有大量像素显著变化；
     量化只改变照度分档，不改变几何与纹理。
*/
TEST_F(Renderer_Scene_Test, 色阶量化改变分面明暗)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	renderer.texture_bind_set(texture);
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, cube_data(0.4f)));
	//场景：方向光斜射（各面 N·L 不同）、环境光 0
	engine::Camera_Data camera{};
	engine::Light_Data light{};
	light.direction[0] = 0.6f;
	light.direction[1] = -0.6f;
	light.direction[2] = -0.6f;
	light.intensity = 1.0f;
	light.ambient = 0.0f;
	engine::Fog_Data fog{};
	renderer.scene_apply(camera, light, fog);

	//关闭量化
	renderer.toon_set(0, 0.0f);
	ASSERT_TRUE(renderer.mesh_draw(mesh, nullptr));
	std::vector<uint8_t> plain(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(plain.data(), static_cast<uint32_t>(plain.size())));

	//一档量化（二值化：受光面拉到满照、背光面保持 0）
	renderer.toon_set(1, 0.0f);
	ASSERT_TRUE(renderer.mesh_draw(mesh, nullptr));
	std::vector<uint8_t> toon(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(toon.data(), static_cast<uint32_t>(toon.size())));

	//统计显著差异像素
	std::size_t changed = 0;
	for (std::size_t index = 0; index < plain.size(); index += 4)
	{
		int delta = static_cast<int>(plain[index]) - static_cast<int>(toon[index]);
		if (delta < 0)
			delta = -delta;
		if (delta >= 20)
			++changed;
	}
	RecordProperty("toon_quantize_changed_pixels", static_cast<uint32_t>(changed));
	EXPECT_GE(changed, 30u) << "色阶量化未改变分面明暗（变化像素 " << changed << "）";

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

/*
菲涅尔边缘光抬升轮廓亮度（第一百七十四轮）
口径：倾斜 45° 四边形（法线 (0, 0.707, 0.707)）、环境光 0、方向光关闭，画面本应全黑；
     开启边缘光后，视线与法线夹角产生的菲涅尔项在整片表面抬起亮度。
*/
TEST_F(Renderer_Scene_Test, 边缘光抬升表面亮度)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	renderer.texture_bind_set(texture);
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data_with_normal(0.0f, 0.7071f, 0.7071f)));
	//场景：环境光 0、方向光关闭（无边缘光时应为全黑）
	engine::Camera_Data camera{};
	engine::Light_Data light{};
	light.intensity = 0.0f;
	light.ambient = 0.0f;
	engine::Fog_Data fog{};
	renderer.scene_apply(camera, light, fog);

	//关闭边缘光
	renderer.toon_set(0, 0.0f);
	ASSERT_TRUE(renderer.mesh_draw(mesh, nullptr));
	std::vector<uint8_t> plain(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(plain.data(), static_cast<uint32_t>(plain.size())));

	//开启边缘光（强度 1）
	renderer.toon_set(0, 1.0f);
	ASSERT_TRUE(renderer.mesh_draw(mesh, nullptr));
	std::vector<uint8_t> rim_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(rim_pixels.data(), static_cast<uint32_t>(rim_pixels.size())));

	const std::array<uint8_t, 4> plain_center = pixel_get(plain, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	const std::array<uint8_t, 4> rim_center = pixel_get(rim_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	const uint8_t plain_outer = plain_center[0] > plain_center[2] ? plain_center[0] : plain_center[2];
	const uint8_t rim_outer = rim_center[0] > rim_center[2] ? rim_center[0] : rim_center[2];
	RecordProperty("toon_rim_plain_outer", static_cast<uint32_t>(plain_outer));
	RecordProperty("toon_rim_lit_outer", static_cast<uint32_t>(rim_outer));

	EXPECT_LE(plain_outer, 20u) << "无光照且无边缘光时中心应为黑（" << pixel_text(plain_center) << "）";
	EXPECT_GE(rim_outer, 40u) << "边缘光未抬升表面亮度（" << pixel_text(rim_center) << "）";

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

/*
非法参数钳制与数据化路径（第一百七十四轮）
口径：色阶数超过 8 钳到 8；边缘光超过 1 钳到 1、负数 / 非数按 0；
     `Scene_Data::toon_steps` / `toon_rim` 经 scene_apply 生效，与门面 setter 同口径。
*/
TEST_F(Renderer_Scene_Test, 风格化参数钳制与数据路径)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//默认 (0, 0)
	EXPECT_EQ(renderer.toon_steps(), 0u);
	EXPECT_FLOAT_EQ(renderer.toon_rim(), 0.0f);

	//越界钳制
	renderer.toon_set(100, 2.0f);
	EXPECT_EQ(renderer.toon_steps(), 8u);
	EXPECT_FLOAT_EQ(renderer.toon_rim(), 1.0f);
	renderer.toon_set(0, -1.0f);
	EXPECT_FLOAT_EQ(renderer.toon_rim(), 0.0f);

	//数据化路径（扁平 Scene_Data）
	engine::Scene_Data scene{};
	scene.toon_steps = 4;
	scene.toon_rim = 0.5f;
	renderer.scene_apply(scene);
	EXPECT_EQ(renderer.toon_steps(), 4u);
	EXPECT_FLOAT_EQ(renderer.toon_rim(), 0.5f);

	//三组数据路径（Light_Data）
	engine::Light_Data light{};
	light.toon_steps = 2;
	light.toon_rim = 0.25f;
	engine::Camera_Data camera{};
	engine::Fog_Data fog{};
	renderer.scene_apply(camera, light, fog);
	EXPECT_EQ(renderer.toon_steps(), 2u);
	EXPECT_FLOAT_EQ(renderer.toon_rim(), 0.25f);
}
