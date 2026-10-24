/*
渲染后处理伽马测试（第八十九轮）
覆盖：伽马校正（γ 2.2 提亮中间调、γ 0.45 压暗、γ 1.0 原样）。
*/
#include "渲染后处理链测试辅助.h"

/*
后处理预设 · 伽马校正（第八十九轮）
口径：中灰（64）纹理；γ 2.2 / 1.0 / 0.45 三帧，断言中心绿通道依次降低；γ 1.0 与直通近似一致。
*/
TEST_F(Renderer_Post_Chain_Test, 伽马校正改变中间调)
{
	std::vector<uint8_t> texels(2 * 2 * 4);
	for (std::size_t index = 0; index < 4; ++index)
	{
		texels[index * 4 + 0] = 64;
		texels[index * 4 + 1] = 64;
		texels[index * 4 + 2] = 64;
		texels[index * 4 + 3] = 255;
	}
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	renderer.texture_bind_set(texture);
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.mesh_load(handle, quad_data()));
	ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
	//参考帧（直通）
	std::vector<uint8_t> reference(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(reference.data(), static_cast<uint32_t>(reference.size())));
	renderer.mesh_unload(handle);
	renderer.texture_unload(texture);

	//单级伽马链
	uint64_t target = 0;
	ASSERT_TRUE(renderer.render_target_create(target, renderer.extent_width(), renderer.extent_height()));
	uint64_t pass = post_pipeline_register(renderer, "伽马校正.frag.spv");
	ASSERT_NE(pass, 0u);
	ASSERT_TRUE(renderer.post_chain_set(&pass, 1));
	renderer.render_target_present_set(target);
	uint64_t texture2 = 0;
	ASSERT_TRUE(renderer.texture_load(texture2, texels.data(), 2, 2));
	renderer.texture_bind_set(texture2);
	engine::Mesh_Handle handle2 = 0;
	ASSERT_TRUE(renderer.mesh_load(handle2, quad_data()));

	auto render_with_gamma = [&](float gamma, std::vector<uint8_t>& pixels)
	{
		renderer.post_stage_param_set(0, 1.0f, 0.0f, gamma);
		ASSERT_TRUE(renderer.mesh_draw(handle2, nullptr));
		pixels.assign(pixel_bytes(), 0);
		ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	};

	std::vector<uint8_t> brightened;
	std::vector<uint8_t> unchanged;
	std::vector<uint8_t> darkened;
	render_with_gamma(2.2f, brightened);
	render_with_gamma(1.0f, unchanged);
	render_with_gamma(0.45f, darkened);
	int brightened_value = pixel_get(brightened, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2)[1];
	int unchanged_value = pixel_get(unchanged, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2)[1];
	int darkened_value = pixel_get(darkened, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2)[1];
	int reference_value = pixel_get(reference, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2)[1];
	RecordProperty("gamma_brightened", brightened_value);
	RecordProperty("gamma_unchanged", unchanged_value);
	RecordProperty("gamma_darkened", darkened_value);
	EXPECT_GT(brightened_value, unchanged_value) << "γ 2.2 未提亮中间调";
	EXPECT_LT(darkened_value, unchanged_value) << "γ 0.45 未压暗中间调";
	EXPECT_LE(std::abs(unchanged_value - reference_value), 6)
		<< "γ 1.0 与直通不一致：" << unchanged_value << " vs " << reference_value;

	renderer.mesh_unload(handle2);
	renderer.texture_unload(texture2);
	renderer.pipeline_unload(pass);
	renderer.render_target_destroy(target);
}
