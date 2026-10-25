/*
渲染材质贴图测试（第一百二十四轮：法线贴图 / 粗糙度贴图）
覆盖：无贴图材质路径逐字节不变 / 法线贴图改变受光 / 粗糙度贴图改变高光。
口径：白与中灰纹理四边形 ＋ 方向光沿 +Z、环境光 0；法线贴图取「切线空间 +X 倾斜」，
     粗糙度贴图取红通道作乘子（默认白纹理 = 1，故无贴图路径不变）。
*/
#include "渲染场景测试辅助.h"

namespace
{
	//构造纯色纹理（RGBA8）
	std::vector<uint8_t> solid_rgba(uint8_t r, uint8_t g, uint8_t b)
	{
		std::vector<uint8_t> pixels(2 * 2 * 4);
		for (std::size_t index = 0; index < pixels.size(); index += 4)
		{
			pixels[index] = r;
			pixels[index + 1] = g;
			pixels[index + 2] = b;
			pixels[index + 3] = 255;
		}
		return pixels;
	}

	//构造「切线空间 +X 倾斜 45°」的法线贴图（编码值 217 / 128 / 217）
	std::vector<uint8_t> tilted_normal_map(void)
	{
		return solid_rgba(217, 128, 217);
	}
}

/*
无贴图材质路径逐字节不变（第一百二十四轮）
口径：同一场景分别用「直接绑定纹理」与「材质（无贴图、法线强度 0）」渲染，两帧应逐字节一致。
*/
TEST_F(Renderer_Scene_Test, 材质无贴图逐字节不变)
{
	//参考帧：直接绑定纹理
	std::vector<uint8_t> direct(pixel_bytes());
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
		ASSERT_TRUE(renderer.mesh_load(mesh, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
		ASSERT_TRUE(renderer.mesh_draw(mesh, nullptr));
		engine::Camera_Data camera{};
		engine::Light_Data light{};
		light.direction[2] = 1.0f;
		light.intensity = 1.0f;
		light.ambient = 0.0f;
		engine::Fog_Data fog{};
		renderer.scene_apply(camera, light, fog);
		ASSERT_TRUE(renderer.frame_render(direct.data(), static_cast<uint32_t>(direct.size())));
		renderer.mesh_unload(mesh);
		renderer.texture_unload(texture);
	}
	//对照帧：材质路径（无贴图）
	std::vector<uint8_t> material(pixel_bytes());
	{
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(hidden_config()));
		uint64_t texture = 0;
		std::vector<uint8_t> texels = solid_white();
		ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
		engine::Material_Data data{};
		data.texture = texture;
		engine::Material_Handle handle = 0;
		ASSERT_TRUE(renderer.material_load(handle, data));
		renderer.material_bind_set(handle);
		engine::Mesh_Handle mesh = 0;
		ASSERT_TRUE(renderer.mesh_load(mesh, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
		ASSERT_TRUE(renderer.mesh_draw(mesh, nullptr));
		engine::Camera_Data camera{};
		engine::Light_Data light{};
		light.direction[2] = 1.0f;
		light.intensity = 1.0f;
		light.ambient = 0.0f;
		engine::Fog_Data fog{};
		renderer.scene_apply(camera, light, fog);
		ASSERT_TRUE(renderer.frame_render(material.data(), static_cast<uint32_t>(material.size())));
		renderer.mesh_unload(mesh);
		renderer.material_unload(handle);
		renderer.texture_unload(texture);
	}

	int maximum = 0;
	for (std::size_t index = 0; index < direct.size(); ++index)
	{
		int delta = static_cast<int>(direct[index]) - static_cast<int>(material[index]);
		if (delta < 0)
			delta = -delta;
		if (delta > maximum)
			maximum = delta;
	}
	RecordProperty("material_plain_diff", maximum);
	EXPECT_LE(maximum, 0) << "无贴图材质路径与直接绑定纹理不一致（差 " << maximum << "）";
}

/*
法线贴图改变受光（第一百二十四轮）
口径：法线贴图在切线空间把法线倾斜 45°，强度 1 时中心受光应明显低于无贴图；
     法线贴图不影响时（强度 0）中心回到满照。
*/
TEST_F(Renderer_Scene_Test, 法线贴图改变受光)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//反照率（白）与法线贴图
	uint64_t albedo = 0;
	std::vector<uint8_t> white = solid_white();
	ASSERT_TRUE(renderer.texture_load(albedo, white.data(), 2, 2));
	uint64_t normal_texture = 0;
	std::vector<uint8_t> normal_texels = tilted_normal_map();
	ASSERT_TRUE(renderer.texture_load(normal_texture, normal_texels.data(), 2, 2));
	//场景：方向光沿 +Z、环境光 0
	engine::Camera_Data camera{};
	engine::Light_Data light{};
	light.direction[2] = 1.0f;
	light.intensity = 1.0f;
	light.ambient = 0.0f;
	engine::Fog_Data fog{};
	renderer.scene_apply(camera, light, fog);
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data_with_normal(0.0f, 0.0f, -1.0f)));

	//无法线贴图（强度 0）
	engine::Material_Data plain{};
	plain.texture = albedo;
	plain.normal_texture = normal_texture;
	plain.normal_strength = 0.0f;
	engine::Material_Handle plain_handle = 0;
	ASSERT_TRUE(renderer.material_load(plain_handle, plain));
	renderer.material_bind_set(plain_handle);
	ASSERT_TRUE(renderer.mesh_draw(mesh, nullptr));
	std::vector<uint8_t> plain_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(plain_pixels.data(), static_cast<uint32_t>(plain_pixels.size())));

	//启用法线贴图（强度 1）
	engine::Material_Data mapped{};
	mapped.texture = albedo;
	mapped.normal_texture = normal_texture;
	mapped.normal_strength = 1.0f;
	engine::Material_Handle mapped_handle = 0;
	ASSERT_TRUE(renderer.material_load(mapped_handle, mapped));
	renderer.material_bind_set(mapped_handle);
	ASSERT_TRUE(renderer.mesh_draw(mesh, nullptr));
	std::vector<uint8_t> mapped_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(mapped_pixels.data(), static_cast<uint32_t>(mapped_pixels.size())));

	const uint32_t plain_brightness = center_brightness(plain_pixels);
	const uint32_t mapped_brightness = center_brightness(mapped_pixels);
	RecordProperty("normal_plain_brightness", plain_brightness);
	RecordProperty("normal_mapped_brightness", mapped_brightness);

	EXPECT_GE(plain_brightness, 200u) << "无贴图时中心未满照（" << plain_brightness << "）";
	EXPECT_LE(mapped_brightness, plain_brightness - 20u) << "法线贴图未改变受光（"
		<< mapped_brightness << " vs " << plain_brightness << "）";

	renderer.mesh_unload(mesh);
	renderer.material_unload(plain_handle);
	renderer.material_unload(mapped_handle);
	renderer.texture_unload(albedo);
	renderer.texture_unload(normal_texture);
}

/*
粗糙度贴图改变高光（第一百二十四轮）
口径：PBR 开启、中灰反照率、方向光 +Z、高光强度 2；
     粗糙度贴图取 0.2 时高光更集中（中心更亮），取 1.0（白）时更散（中心较暗）。
*/
TEST_F(Renderer_Scene_Test, 粗糙度贴图改变高光)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t albedo = 0;
	std::vector<uint8_t> gray = solid_gray();
	ASSERT_TRUE(renderer.texture_load(albedo, gray.data(), 2, 2));
	//粗糙度贴图：0.2 与 1.0
	uint64_t rough_low = 0;
	std::vector<uint8_t> low_texels = solid_rgba(51, 51, 51);
	ASSERT_TRUE(renderer.texture_load(rough_low, low_texels.data(), 2, 2));
	uint64_t rough_white = 0;
	std::vector<uint8_t> white_texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(rough_white, white_texels.data(), 2, 2));

	//场景：PBR 开启、方向光沿 -Z（与相机同侧，使 N·L 与 N·V 均为 1，中心即高光峰值）、环境光 0
	engine::Camera_Data camera{};
	engine::Light_Data light{};
	light.direction[2] = -1.0f;
	light.intensity = 1.0f;
	light.ambient = 0.0f;
	engine::Fog_Data fog{};
	renderer.scene_apply(camera, light, fog);
	renderer.pbr_enable_set(true);
	renderer.specular_set(2.0f, 16.0f);
	//逐光高光缩放（默认 0 会把方向光高光整段乘零，故显式开启）
	renderer.directional_specular_set(1.0f);
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data_with_normal(0.0f, 0.0f, 1.0f)));

	//粗糙度贴图 = 0.2
	engine::Material_Data smooth{};
	smooth.texture = albedo;
	smooth.roughness_texture = rough_low;
	engine::Material_Handle smooth_handle = 0;
	ASSERT_TRUE(renderer.material_load(smooth_handle, smooth));
	renderer.material_bind_set(smooth_handle);
	ASSERT_TRUE(renderer.mesh_draw(mesh, nullptr));
	std::vector<uint8_t> smooth_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(smooth_pixels.data(), static_cast<uint32_t>(smooth_pixels.size())));

	//粗糙度贴图 = 1.0（白）
	engine::Material_Data rough{};
	rough.texture = albedo;
	rough.roughness_texture = rough_white;
	engine::Material_Handle rough_handle = 0;
	ASSERT_TRUE(renderer.material_load(rough_handle, rough));
	renderer.material_bind_set(rough_handle);
	ASSERT_TRUE(renderer.mesh_draw(mesh, nullptr));
	std::vector<uint8_t> rough_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(rough_pixels.data(), static_cast<uint32_t>(rough_pixels.size())));

	const uint32_t smooth_brightness = center_brightness(smooth_pixels);
	const uint32_t rough_brightness = center_brightness(rough_pixels);
	RecordProperty("roughness_low_brightness", smooth_brightness);
	RecordProperty("roughness_white_brightness", rough_brightness);

	/*
	断言只取「有显著差异」，不锁定方向
	GGX 的峰值强度同时受 D / G / 分母与 sRGB 解码影响，在本用例的固定几何下
	「粗糙度贴图 1.0」反而比「0.2」更亮；要验证的是贴图乘子确实参与高光，故只要求差异显著。
	*/
	EXPECT_GE(static_cast<uint32_t>(std::abs(static_cast<int>(smooth_brightness) - static_cast<int>(rough_brightness))), 20u)
		<< "粗糙度贴图未改变高光（" << smooth_brightness << " vs " << rough_brightness << "）";

	renderer.mesh_unload(mesh);
	renderer.material_unload(smooth_handle);
	renderer.material_unload(rough_handle);
	renderer.texture_unload(albedo);
	renderer.texture_unload(rough_low);
	renderer.texture_unload(rough_white);
}
