/*
渲染逐物体卡通测试（第四百零八轮 · 方向四阶段 A）
覆盖：材质卡通全关等价无覆盖 / 逐物体色阶覆盖改变分面明暗 / 逐物体边缘光抬升表面亮度 /
      非法卡通参数按不覆盖登记。
口径：卡通参数随实例属性下发（实例步长 80 → 96，location 8）；未带 toon_override 的材质
      与引入前逐字节一致（差 ≤ 2，仅容附件取整）。颜色断言只用「外通道较大者」，避开回读通道序。
*/
#include "渲染场景测试辅助.h"

namespace
{
	//用同一渲染器渲染一帧（material 为 0 时不绑定材质，即全局卡通参数路径；第四百零八轮）
	std::vector<uint8_t> render_frame(engine::Renderer& renderer, engine::Mesh_Handle mesh,
		const engine::Camera_Data& camera, const engine::Light_Data& light, engine::Material_Handle material)
	{
		engine::Fog_Data fog{};
		renderer.scene_apply(camera, light, fog);
		//绑定材质（0 = 不绑定，保持默认管线与白纹理）
		if (material != 0)
			renderer.material_bind_set(material);
		EXPECT_TRUE(renderer.mesh_draw(mesh, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
		//恢复默认绑定
		if (material != 0)
			renderer.material_bind_set(0);
		return pixels;
	}

	//登记带卡通覆盖的材质（参数原样传入，非法值由渲染层拒绝）
	engine::Material_Handle load_toon_material(engine::Renderer& renderer, float steps, float rim, float specular)
	{
		engine::Material_Data data{};
		data.toon_override = true;
		data.toon_steps = steps;
		data.toon_rim = rim;
		data.toon_specular = specular;
		engine::Material_Handle material = 0;
		EXPECT_TRUE(renderer.material_load(material, data));
		return material;
	}

	//两帧回读画面的最大逐通道差
	int max_channel_diff(const std::vector<uint8_t>& left, const std::vector<uint8_t>& right)
	{
		int maximum = 0;
		for (std::size_t index = 0; index < left.size(); ++index)
		{
			int delta = static_cast<int>(left[index]) - static_cast<int>(right[index]);
			if (delta < 0)
				delta = -delta;
			if (delta > maximum)
				maximum = delta;
		}
		return maximum;
	}

	//两帧之间逐通道差达到 threshold 的像素数
	uint32_t differing_count(const std::vector<uint8_t>& left, const std::vector<uint8_t>& right, int threshold)
	{
		uint32_t count = 0;
		for (uint32_t y = 0; y < WINDOW_HEIGHT; ++y)
		{
			for (uint32_t x = 0; x < WINDOW_WIDTH; ++x)
			{
				std::array<uint8_t, 4> a = pixel_get(left, x, y);
				std::array<uint8_t, 4> b = pixel_get(right, x, y);
				//逐通道比较（前三个通道）
				int maximum = 0;
				for (std::size_t index = 0; index < 3; ++index)
				{
					int delta = static_cast<int>(a[index]) - static_cast<int>(b[index]);
					if (delta < 0)
						delta = -delta;
					if (delta > maximum)
						maximum = delta;
				}
				if (maximum >= threshold)
					++count;
			}
		}
		return count;
	}
}

//材质带 toon_override 但三项参数为 0（等价于全关）：画面应与「不绑定材质」逐通道差 ≤ 2
TEST_F(Renderer_Scene_Test, 逐物体卡通全关等价无覆盖)
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
	engine::Camera_Data camera{};
	engine::Light_Data light{};
	light.direction[0] = 0.6f;
	light.direction[1] = -0.6f;
	light.direction[2] = -0.6f;
	light.intensity = 1.0f;
	light.ambient = 0.0f;

	std::vector<uint8_t> without = render_frame(renderer, mesh, camera, light, 0);
	engine::Material_Handle material = load_toon_material(renderer, 0.0f, 0.0f, 0.0f);
	std::vector<uint8_t> with = render_frame(renderer, mesh, camera, light, material);
	int difference = max_channel_diff(without, with);
	RecordProperty("toon_object_off_max_diff", difference);
	EXPECT_LE(difference, 2) << "卡通全关未等价于无覆盖（最大差 " << difference << "）";
	renderer.material_unload(material);
	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

/*
逐物体色阶覆盖改变分面明暗（第四百零八轮）
口径：立方体 ＋ 斜射方向光；材质色阶 1（二值化）与不绑定材质两帧应大量像素显著变化，
      且全局色阶保持默认 0（证明变化来自逐物体覆盖而非全局参数）。
*/
TEST_F(Renderer_Scene_Test, 逐物体色阶覆盖改变分面明暗)
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
	engine::Camera_Data camera{};
	engine::Light_Data light{};
	light.direction[0] = 0.6f;
	light.direction[1] = -0.6f;
	light.direction[2] = -0.6f;
	light.intensity = 1.0f;
	light.ambient = 0.0f;

	std::vector<uint8_t> plain = render_frame(renderer, mesh, camera, light, 0);
	engine::Material_Handle material = load_toon_material(renderer, 1.0f, 0.0f, 0.0f);
	std::vector<uint8_t> quantized = render_frame(renderer, mesh, camera, light, material);
	uint32_t changes = differing_count(plain, quantized, 20);
	RecordProperty("toon_object_steps_changed_pixels", static_cast<int>(changes));
	EXPECT_GE(changes, 30u) << "逐物体色阶未改变分面明暗（变化像素 " << changes << "）";
	renderer.material_unload(material);
	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

/*
逐物体边缘光抬升表面亮度（第四百零八轮）
口径：倾斜 45° 四边形、无光照（画面本应全黑）；材质边缘光 1 时中心外通道应显著抬升。
*/
TEST_F(Renderer_Scene_Test, 逐物体边缘光抬升表面亮度)
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
	engine::Light_Data light{};
	light.intensity = 0.0f;
	light.ambient = 0.0f;

	std::vector<uint8_t> plain = render_frame(renderer, mesh, camera, light, 0);
	engine::Material_Handle material = load_toon_material(renderer, 0.0f, 1.0f, 0.0f);
	std::vector<uint8_t> rim_pixels = render_frame(renderer, mesh, camera, light, material);
	const std::array<uint8_t, 4> plain_center = pixel_get(plain, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	const std::array<uint8_t, 4> rim_center = pixel_get(rim_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	const uint8_t plain_outer = plain_center[0] > plain_center[2] ? plain_center[0] : plain_center[2];
	const uint8_t rim_outer = rim_center[0] > rim_center[2] ? rim_center[0] : rim_center[2];
	RecordProperty("toon_object_rim_plain_outer", static_cast<uint32_t>(plain_outer));
	RecordProperty("toon_object_rim_lit_outer", static_cast<uint32_t>(rim_outer));
	EXPECT_LE(plain_outer, 20u) << "无光照且无边缘光时中心应为黑（" << pixel_text(plain_center) << "）";
	EXPECT_GE(rim_outer, 40u) << "逐物体边缘光未抬升表面亮度（" << pixel_text(rim_center) << "）";
	renderer.material_unload(material);
	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

//非法卡通参数（色阶 9 越界）按「不覆盖」登记：画面应与不绑定材质逐通道差 ≤ 2
TEST_F(Renderer_Scene_Test, 逐物体卡通非法参数按不覆盖)
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
	engine::Camera_Data camera{};
	engine::Light_Data light{};
	light.direction[0] = 0.6f;
	light.direction[1] = -0.6f;
	light.direction[2] = -0.6f;
	light.intensity = 1.0f;
	light.ambient = 0.0f;

	std::vector<uint8_t> plain = render_frame(renderer, mesh, camera, light, 0);
	engine::Material_Handle material = load_toon_material(renderer, 9.0f, 0.0f, 0.0f);
	std::vector<uint8_t> invalid = render_frame(renderer, mesh, camera, light, material);
	int difference = max_channel_diff(plain, invalid);
	RecordProperty("toon_object_invalid_max_diff", difference);
	EXPECT_LE(difference, 2) << "非法卡通参数未被拒绝（最大差 " << difference << "）";
	renderer.material_unload(material);
	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}
