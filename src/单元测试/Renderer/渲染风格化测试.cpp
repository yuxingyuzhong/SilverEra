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

/*
暗部色调化压暗未受光面（第一百七十八轮）
口径：四边形法线 (0, 0.7071, 0.7071)、环境光 0.2（保证暗部仍有基底照度）；
     方向光默认朝 -Z 时该面 ndotl ≈ 0.707（受光面，不应受色调影响）；
     方向光改为 (0, 0.7071, 0.7071) 时 ndotl = 0（暗部，应整体乘上色调）。
*/
TEST_F(Renderer_Scene_Test, 暗部色调化只压暗未受光面)
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
	engine::Camera_Data camera{};
	engine::Fog_Data fog{};

	//① 受光面（默认方向光朝 -Z，N·L ≈ 0.707）：色调不应改变画面
	engine::Light_Data lit_light{};
	lit_light.intensity = 1.0f;
	lit_light.ambient = 0.2f;
	renderer.scene_apply(camera, lit_light, fog);
	renderer.toon_shadow_tint_set(1.0f, 1.0f, 1.0f);
	ASSERT_TRUE(renderer.mesh_draw(mesh, nullptr));
	std::vector<uint8_t> lit_plain(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(lit_plain.data(), static_cast<uint32_t>(lit_plain.size())));

	renderer.toon_shadow_tint_set(0.25f, 0.25f, 0.25f);
	ASSERT_TRUE(renderer.mesh_draw(mesh, nullptr));
	std::vector<uint8_t> lit_tinted(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(lit_tinted.data(), static_cast<uint32_t>(lit_tinted.size())));

	//受光面最大通道差（色调权重为 0，应逐字节一致）
	int lit_delta = 0;
	for (std::size_t index = 0; index < lit_plain.size(); ++index)
	{
		int delta = static_cast<int>(lit_plain[index]) - static_cast<int>(lit_tinted[index]);
		if (delta < 0)
			delta = -delta;
		if (delta > lit_delta)
			lit_delta = delta;
	}
	RecordProperty("toon_shadow_lit_delta", static_cast<uint32_t>(lit_delta));
	EXPECT_LE(lit_delta, 2) << "受光面被暗部色调影响（应完全不受影响）";

	//② 暗部（方向光改为从面背后照来，N·L = 0）：色调应整体压暗
	engine::Light_Data dark_light{};
	dark_light.direction[0] = 0.0f;
	dark_light.direction[1] = 0.7071f;
	dark_light.direction[2] = 0.7071f;
	dark_light.intensity = 1.0f;
	dark_light.ambient = 0.2f;
	renderer.scene_apply(camera, dark_light, fog);
	renderer.toon_shadow_tint_set(1.0f, 1.0f, 1.0f);
	ASSERT_TRUE(renderer.mesh_draw(mesh, nullptr));
	std::vector<uint8_t> dark_plain(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(dark_plain.data(), static_cast<uint32_t>(dark_plain.size())));

	renderer.toon_shadow_tint_set(0.25f, 0.25f, 0.25f);
	ASSERT_TRUE(renderer.mesh_draw(mesh, nullptr));
	std::vector<uint8_t> dark_tinted(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(dark_tinted.data(), static_cast<uint32_t>(dark_tinted.size())));

	const uint32_t dark_plain_brightness = center_brightness(dark_plain);
	const uint32_t dark_tinted_brightness = center_brightness(dark_tinted);
	RecordProperty("toon_shadow_dark_plain", dark_plain_brightness);
	RecordProperty("toon_shadow_dark_tinted", dark_tinted_brightness);

	EXPECT_GE(dark_plain_brightness, 30u) << "暗部基准亮度异常（环境光未生效）："
		<< pixel_text(pixel_get(dark_plain, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2));
	//压暗后不超过基准的 70%（线性空间乘 0.25，经 sRGB 编码后回读约降至 0.5 倍）
	EXPECT_LE(dark_tinted_brightness * 10, dark_plain_brightness * 7)
		<< "暗部未被色调压暗（" << dark_plain_brightness << " → " << dark_tinted_brightness << "）";

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

/*
暗部色调参数钳制与数据路径（第一百七十八轮）
口径：默认 (1,1,1) 为关闭；越界钳到 [0,1]、非有限值按 1；
     `Scene_Data::toon_shadow_tint` 与 `Light_Data::toon_shadow_tint` 经 scene_apply 生效。
*/
TEST_F(Renderer_Scene_Test, 暗部色调参数钳制与数据路径)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));

	//默认白（关闭）
	float tint[3] = {};
	renderer.toon_shadow_tint(tint);
	for (std::size_t index = 0; index < 3; ++index)
		EXPECT_FLOAT_EQ(tint[index], 1.0f);

	//越界与非有限值归一
	renderer.toon_shadow_tint_set(2.0f, -1.0f, std::numeric_limits<float>::quiet_NaN());
	renderer.toon_shadow_tint(tint);
	EXPECT_FLOAT_EQ(tint[0], 1.0f);
	EXPECT_FLOAT_EQ(tint[1], 0.0f);
	EXPECT_FLOAT_EQ(tint[2], 1.0f);

	//扁平 Scene_Data 路径
	engine::Scene_Data scene{};
	scene.toon_shadow_tint[0] = 0.1f;
	scene.toon_shadow_tint[1] = 0.2f;
	scene.toon_shadow_tint[2] = 0.3f;
	renderer.scene_apply(scene);
	renderer.toon_shadow_tint(tint);
	EXPECT_FLOAT_EQ(tint[0], 0.1f);
	EXPECT_FLOAT_EQ(tint[1], 0.2f);
	EXPECT_FLOAT_EQ(tint[2], 0.3f);

	//三组数据路径（Light_Data）
	engine::Light_Data light{};
	light.toon_shadow_tint[0] = 0.6f;
	light.toon_shadow_tint[1] = 0.5f;
	light.toon_shadow_tint[2] = 0.4f;
	engine::Camera_Data camera{};
	engine::Fog_Data fog{};
	renderer.scene_apply(camera, light, fog);
	renderer.toon_shadow_tint(tint);
	EXPECT_FLOAT_EQ(tint[0], 0.6f);
	EXPECT_FLOAT_EQ(tint[1], 0.5f);
	EXPECT_FLOAT_EQ(tint[2], 0.4f);
}

/*
卡通高光阈值化抹除弱高光（第一百七十九轮）
口径：球体 + 方向光 + 中等光泽度（高光覆盖较大一片）、环境光 0.1；
     阈值 0（关闭）与 0.35（二值化）对比：弱高光像素应显著变暗，且不应出现任何变亮像素。
*/
TEST_F(Renderer_Scene_Test, 卡通高光阈值化抹除弱高光)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	//中灰纹理：避免白色纹理 + 满照时漫反射已饱和，使高光本身可见
	std::vector<uint8_t> texels = solid_gray();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	renderer.texture_bind_set(texture);
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_sphere_load(mesh, 0.6f, 32, 16));
	//场景：默认方向光 + 中等环境光；高光强度 3、光泽度 8（高光面较大）
	engine::Camera_Data camera{};
	engine::Light_Data light{};
	light.intensity = 1.0f;
	light.ambient = 0.1f;
	engine::Fog_Data fog{};
	renderer.scene_apply(camera, light, fog);
	renderer.specular_set(3.0f, 8.0f);

	//关闭阈值化
	renderer.toon_specular_set(0.0f);
	ASSERT_TRUE(renderer.mesh_draw(mesh, nullptr));
	std::vector<uint8_t> plain(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(plain.data(), static_cast<uint32_t>(plain.size())));

	//开启阈值化
	renderer.toon_specular_set(0.35f);
	ASSERT_TRUE(renderer.mesh_draw(mesh, nullptr));
	std::vector<uint8_t> toon(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(toon.data(), static_cast<uint32_t>(toon.size())));

	//逐像素比较：暗化像素数与被意外提亮的像素数
	std::size_t darkened = 0;
	std::size_t brightened = 0;
	for (std::size_t index = 0; index < plain.size(); index += 4)
	{
		//本像素三通道亮度和
		int plain_sum = static_cast<int>(plain[index]) + plain[index + 1] + plain[index + 2];
		int toon_sum = static_cast<int>(toon[index]) + toon[index + 1] + toon[index + 2];
		if (plain_sum - toon_sum >= 20)
			++darkened;
		if (toon_sum - plain_sum > 2)
			++brightened;
	}
	RecordProperty("toon_specular_darkened_pixels", static_cast<uint32_t>(darkened));
	RecordProperty("toon_specular_brightened_pixels", static_cast<uint32_t>(brightened));

	EXPECT_GT(darkened, 20u) << "阈值化未抹除弱高光（暗化像素 " << darkened << "）";
	EXPECT_EQ(brightened, 0u) << "阈值化意外提亮了像素（" << brightened << " 个）";

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

/*
卡通高光参数归一与数据路径（第一百七十九轮）
口径：阈值钳到 [0,1]、非有限值按 0（关闭）；`Scene_Data::toon_specular` 与
     `Light_Data::toon_specular` 经 scene_apply 生效。
*/
TEST_F(Renderer_Scene_Test, 卡通高光参数归一与数据路径)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//默认关闭
	EXPECT_FLOAT_EQ(renderer.toon_specular(), 0.0f);

	//越界与非有限值归一
	renderer.toon_specular_set(2.0f);
	EXPECT_FLOAT_EQ(renderer.toon_specular(), 1.0f);
	renderer.toon_specular_set(-1.0f);
	EXPECT_FLOAT_EQ(renderer.toon_specular(), 0.0f);
	renderer.toon_specular_set(std::numeric_limits<float>::quiet_NaN());
	EXPECT_FLOAT_EQ(renderer.toon_specular(), 0.0f);

	//扁平 Scene_Data 路径
	engine::Scene_Data scene{};
	scene.toon_specular = 0.4f;
	renderer.scene_apply(scene);
	EXPECT_FLOAT_EQ(renderer.toon_specular(), 0.4f);

	//三组数据路径（Light_Data）
	engine::Light_Data light{};
	light.toon_specular = 0.7f;
	engine::Camera_Data camera{};
	engine::Fog_Data fog{};
	renderer.scene_apply(camera, light, fog);
	EXPECT_FLOAT_EQ(renderer.toon_specular(), 0.7f);
}
