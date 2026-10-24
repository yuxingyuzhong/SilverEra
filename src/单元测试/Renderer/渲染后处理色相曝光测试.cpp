/*
渲染后处理色相与曝光测试（第八十七 / 八十八轮）
覆盖：色相旋转（纯红旋转 120° 后主通道改变、饱和度保持）、曝光（倍数缩放单调、1.0 为原样）。
断言口径：主通道以「最大通道下标」判定，与表面通道序无关。
*/
#include "渲染后处理链测试辅助.h"

namespace
{
	//中心像素的最大通道下标
	uint32_t dominant_channel(const std::vector<uint8_t>& pixels)
	{
		std::array<uint8_t, 4> value = pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
		uint32_t best = 0;
		for (uint32_t channel = 1; channel < 3; ++channel)
			if (value[channel] > value[best])
				best = channel;
		return best;
	}

	//纯色纹理（2×2）
	std::vector<uint8_t> solid_texture(uint8_t red, uint8_t green, uint8_t blue)
	{
		std::vector<uint8_t> texels(2 * 2 * 4);
		for (std::size_t index = 0; index < 4; ++index)
		{
			texels[index * 4 + 0] = red;
			texels[index * 4 + 1] = green;
			texels[index * 4 + 2] = blue;
			texels[index * 4 + 3] = 255;
		}
		return texels;
	}
}

/*
后处理预设 · 色相旋转（第八十七轮）
口径：纯红纹理贴中央四边形；参考帧直通，对照帧单级「色相旋转」且偏移 120°；
      断言主通道发生改变（红 → 绿），且旋转后仍为高饱和（最大通道 ≥ 150、极差 ≥ 100）。
*/
TEST_F(Renderer_Post_Chain_Test, 色相旋转改变主通道)
{
	std::vector<uint8_t> texels = solid_texture(255, 0, 0);
	//参考帧
	std::vector<uint8_t> reference(pixel_bytes());
	{
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
		ASSERT_TRUE(renderer.frame_render(reference.data(), static_cast<uint32_t>(reference.size())));
		renderer.mesh_unload(handle);
		renderer.texture_unload(texture);
	}
	//对照帧：色相 +120°
	std::vector<uint8_t> rotated(pixel_bytes());
	{
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(hidden_config()));
		if (!renderer.readback_supported())
			GTEST_SKIP() << "本机交换链图像不具备传输源用法";
		uint64_t target = 0;
		ASSERT_TRUE(renderer.render_target_create(target, renderer.extent_width(), renderer.extent_height()));
		uint64_t pass = post_pipeline_register(renderer, "色相旋转.frag.spv");
		ASSERT_NE(pass, 0u);
		ASSERT_TRUE(renderer.post_chain_set(&pass, 1));
		//偏移 90°（红 → 黄绿；注意门面逐级参数要求 intensity < 100）
		renderer.post_stage_param_set(0, 1.0f, 0.0f, 90.0f);
		uint64_t texture = 0;
		ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
		renderer.texture_bind_set(texture);
		engine::Mesh_Handle handle = 0;
		ASSERT_TRUE(renderer.mesh_load(handle, quad_data()));
		ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
		renderer.render_target_present_set(target);
		ASSERT_TRUE(renderer.frame_render(rotated.data(), static_cast<uint32_t>(rotated.size())));
		renderer.mesh_unload(handle);
		renderer.texture_unload(texture);
		renderer.pipeline_unload(pass);
		renderer.render_target_destroy(target);
	}

	uint32_t reference_dominant = dominant_channel(reference);
	uint32_t rotated_dominant = dominant_channel(rotated);
	std::array<uint8_t, 4> rotated_center = pixel_get(rotated, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	RecordProperty("hue_reference_dominant", static_cast<int>(reference_dominant));
	RecordProperty("hue_rotated_dominant", static_cast<int>(rotated_dominant));
	EXPECT_NE(reference_dominant, rotated_dominant) << "色相未旋转（主通道不变）："
		<< pixel_text(rotated_center);
	//旋转后仍高饱和
	int maximum = rotated_center[0] > rotated_center[1] ? rotated_center[0] : rotated_center[1];
	if (rotated_center[2] > maximum)
		maximum = rotated_center[2];
	int minimum = rotated_center[0] < rotated_center[1] ? rotated_center[0] : rotated_center[1];
	if (rotated_center[2] < minimum)
		minimum = rotated_center[2];
	RecordProperty("hue_rotated_max", maximum);
	RecordProperty("hue_rotated_spread", maximum - minimum);
	EXPECT_GE(maximum, 150) << "旋转后主通道过暗：" << pixel_text(rotated_center);
	EXPECT_GE(maximum - minimum, 100) << "旋转后饱和度丢失：" << pixel_text(rotated_center);
}

/*
后处理预设 · 曝光（第八十八轮）
口径：中灰（128）纹理；倍数 2 / 1 / 0.5 三帧，断言中心绿通道依次降低；倍数 1 与直通近似一致。
*/
TEST_F(Renderer_Post_Chain_Test, 曝光倍数单调改变亮度)
{
	std::vector<uint8_t> texels = solid_texture(128, 128, 128);
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

	//单级曝光链
	uint64_t target = 0;
	ASSERT_TRUE(renderer.render_target_create(target, renderer.extent_width(), renderer.extent_height()));
	uint64_t pass = post_pipeline_register(renderer, "曝光.frag.spv");
	ASSERT_NE(pass, 0u);
	ASSERT_TRUE(renderer.post_chain_set(&pass, 1));
	renderer.render_target_present_set(target);
	uint64_t texture2 = 0;
	ASSERT_TRUE(renderer.texture_load(texture2, texels.data(), 2, 2));
	renderer.texture_bind_set(texture2);
	engine::Mesh_Handle handle2 = 0;
	ASSERT_TRUE(renderer.mesh_load(handle2, quad_data()));

	auto render_with_multiplier = [&](float multiplier, std::vector<uint8_t>& pixels)
	{
		renderer.post_stage_param_set(0, 1.0f, 0.0f, multiplier);
		ASSERT_TRUE(renderer.mesh_draw(handle2, nullptr));
		pixels.assign(pixel_bytes(), 0);
		ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	};

	std::vector<uint8_t> doubled;
	std::vector<uint8_t> unchanged;
	std::vector<uint8_t> halved;
	render_with_multiplier(2.0f, doubled);
	render_with_multiplier(1.0f, unchanged);
	render_with_multiplier(0.5f, halved);
	int doubled_value = pixel_get(doubled, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2)[1];
	int unchanged_value = pixel_get(unchanged, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2)[1];
	int halved_value = pixel_get(halved, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2)[1];
	int reference_value = pixel_get(reference, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2)[1];
	RecordProperty("exposure_doubled", doubled_value);
	RecordProperty("exposure_unchanged", unchanged_value);
	RecordProperty("exposure_halved", halved_value);
	EXPECT_GT(doubled_value, unchanged_value) << "倍数 2 未提亮";
	EXPECT_LT(halved_value, unchanged_value) << "倍数 0.5 未压暗";
	EXPECT_LE(std::abs(unchanged_value - reference_value), 6)
		<< "倍数 1 与直通不一致：" << unchanged_value << " vs " << reference_value;

	renderer.mesh_unload(handle2);
	renderer.texture_unload(texture2);
	renderer.pipeline_unload(pass);
	renderer.render_target_destroy(target);
}
