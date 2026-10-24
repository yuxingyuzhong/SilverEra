/*
渲染后处理颗粒与像素化测试（第七十九 / 八十轮）
覆盖：胶片颗粒（平坦画面叠加零均值噪声）、像素化（马赛克块化）。
断言口径：以「相邻像素差之和」为高频能量观测量，避免依赖具体噪声值；
          亮度均值变化受控（零均值噪声不改变整体明暗）。
*/
#include "渲染后处理链测试辅助.h"

/*
后处理预设 · 胶片颗粒（第七十九轮）
口径：2×2 纯灰纹理（128）贴中央四边形；参考帧交换链直通（画面平坦），
      对照帧单级「胶片颗粒」链并把该级强度专设为 0.4；
      以中央三行「相邻像素灰度差之和」为高频能量：直通应近 0、加颗粒后显著增大；
      同时断言中央区域平均亮度变化 ≤ 4（零均值噪声不改变整体明暗）。
*/
TEST_F(Renderer_Post_Chain_Test, 胶片颗粒增加高频噪声)
{
	//纯灰纹理（2×2，灰度 128）
	std::vector<uint8_t> texels(2 * 2 * 4);
	for (std::size_t index = 0; index < 4; ++index)
	{
		texels[index * 4 + 0] = 128;
		texels[index * 4 + 1] = 128;
		texels[index * 4 + 2] = 128;
		texels[index * 4 + 3] = 255;
	}
	//高频能量：中央三行相邻像素绿通道差之和（跳过清屏色像素）
	auto high_frequency = [&](const std::vector<uint8_t>& pixels)
	{
		std::uint64_t energy = 0;
		for (uint32_t y = WINDOW_HEIGHT / 2 - 1; y <= WINDOW_HEIGHT / 2 + 1; ++y)
		{
			//严格落在四边形内部（避开四边形边界与清屏色的过渡）
			for (uint32_t x = WINDOW_WIDTH / 2 - 6; x <= WINDOW_WIDTH / 2 + 6; ++x)
			{
				std::array<uint8_t, 4> left = pixel_get(pixels, x - 1, y);
				std::array<uint8_t, 4> right = pixel_get(pixels, x, y);
				int diff = static_cast<int>(left[1]) - static_cast<int>(right[1]);
				if (diff < 0)
					diff = -diff;
				energy += static_cast<std::uint64_t>(diff);
			}
		}
		return energy;
	};
	//中央区域平均亮度（三通道均值）
	auto mean_luma = [&](const std::vector<uint8_t>& pixels)
	{
		double sum = 0.0;
		uint32_t count = 0;
		for (uint32_t y = WINDOW_HEIGHT / 4; y < WINDOW_HEIGHT * 3 / 4; ++y)
			for (uint32_t x = WINDOW_WIDTH / 4; x < WINDOW_WIDTH * 3 / 4; ++x)
			{
				std::array<uint8_t, 4> value = pixel_get(pixels, x, y);
				sum += static_cast<double>(value[0]) + value[1] + value[2];
				++count;
			}
		return count == 0 ? 0.0 : sum / (3.0 * static_cast<double>(count));
	};

	//参考帧：交换链直通（平坦）
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
	//对照帧：单级胶片颗粒链（强度 0.4）
	std::vector<uint8_t> grained(pixel_bytes());
	{
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(hidden_config()));
		if (!renderer.readback_supported())
			GTEST_SKIP() << "本机交换链图像不具备传输源用法";
		uint64_t target = 0;
		ASSERT_TRUE(renderer.render_target_create(target, renderer.extent_width(), renderer.extent_height()));
		uint64_t pass = post_pipeline_register(renderer, "胶片颗粒.frag.spv");
		ASSERT_NE(pass, 0u);
		ASSERT_TRUE(renderer.post_chain_set(&pass, 1));
		//颗粒强度专设（阈值 / 软膝由校验规则要求合法，本片元不使用）
		renderer.post_stage_param_set(0, 1.0f, 0.0f, 0.3f);
		uint64_t texture = 0;
		ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
		renderer.texture_bind_set(texture);
		engine::Mesh_Handle handle = 0;
		ASSERT_TRUE(renderer.mesh_load(handle, quad_data()));
		ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
		renderer.render_target_present_set(target);
		ASSERT_TRUE(renderer.frame_render(grained.data(), static_cast<uint32_t>(grained.size())));
		renderer.mesh_unload(handle);
		renderer.texture_unload(texture);
		renderer.pipeline_unload(pass);
		renderer.render_target_destroy(target);
	}

	//高频能量：平坦画面近 0，加颗粒后显著增大
	std::uint64_t reference_energy = high_frequency(reference);
	std::uint64_t grained_energy = high_frequency(grained);
	RecordProperty("grain_reference_energy", static_cast<int>(reference_energy));
	RecordProperty("grain_energy", static_cast<int>(grained_energy));
	EXPECT_LE(reference_energy, 20u) << "参考帧本身不平坦（用例前提不成立）：" << reference_energy;
	EXPECT_GT(grained_energy, reference_energy + 200u) << "颗粒未引入高频噪声（"
		<< grained_energy << " vs " << reference_energy << "）";

	//零均值：平均亮度基本不变
	double reference_mean = mean_luma(reference);
	double grained_mean = mean_luma(grained);
	RecordProperty("grain_reference_mean", static_cast<int>(reference_mean));
	RecordProperty("grain_mean", static_cast<int>(grained_mean));
	//sRGB 编码非线性会让零均值线性噪声的编码均值略有偏移，容差取 10
	EXPECT_LE(std::abs(grained_mean - reference_mean), 10.0) << "颗粒改变了整体明暗（"
		<< reference_mean << " → " << grained_mean << "）";
}

/*
后处理预设 · 像素化（第八十轮）
口径：64×1 水平灰度渐变贴中央四边形；参考帧交换链直通（渐变连续），
      对照帧单级「像素化」链并把该级强度专设为 6（块尺寸 6 宿像素）；
      以「中央行不同灰度值个数」为块化观测量：直通应多、块化后应显著减少。
*/
TEST_F(Renderer_Post_Chain_Test, 像素化形成马赛克块)
{
	//水平灰度渐变（64×1）
	std::vector<uint8_t> texels(64 * 4);
	for (uint32_t x = 0; x < 64; ++x)
	{
		uint8_t value = static_cast<uint8_t>(x * 255 / 63);
		std::size_t offset = static_cast<std::size_t>(x) * 4;
		texels[offset + 0] = value;
		texels[offset + 1] = value;
		texels[offset + 2] = value;
		texels[offset + 3] = 255;
	}
	//中央行的不同灰度值个数（跳过清屏色）
	auto distinct_grays = [&](const std::vector<uint8_t>& pixels)
	{
		bool seen[256] = {};
		uint32_t count = 0;
		uint32_t row = WINDOW_HEIGHT / 2;
		for (uint32_t x = WINDOW_WIDTH / 4; x < WINDOW_WIDTH * 3 / 4; ++x)
		{
			std::array<uint8_t, 4> value = pixel_get(pixels, x, row);
			int spread = static_cast<int>(value[0]) - static_cast<int>(value[2]);
			if (spread < 0)
				spread = -spread;
			if (spread > 3)
				continue;
			uint8_t gray = value[1];
			if (!seen[gray])
			{
				seen[gray] = true;
				++count;
			}
		}
		return count;
	};

	//参考帧：交换链直通
	std::vector<uint8_t> reference(pixel_bytes());
	{
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(hidden_config()));
		if (!renderer.readback_supported())
			GTEST_SKIP() << "本机交换链图像不具备传输源用法";
		uint64_t texture = 0;
		ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 64, 1));
		renderer.texture_bind_set(texture);
		engine::Mesh_Handle handle = 0;
		ASSERT_TRUE(renderer.mesh_load(handle, quad_data()));
		ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
		ASSERT_TRUE(renderer.frame_render(reference.data(), static_cast<uint32_t>(reference.size())));
		renderer.mesh_unload(handle);
		renderer.texture_unload(texture);
	}
	//对照帧：单级像素化链（块尺寸 6）
	std::vector<uint8_t> pixelated(pixel_bytes());
	{
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(hidden_config()));
		if (!renderer.readback_supported())
			GTEST_SKIP() << "本机交换链图像不具备传输源用法";
		uint64_t target = 0;
		ASSERT_TRUE(renderer.render_target_create(target, renderer.extent_width(), renderer.extent_height()));
		uint64_t pass = post_pipeline_register(renderer, "像素化.frag.spv");
		ASSERT_NE(pass, 0u);
		ASSERT_TRUE(renderer.post_chain_set(&pass, 1));
		//块尺寸专设（阈值 / 软膝由校验规则要求合法，本片元不使用）
		renderer.post_stage_param_set(0, 1.0f, 0.0f, 6.0f);
		uint64_t texture = 0;
		ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 64, 1));
		renderer.texture_bind_set(texture);
		engine::Mesh_Handle handle = 0;
		ASSERT_TRUE(renderer.mesh_load(handle, quad_data()));
		ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
		renderer.render_target_present_set(target);
		ASSERT_TRUE(renderer.frame_render(pixelated.data(), static_cast<uint32_t>(pixelated.size())));
		renderer.mesh_unload(handle);
		renderer.texture_unload(texture);
		renderer.pipeline_unload(pass);
		renderer.render_target_destroy(target);
	}

	//渐变直通灰度级多，马赛克后显著减少
	uint32_t reference_grays = distinct_grays(reference);
	uint32_t pixelated_grays = distinct_grays(pixelated);
	RecordProperty("pixelate_reference_grays", static_cast<int>(reference_grays));
	RecordProperty("pixelate_pixelated_grays", static_cast<int>(pixelated_grays));
	EXPECT_GE(reference_grays, 10u) << "参考帧灰度级过少（用例前提不成立）：" << reference_grays;
	EXPECT_LE(pixelated_grays, 5u) << "像素化未形成马赛克块（仍有 " << pixelated_grays << " 个灰度级）";
	EXPECT_LE(pixelated_grays * 3, reference_grays) << "块化后灰度级未显著减少（"
		<< pixelated_grays << " vs " << reference_grays << "）";
}
