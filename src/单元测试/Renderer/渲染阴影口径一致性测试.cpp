/*
渲染阴影口径一致性测试（第六百六十二轮；深研方向八 D2）
覆盖：Blinn / PBR / toon 三套光照模型共用同一阴影因子 —— 遮蔽掩码逐像素一致、
     阴影强度 0.5 时三模型同比例、关闭阴影时三模型均无遮蔽。
口径：全屏接收面（z = 0）＋ z = -9 遮挡面（0.6 缩放）、方向光沿 +Z、环境光 0。
*/
#include "渲染场景测试辅助.h"

namespace
{
	//三种光照模型（0 = Blinn、1 = PBR、2 = toon）
	constexpr int MODEL_COUNT = 3;

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

	//按模型构造光照组（方向光沿 +Z、环境光 0）
	engine::Light_Data model_light(int model)
	{
		engine::Light_Data light{};
		light.direction[2] = 1.0f;
		light.intensity = 1.0f;
		light.ambient = 0.0f;
		//PBR-lite
		if (model == 1)
		{
			light.pbr_enabled = 1;
			light.pbr_roughness = 0.5f;
			light.pbr_metallic = 0.0f;
		}
		//二次元色阶
		if (model == 2)
		{
			light.toon_steps = 3;
			light.toon_rim = 0.0f;
		}
		return light;
	}

	//绘制全屏接收面 ＋ 遮挡面并推进一帧（返回画面像素）
	std::vector<uint8_t> draw_scene(engine::Renderer& renderer, engine::Mesh_Handle occluder, engine::Mesh_Handle receiver)
	{
		EXPECT_TRUE(renderer.mesh_draw(occluder, occluder_model(0.0f, 0.0f, -9.0f).data()));
		EXPECT_TRUE(renderer.mesh_draw(receiver, full_screen_matrix().data()));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return pixels;
	}

	//像素亮度（三通道均值）
	uint32_t pixel_brightness(const std::vector<uint8_t>& pixels, std::size_t index)
	{
		const std::size_t offset = index * 4;
		return (static_cast<uint32_t>(pixels[offset]) + pixels[offset + 1] + pixels[offset + 2]) / 3;
	}

	//按阈值提取遮蔽掩码（亮度 < 阈值 = 遮蔽）
	std::vector<uint8_t> shadow_mask(const std::vector<uint8_t>& pixels, uint32_t threshold)
	{
		std::vector<uint8_t> mask(pixels.size() / 4);
		for (std::size_t index = 0; index < mask.size(); ++index)
			mask[index] = pixel_brightness(pixels, index) < threshold ? 1 : 0;
		return mask;
	}

	//掩码中被遮蔽的像素数
	std::size_t mask_count(const std::vector<uint8_t>& mask)
	{
		std::size_t count = 0;
		for (uint8_t value : mask)
			count += value;
		return count;
	}
}

/*
三模型阴影边界一致（第六百六十二轮）
口径：以各自无阴影帧中心亮度的一半为阈值，Blinn / PBR / toon 三份遮蔽掩码逐像素一致。
*/
TEST_F(Renderer_Scene_Test, 三模型阴影边界一致)
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

	engine::Camera_Data camera{};
	engine::Fog_Data fog{};
	std::vector<std::vector<uint8_t>> masks(MODEL_COUNT);
	std::vector<std::size_t> counts(MODEL_COUNT);
	for (int model = 0; model < MODEL_COUNT; ++model)
	{
		engine::Light_Data light = model_light(model);
		//无阴影：取中心亮度作为阈值基准
		renderer.scene_apply(camera, light, fog);
		renderer.shadow_enable_set(false);
		const uint32_t lit = center_brightness(draw_scene(renderer, occluder, receiver));
		//有阴影
		renderer.scene_apply(camera, light, fog);
		renderer.shadow_enable_set(true);
		const std::vector<uint8_t> shadowed = draw_scene(renderer, occluder, receiver);
		masks[model] = shadow_mask(shadowed, lit / 2);
		counts[model] = mask_count(masks[model]);
		RecordProperty("mask_lit", lit);
		EXPECT_GT(counts[model], 0u) << "模型 " << model << " 未检测到遮蔽区域";
	}

	EXPECT_TRUE(masks[0] == masks[1]) << "PBR 与 Blinn 的遮蔽掩码不一致";
	EXPECT_TRUE(masks[0] == masks[2]) << "toon 与 Blinn 的遮蔽掩码不一致";
	RecordProperty("mask_blinn", counts[0]);
	RecordProperty("mask_pbr", counts[1]);
	RecordProperty("mask_toon", counts[2]);

	renderer.mesh_unload(occluder);
	renderer.mesh_unload(receiver);
	renderer.texture_unload(texture);
}

/*
阴影强度同比例（第六百六十二轮）
口径：shadow_strength = 0.5 时，三模型中心亮度 / 各自无阴影中心亮度均约 0.5（±0.03）。
*/
TEST_F(Renderer_Scene_Test, 阴影强度三模型同比例)
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

	engine::Camera_Data camera{};
	engine::Fog_Data fog{};
	for (int model = 0; model < MODEL_COUNT; ++model)
	{
		engine::Light_Data light = model_light(model);
		//基准：无阴影
		renderer.scene_apply(camera, light, fog);
		renderer.shadow_enable_set(false);
		const uint32_t lit = center_brightness(draw_scene(renderer, occluder, receiver));
		ASSERT_GT(lit, 0u);
		//半强度阴影
		light.shadow_strength = 0.5f;
		renderer.scene_apply(camera, light, fog);
		renderer.shadow_enable_set(true);
		const uint32_t shadowed = center_brightness(draw_scene(renderer, occluder, receiver));
		/*
		交换链输出为 sRGB 编码：先把 8 位亮度近似解码回线性再做比值，
		否则线性 0.5 会被编码成约 0.737，比值断言失真。
		*/
		const float lit_linear = std::pow(static_cast<float>(lit) / 255.0f, 2.2f);
		const float shadowed_linear = std::pow(static_cast<float>(shadowed) / 255.0f, 2.2f);
		const float ratio = shadowed_linear / lit_linear;
		RecordProperty("strength_ratio", ratio);
		EXPECT_NEAR(ratio, 0.5f, 0.03f) << "模型 " << model << " 的半强度阴影比例偏离 0.5";
	}

	renderer.mesh_unload(occluder);
	renderer.mesh_unload(receiver);
	renderer.texture_unload(texture);
}

/*
关闭阴影三模型无遮蔽（第六百六十二轮）
口径：shadow_enable_set(false) 后三模型掩码均为空（阴影因子恒 1）。
*/
TEST_F(Renderer_Scene_Test, 关闭阴影三模型无遮蔽)
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

	engine::Camera_Data camera{};
	engine::Fog_Data fog{};
	for (int model = 0; model < MODEL_COUNT; ++model)
	{
		engine::Light_Data light = model_light(model);
		renderer.scene_apply(camera, light, fog);
		renderer.shadow_enable_set(false);
		const std::vector<uint8_t> pixels = draw_scene(renderer, occluder, receiver);
		const uint32_t lit = center_brightness(pixels);
		ASSERT_GT(lit, 0u);
		const std::vector<uint8_t> mask = shadow_mask(pixels, lit / 2);
		EXPECT_EQ(mask_count(mask), 0u) << "模型 " << model << " 在关闭阴影后仍存在遮蔽像素";
	}

	renderer.mesh_unload(occluder);
	renderer.mesh_unload(receiver);
	renderer.texture_unload(texture);
}
