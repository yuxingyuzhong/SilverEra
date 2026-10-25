/*
渲染 Alpha 覆盖率写入测试（第一百二十六轮）
口径：
  ① 1×（未开 MSAA）时覆盖率写入自动关闭，画面与既有 alpha 混合逐字节一致；
  ② MSAA 下覆盖率写入使半透明面片只覆盖部分采样，解析后比纯 alpha 混合更「薄」；
  ③ 覆盖率写入在 Opaque 档同样生效（不依赖混合），未开启时仍为纯不透明。
*/
#include "渲染场景测试辅助.h"

//半透明白色纹理（RGBA8：rgb = 255、alpha = 128 → 约 0.5）
std::vector<uint8_t> half_alpha_white(void)
{
	std::vector<uint8_t> pixels(2 * 2 * 4);
	for (std::size_t index = 0; index < pixels.size(); index += 4)
	{
		pixels[index] = 255;
		pixels[index + 1] = 255;
		pixels[index + 2] = 255;
		pixels[index + 3] = 128;
	}
	return pixels;
}

//中心像素亮度
uint32_t center_luma(const std::vector<uint8_t>& pixels)
{
	return center_brightness(pixels);
}

/*
单采样下覆盖率开关不改变画面（第一百二十六轮）
同一渲染器分别用「Alpha 混合」与「Alpha 混合 ＋ 覆盖率写入」渲一帧，1× 下两帧须逐字节一致。
*/
TEST_F(Renderer_Scene_Test, 覆盖率写入单采样下画面不变)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	ASSERT_EQ(renderer.msaa_samples(), 1u) << "本用例要求 1×（未开 MSAA）";

	engine::Mesh_Handle quad = 0;
	ASSERT_TRUE(renderer.mesh_load(quad, quad_data()));
	uint64_t texture = 0;
	std::vector<uint8_t> texels = half_alpha_white();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));

	//材质 A：Alpha 混合（不启用覆盖率）
	engine::Material_Data plain{};
	plain.texture = texture;
	plain.blend = engine::Blend_Mode::Alpha;
	engine::Material_Handle material_plain = 0;
	ASSERT_TRUE(renderer.material_load(material_plain, plain));
	//材质 B：Alpha 混合 ＋ 覆盖率写入
	engine::Material_Data coverage = plain;
	coverage.alpha_to_coverage = true;
	engine::Material_Handle material_coverage = 0;
	ASSERT_TRUE(renderer.material_load(material_coverage, coverage));

	//渲染一帧并返回整帧像素
	auto render_frame = [&]()
	{
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return pixels;
	};

	//基线帧（纯 alpha 混合）
	renderer.material_bind_set(material_plain);
	ASSERT_TRUE(renderer.mesh_draw(quad, nullptr));
	std::vector<uint8_t> baseline = render_frame();

	//覆盖率帧（1× 下应自动关闭，与基线逐字节一致）
	renderer.material_bind_set(material_coverage);
	ASSERT_TRUE(renderer.mesh_draw(quad, nullptr));
	std::vector<uint8_t> covered = render_frame();

	RecordProperty("coverage_1x_baseline", static_cast<int>(center_luma(baseline)));
	RecordProperty("coverage_1x_covered", static_cast<int>(center_luma(covered)));
	EXPECT_EQ(covered, baseline) << "1× 下覆盖率写入改变了画面";

	renderer.material_unload(material_plain);
	renderer.material_unload(material_coverage);
	renderer.mesh_unload(quad);
	renderer.texture_unload(texture);
}

/*
MSAA 下覆盖率写入降低透明覆盖（第一百二十六轮）
4×（按设备上限降档）时，同为 alpha 0.5 的 Alpha 混合面片：
关闭覆盖率为「所有采样都混合」，开启后仅约一半采样参与，解析后中心明显更接近清屏色。
*/
TEST_F(Renderer_Scene_Test, 多重采样下覆盖率降低透明覆盖)
{
	engine::Renderer renderer;
	engine::Renderer_Config config = hidden_config();
	config.msaa_samples = 4;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//设备不支持多重采样则跳过（本用例只验证 > 1× 的行为）
	if (renderer.msaa_samples() < 2)
		GTEST_SKIP() << "本机不支持多重采样";

	engine::Mesh_Handle quad = 0;
	ASSERT_TRUE(renderer.mesh_load(quad, quad_data()));
	uint64_t texture = 0;
	std::vector<uint8_t> texels = half_alpha_white();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));

	//材质 A：Alpha 混合
	engine::Material_Data plain{};
	plain.texture = texture;
	plain.blend = engine::Blend_Mode::Alpha;
	engine::Material_Handle material_plain = 0;
	ASSERT_TRUE(renderer.material_load(material_plain, plain));
	//材质 B：Alpha 混合 ＋ 覆盖率写入
	engine::Material_Data coverage = plain;
	coverage.alpha_to_coverage = true;
	engine::Material_Handle material_coverage = 0;
	ASSERT_TRUE(renderer.material_load(material_coverage, coverage));

	auto render_frame = [&]()
	{
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return pixels;
	};

	renderer.material_bind_set(material_plain);
	ASSERT_TRUE(renderer.mesh_draw(quad, nullptr));
	const uint32_t mixed_center = center_luma(render_frame());

	renderer.material_bind_set(material_coverage);
	ASSERT_TRUE(renderer.mesh_draw(quad, nullptr));
	const uint32_t covered_center = center_luma(render_frame());

	//切回材质 A，确认可逆
	renderer.material_bind_set(material_plain);
	ASSERT_TRUE(renderer.mesh_draw(quad, nullptr));
	const uint32_t restored_center = center_luma(render_frame());

	RecordProperty("coverage_msaa_mixed", static_cast<int>(mixed_center));
	RecordProperty("coverage_msaa_covered", static_cast<int>(covered_center));
	RecordProperty("coverage_msaa_restored", static_cast<int>(restored_center));
	RecordProperty("coverage_msaa_samples", static_cast<int>(renderer.msaa_samples()));

	EXPECT_GE(mixed_center, 140u) << "基线面片过暗（用例前提不成立）";
	EXPECT_LE(covered_center, mixed_center - 25u) << "覆盖率写入未减少透明覆盖";
	EXPECT_NEAR(restored_center, mixed_center, 3u) << "切回无覆盖率材质后画面未恢复";

	renderer.material_unload(material_plain);
	renderer.material_unload(material_coverage);
	renderer.mesh_unload(quad);
	renderer.texture_unload(texture);
}

/*
覆盖率写入在不透明档同样生效（第一百二十六轮）
Opaque 档（不混合）＋ alpha 0.5 纹理：开启覆盖率只写入约一半采样，解析后介于清屏色与纯白之间；
未开启时 alpha 被忽略，中心为纯白。
*/
TEST_F(Renderer_Scene_Test, 覆盖率写入不透明档产生覆盖)
{
	engine::Renderer renderer;
	engine::Renderer_Config config = hidden_config();
	config.msaa_samples = 4;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	if (renderer.msaa_samples() < 2)
		GTEST_SKIP() << "本机不支持多重采样";

	engine::Mesh_Handle quad = 0;
	ASSERT_TRUE(renderer.mesh_load(quad, quad_data()));
	uint64_t texture = 0;
	std::vector<uint8_t> texels = half_alpha_white();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));

	//材质 A：Opaque（alpha 被忽略 → 纯白）
	engine::Material_Data opaque{};
	opaque.texture = texture;
	opaque.blend = engine::Blend_Mode::Opaque;
	engine::Material_Handle material_opaque = 0;
	ASSERT_TRUE(renderer.material_load(material_opaque, opaque));
	//材质 B：Opaque ＋ 覆盖率写入
	engine::Material_Data coverage = opaque;
	coverage.alpha_to_coverage = true;
	engine::Material_Handle material_coverage = 0;
	ASSERT_TRUE(renderer.material_load(material_coverage, coverage));

	auto render_frame = [&]()
	{
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return pixels;
	};

	renderer.material_bind_set(material_opaque);
	ASSERT_TRUE(renderer.mesh_draw(quad, nullptr));
	const uint32_t opaque_center = center_luma(render_frame());

	renderer.material_bind_set(material_coverage);
	ASSERT_TRUE(renderer.mesh_draw(quad, nullptr));
	const uint32_t covered_center = center_luma(render_frame());

	RecordProperty("coverage_opaque_plain", static_cast<int>(opaque_center));
	RecordProperty("coverage_opaque_covered", static_cast<int>(covered_center));

	EXPECT_GE(opaque_center, 250u) << "不透明面片未写满中心";
	EXPECT_LE(covered_center, opaque_center - 40u) << "不透明档的覆盖率写入未生效";
	EXPECT_GE(covered_center, 100u) << "覆盖率写入过度（中心接近清屏色）";

	renderer.material_unload(material_opaque);
	renderer.material_unload(material_coverage);
	renderer.mesh_unload(quad);
	renderer.texture_unload(texture);
}
