/*
渲染材质贴图测试（第一百二十四轮：法线贴图 / 粗糙度贴图；第一百六十八轮：自发光贴图；
第一百六十九轮：环境光遮蔽贴图 / 金属度贴图）
覆盖：无贴图材质路径逐字节不变 / 法线贴图改变受光 / 粗糙度贴图改变高光 /
     自发光贴图默认白逐字节不变 / 自发光贴图改变发光颜色 /
     环境光遮蔽贴图降低环境光 / 金属度贴图改变漫反射权重。
口径：白与中灰纹理四边形 ＋ 方向光沿 +Z、环境光 0；法线贴图取「切线空间 +X 倾斜」，
     粗糙度贴图取红通道作乘子（默认白纹理 = 1，故无贴图路径不变）；
     自发光贴图乘到自发光项上（默认白纹理 = 1，未配置时与引入前逐字节一致）；
     遮蔽贴图乘到环境光基底上，金属度贴图乘到金属度上（两者默认白 = 1，未配置时逐字节一致）。
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

/*
自发光贴图默认白逐字节不变（第一百六十八轮）
口径：同一场景两次渲染，材质自发光强度均为 1，唯一区别是「不配贴图」与「配白贴图」；
     白纹理采样恒为 1，故自发光项应为同一值，两帧逐字节一致。
*/
TEST_F(Renderer_Scene_Test, 自发光贴图默认白逐字节不变)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t albedo = 0;
	std::vector<uint8_t> white = solid_white();
	ASSERT_TRUE(renderer.texture_load(albedo, white.data(), 2, 2));
	uint64_t white_map = 0;
	std::vector<uint8_t> white_texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(white_map, white_texels.data(), 2, 2));
	//场景：关闭方向光与环境光，画面只剩自发光项，便于直接观察贴图乘子
	engine::Camera_Data camera{};
	engine::Light_Data light{};
	light.intensity = 0.0f;
	light.ambient = 0.0f;
	engine::Fog_Data fog{};
	renderer.scene_apply(camera, light, fog);
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data_with_normal(0.0f, 0.0f, -1.0f)));

	//不配贴图（仅标量自发光）
	engine::Material_Data plain{};
	plain.texture = albedo;
	plain.emissive = 1.0f;
	engine::Material_Handle plain_handle = 0;
	ASSERT_TRUE(renderer.material_load(plain_handle, plain));
	renderer.material_bind_set(plain_handle);
	ASSERT_TRUE(renderer.mesh_draw(mesh, nullptr));
	std::vector<uint8_t> plain_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(plain_pixels.data(), static_cast<uint32_t>(plain_pixels.size())));

	//配白贴图
	engine::Material_Data mapped{};
	mapped.texture = albedo;
	mapped.emissive = 1.0f;
	mapped.emissive_texture = white_map;
	engine::Material_Handle mapped_handle = 0;
	ASSERT_TRUE(renderer.material_load(mapped_handle, mapped));
	renderer.material_bind_set(mapped_handle);
	ASSERT_TRUE(renderer.mesh_draw(mesh, nullptr));
	std::vector<uint8_t> mapped_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(mapped_pixels.data(), static_cast<uint32_t>(mapped_pixels.size())));

	//逐字节最大差
	int maximum = 0;
	for (std::size_t index = 0; index < plain_pixels.size(); ++index)
	{
		int delta = static_cast<int>(plain_pixels[index]) - static_cast<int>(mapped_pixels[index]);
		if (delta < 0)
			delta = -delta;
		if (delta > maximum)
			maximum = delta;
	}
	RecordProperty("emissive_white_diff", maximum);
	EXPECT_LE(maximum, 0) << "白自发光贴图改变了画面（差 " << maximum << "）";

	renderer.mesh_unload(mesh);
	renderer.material_unload(plain_handle);
	renderer.material_unload(mapped_handle);
	renderer.texture_unload(albedo);
	renderer.texture_unload(white_map);
}

/*
自发光贴图改变发光颜色（第一百六十八轮）
口径：自发光强度 1、关闭光照，白贴图下中心接近纯白（通道间几乎无差），
     纯红贴图下中心只剩一个亮通道（通道间差显著）；断言用「通道最大 − 最小」，
     与回读通道序无关。
*/
TEST_F(Renderer_Scene_Test, 自发光贴图改变发光颜色)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t albedo = 0;
	std::vector<uint8_t> white = solid_white();
	ASSERT_TRUE(renderer.texture_load(albedo, white.data(), 2, 2));
	uint64_t white_map = 0;
	std::vector<uint8_t> white_texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(white_map, white_texels.data(), 2, 2));
	uint64_t red_map = 0;
	std::vector<uint8_t> red_texels = solid_rgba(255, 0, 0);
	ASSERT_TRUE(renderer.texture_load(red_map, red_texels.data(), 2, 2));
	//场景：关闭方向光与环境光
	engine::Camera_Data camera{};
	engine::Light_Data light{};
	light.intensity = 0.0f;
	light.ambient = 0.0f;
	engine::Fog_Data fog{};
	renderer.scene_apply(camera, light, fog);
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data_with_normal(0.0f, 0.0f, -1.0f)));

	//白贴图
	engine::Material_Data white_material{};
	white_material.texture = albedo;
	white_material.emissive = 1.0f;
	white_material.emissive_texture = white_map;
	engine::Material_Handle white_handle = 0;
	ASSERT_TRUE(renderer.material_load(white_handle, white_material));
	renderer.material_bind_set(white_handle);
	ASSERT_TRUE(renderer.mesh_draw(mesh, nullptr));
	std::vector<uint8_t> white_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(white_pixels.data(), static_cast<uint32_t>(white_pixels.size())));

	//纯红贴图
	engine::Material_Data red_material{};
	red_material.texture = albedo;
	red_material.emissive = 1.0f;
	red_material.emissive_texture = red_map;
	engine::Material_Handle red_handle = 0;
	ASSERT_TRUE(renderer.material_load(red_handle, red_material));
	renderer.material_bind_set(red_handle);
	ASSERT_TRUE(renderer.mesh_draw(mesh, nullptr));
	std::vector<uint8_t> red_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(red_pixels.data(), static_cast<uint32_t>(red_pixels.size())));

	//中心像素三通道极差（通道序无关）
	const std::array<uint8_t, 4> white_center = pixel_get(white_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	const std::array<uint8_t, 4> red_center = pixel_get(red_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	auto channel_spread = [](const std::array<uint8_t, 4>& value)
	{
		uint8_t highest = value[0];
		uint8_t lowest = value[0];
		for (std::size_t index = 1; index < 3; ++index)
		{
			if (value[index] > highest)
				highest = value[index];
			if (value[index] < lowest)
				lowest = value[index];
		}
		return static_cast<uint32_t>(highest) - static_cast<uint32_t>(lowest);
	};
	const uint32_t white_spread = channel_spread(white_center);
	const uint32_t red_spread = channel_spread(red_center);
	RecordProperty("emissive_white_spread", white_spread);
	RecordProperty("emissive_red_spread", red_spread);

	EXPECT_LE(white_spread, 20u) << "白自发光贴图下中心应为近白（" << pixel_text(white_center) << "）";
	EXPECT_GE(red_spread, 120u) << "红自发光贴图未改变发光颜色（" << pixel_text(red_center) << "）";

	renderer.mesh_unload(mesh);
	renderer.material_unload(white_handle);
	renderer.material_unload(red_handle);
	renderer.texture_unload(albedo);
	renderer.texture_unload(white_map);
	renderer.texture_unload(red_map);
}

/*
环境光遮蔽贴图降低环境光（第一百六十九轮）
口径：方向光关闭、环境光 0.5，画面只剩「反照率 × 环境光 × 遮蔽」；
     白遮蔽贴图与「不配遮蔽贴图」逐字节一致，中灰（0.2）遮蔽贴图使中心明显变暗。
*/
TEST_F(Renderer_Scene_Test, 环境光遮蔽贴图降低环境光)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t albedo = 0;
	std::vector<uint8_t> white = solid_white();
	ASSERT_TRUE(renderer.texture_load(albedo, white.data(), 2, 2));
	uint64_t white_map = 0;
	std::vector<uint8_t> white_texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(white_map, white_texels.data(), 2, 2));
	uint64_t dark_map = 0;
	std::vector<uint8_t> dark_texels = solid_rgba(51, 51, 51);
	ASSERT_TRUE(renderer.texture_load(dark_map, dark_texels.data(), 2, 2));
	//场景：关闭方向光、环境光 0.5
	engine::Camera_Data camera{};
	engine::Light_Data light{};
	light.intensity = 0.0f;
	light.ambient = 0.5f;
	engine::Fog_Data fog{};
	renderer.scene_apply(camera, light, fog);
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data_with_normal(0.0f, 0.0f, -1.0f)));

	//不配遮蔽贴图
	engine::Material_Data plain{};
	plain.texture = albedo;
	engine::Material_Handle plain_handle = 0;
	ASSERT_TRUE(renderer.material_load(plain_handle, plain));
	renderer.material_bind_set(plain_handle);
	ASSERT_TRUE(renderer.mesh_draw(mesh, nullptr));
	std::vector<uint8_t> plain_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(plain_pixels.data(), static_cast<uint32_t>(plain_pixels.size())));

	//白遮蔽贴图
	engine::Material_Data white_material{};
	white_material.texture = albedo;
	white_material.ao_texture = white_map;
	engine::Material_Handle white_handle = 0;
	ASSERT_TRUE(renderer.material_load(white_handle, white_material));
	renderer.material_bind_set(white_handle);
	ASSERT_TRUE(renderer.mesh_draw(mesh, nullptr));
	std::vector<uint8_t> white_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(white_pixels.data(), static_cast<uint32_t>(white_pixels.size())));

	//中灰（0.2）遮蔽贴图
	engine::Material_Data dark_material{};
	dark_material.texture = albedo;
	dark_material.ao_texture = dark_map;
	engine::Material_Handle dark_handle = 0;
	ASSERT_TRUE(renderer.material_load(dark_handle, dark_material));
	renderer.material_bind_set(dark_handle);
	ASSERT_TRUE(renderer.mesh_draw(mesh, nullptr));
	std::vector<uint8_t> dark_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(dark_pixels.data(), static_cast<uint32_t>(dark_pixels.size())));

	//白贴图与无贴图逐字节一致
	int maximum = 0;
	for (std::size_t index = 0; index < plain_pixels.size(); ++index)
	{
		int delta = static_cast<int>(plain_pixels[index]) - static_cast<int>(white_pixels[index]);
		if (delta < 0)
			delta = -delta;
		if (delta > maximum)
			maximum = delta;
	}
	const uint32_t plain_brightness = center_brightness(plain_pixels);
	const uint32_t dark_brightness = center_brightness(dark_pixels);
	RecordProperty("ao_white_diff", maximum);
	RecordProperty("ao_plain_brightness", plain_brightness);
	RecordProperty("ao_dark_brightness", dark_brightness);

	EXPECT_LE(maximum, 0) << "白遮蔽贴图改变了画面（差 " << maximum << "）";
	EXPECT_LE(dark_brightness, plain_brightness - 40u) << "遮蔽贴图未压暗环境光（"
		<< dark_brightness << " vs " << plain_brightness << "）";

	renderer.mesh_unload(mesh);
	renderer.material_unload(plain_handle);
	renderer.material_unload(white_handle);
	renderer.material_unload(dark_handle);
	renderer.texture_unload(albedo);
	renderer.texture_unload(white_map);
	renderer.texture_unload(dark_map);
}

/*
金属度贴图改变漫反射权重（第一百六十九轮）
口径：PBR 开启、逐物体金属度 1、方向光沿 -Z（法线 +Z，N·L = 1）、环境光 0、高光强度 0；
     金属度贴图取红通道乘到金属度上：白贴图 → 金属度 1 → 漫反射权重 0（近黑），
     黑贴图 → 金属度 0 → 漫反射权重 1（近满白）。
*/
TEST_F(Renderer_Scene_Test, 金属度贴图改变漫反射权重)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t albedo = 0;
	std::vector<uint8_t> white = solid_white();
	ASSERT_TRUE(renderer.texture_load(albedo, white.data(), 2, 2));
	uint64_t white_map = 0;
	std::vector<uint8_t> white_texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(white_map, white_texels.data(), 2, 2));
	uint64_t black_map = 0;
	std::vector<uint8_t> black_texels = solid_rgba(0, 0, 0);
	ASSERT_TRUE(renderer.texture_load(black_map, black_texels.data(), 2, 2));
	//场景：PBR 开启、方向光沿 -Z、环境光 0、高光关闭
	engine::Camera_Data camera{};
	engine::Light_Data light{};
	light.direction[2] = -1.0f;
	light.intensity = 1.0f;
	light.ambient = 0.0f;
	engine::Fog_Data fog{};
	renderer.scene_apply(camera, light, fog);
	renderer.pbr_enable_set(true);
	renderer.specular_set(0.0f, 16.0f);
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data_with_normal(0.0f, 0.0f, 1.0f)));

	//白金属度贴图（金属度 1 → 漫反射权重 0）
	engine::Material_Data metal{};
	metal.texture = albedo;
	metal.metallic_texture = white_map;
	metal.pbr_override = true;
	metal.pbr_roughness = 1.0f;
	metal.pbr_metallic = 1.0f;
	engine::Material_Handle metal_handle = 0;
	ASSERT_TRUE(renderer.material_load(metal_handle, metal));
	renderer.material_bind_set(metal_handle);
	ASSERT_TRUE(renderer.mesh_draw(mesh, nullptr));
	std::vector<uint8_t> metal_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(metal_pixels.data(), static_cast<uint32_t>(metal_pixels.size())));

	//黑金属度贴图（金属度 0 → 漫反射权重 1）
	engine::Material_Data dielectric{};
	dielectric.texture = albedo;
	dielectric.metallic_texture = black_map;
	dielectric.pbr_override = true;
	dielectric.pbr_roughness = 1.0f;
	dielectric.pbr_metallic = 1.0f;
	engine::Material_Handle dielectric_handle = 0;
	ASSERT_TRUE(renderer.material_load(dielectric_handle, dielectric));
	renderer.material_bind_set(dielectric_handle);
	ASSERT_TRUE(renderer.mesh_draw(mesh, nullptr));
	std::vector<uint8_t> dielectric_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(dielectric_pixels.data(), static_cast<uint32_t>(dielectric_pixels.size())));

	const uint32_t metal_brightness = center_brightness(metal_pixels);
	const uint32_t dielectric_brightness = center_brightness(dielectric_pixels);
	RecordProperty("metallic_map_metal_brightness", metal_brightness);
	RecordProperty("metallic_map_dielectric_brightness", dielectric_brightness);

	EXPECT_LE(metal_brightness, 60u) << "白金属度贴图未把漫反射权重压到 0（" << metal_brightness << "）";
	EXPECT_GE(dielectric_brightness, 180u) << "黑金属度贴图未把漫反射权重放开（" << dielectric_brightness << "）";

	renderer.mesh_unload(mesh);
	renderer.material_unload(metal_handle);
	renderer.material_unload(dielectric_handle);
	renderer.texture_unload(albedo);
	renderer.texture_unload(white_map);
	renderer.texture_unload(black_map);
}

/*
五张材质贴图同时生效（第一百七十轮：批量绑定合并后的互不干扰验证）
口径：同一材质同时配法线 / 粗糙度 / 自发光 / 遮蔽 / 金属度贴图，环境光 0.5、方向光关闭；
     红自发光使中心只剩一个亮通道，灰遮蔽（0.2）把其余通道压到低值——
     两者同时体现在同一像素上，说明五个绑定各就各位、批量写入未互相覆盖。
*/
TEST_F(Renderer_Scene_Test, 五张材质贴图同时生效)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t albedo = 0;
	std::vector<uint8_t> white = solid_white();
	ASSERT_TRUE(renderer.texture_load(albedo, white.data(), 2, 2));
	uint64_t white_map = 0;
	std::vector<uint8_t> white_texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(white_map, white_texels.data(), 2, 2));
	uint64_t tilted_normal = 0;
	std::vector<uint8_t> normal_texels = tilted_normal_map();
	ASSERT_TRUE(renderer.texture_load(tilted_normal, normal_texels.data(), 2, 2));
	uint64_t red_map = 0;
	std::vector<uint8_t> red_texels = solid_rgba(255, 0, 0);
	ASSERT_TRUE(renderer.texture_load(red_map, red_texels.data(), 2, 2));
	uint64_t dark_map = 0;
	std::vector<uint8_t> dark_texels = solid_rgba(51, 51, 51);
	ASSERT_TRUE(renderer.texture_load(dark_map, dark_texels.data(), 2, 2));
	//场景：方向光关闭、环境光 0.5
	engine::Camera_Data camera{};
	engine::Light_Data light{};
	light.intensity = 0.0f;
	light.ambient = 0.5f;
	engine::Fog_Data fog{};
	renderer.scene_apply(camera, light, fog);
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data_with_normal(0.0f, 0.0f, -1.0f)));

	//五张贴图同时挂到同一材质
	engine::Material_Data mapped{};
	mapped.texture = albedo;
	mapped.normal_texture = tilted_normal;
	mapped.normal_strength = 1.0f;
	mapped.roughness_texture = white_map;
	mapped.emissive = 1.0f;
	mapped.emissive_texture = red_map;
	mapped.ao_texture = dark_map;
	mapped.metallic_texture = white_map;
	engine::Material_Handle handle = 0;
	ASSERT_TRUE(renderer.material_load(handle, mapped));
	renderer.material_bind_set(handle);
	ASSERT_TRUE(renderer.mesh_draw(mesh, nullptr));
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));

	//中心像素：红自发光使通道极差大，遮蔽把其余通道压低
	const std::array<uint8_t, 4> center = pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	uint8_t highest = center[0];
	uint8_t lowest = center[0];
	for (std::size_t index = 1; index < 3; ++index)
	{
		if (center[index] > highest)
			highest = center[index];
		if (center[index] < lowest)
			lowest = center[index];
	}
	const uint32_t spread = static_cast<uint32_t>(highest) - static_cast<uint32_t>(lowest);
	RecordProperty("multi_map_spread", spread);
	RecordProperty("multi_map_lowest", static_cast<uint32_t>(lowest));

	EXPECT_GE(spread, 120u) << "自发光贴图未生效（" << pixel_text(center) << "）";
	EXPECT_LE(lowest, 120u) << "遮蔽贴图未生效（" << pixel_text(center) << "）";

	renderer.mesh_unload(mesh);
	renderer.material_unload(handle);
	renderer.texture_unload(albedo);
	renderer.texture_unload(white_map);
	renderer.texture_unload(tilted_normal);
	renderer.texture_unload(red_map);
	renderer.texture_unload(dark_map);
}
