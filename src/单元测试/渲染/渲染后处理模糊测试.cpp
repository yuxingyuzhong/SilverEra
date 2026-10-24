/*
渲染后处理模糊测试（第八十一轮）
覆盖：模糊半径参数化（半径 1 = 现行 3×3 行为；半径 4 应显著抹平细节）。
断言口径：以区域对比度（极值差）为观测量，半径越大越平。
*/
#include "渲染后处理链测试辅助.h"

namespace
{
	/*
	中心区域对比度（通道和的极值差）
	只统计非清屏色像素（清屏色外通道不等），避免背景拉大差异。
	*/
	uint32_t region_spread(const std::vector<uint8_t>& pixels, uint32_t center_x, uint32_t center_y, uint32_t radius)
	{
		uint32_t minimum = 0xFFFFFFFFu;
		uint32_t maximum = 0;
		uint32_t samples = 0;
		for (uint32_t y = center_y - radius; y <= center_y + radius; ++y)
			for (uint32_t x = center_x - radius; x <= center_x + radius; ++x)
			{
				std::array<uint8_t, 4> value = pixel_get(pixels, x, y);
				int channel_spread = static_cast<int>(value[0]) - static_cast<int>(value[2]);
				if (channel_spread < 0)
					channel_spread = -channel_spread;
				if (channel_spread > 3)
					continue;
				uint32_t sum = static_cast<uint32_t>(value[0]) + value[1] + value[2];
				if (sum < minimum)
					minimum = sum;
				if (sum > maximum)
					maximum = sum;
				++samples;
			}
		if (samples < 2)
			return 0;
		return maximum - minimum;
	}
}

/*
后处理预设 · 模糊半径（第八十一轮）
口径：64×64 棋盘格（格 8）贴中等四边形（half 0.6、屏幕约 15~18 像素）；
      两帧只改该级 intensity（半径 1 / 半径 4），以中心区域对比度为观测量。
*/
TEST_F(Renderer_Post_Chain_Test, 模糊半径可配)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//棋盘格纹理与中等四边形
	uint64_t texture = 0;
	ASSERT_TRUE(renderer.texture_load_checkerboard(texture, 64, 64, 8));
	renderer.texture_bind_set(texture);
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.mesh_load(handle, quad_data()));
	//单级模糊链
	uint64_t target = 0;
	ASSERT_TRUE(renderer.render_target_create(target, renderer.extent_width(), renderer.extent_height()));
	uint64_t pass = post_pipeline_register(renderer, "模糊.frag.spv");
	ASSERT_NE(pass, 0u);
	ASSERT_TRUE(renderer.post_chain_set(&pass, 1));
	renderer.render_target_present_set(target);

	//按给定半径渲染一帧并量测中心区域对比度
	auto render_with_radius = [&](float radius, std::vector<uint8_t>& pixels)
	{
		renderer.post_stage_param_set(0, 1.0f, 0.0f, radius);
		ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
		pixels.assign(pixel_bytes(), 0);
		ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	};

	std::vector<uint8_t> narrow_pixels;
	std::vector<uint8_t> wide_pixels;
	render_with_radius(1.0f, narrow_pixels);
	render_with_radius(4.0f, wide_pixels);
	uint32_t narrow_spread = region_spread(narrow_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2, 3);
	uint32_t wide_spread = region_spread(wide_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2, 3);
	RecordProperty("blur_narrow_spread", static_cast<int>(narrow_spread));
	RecordProperty("blur_wide_spread", static_cast<int>(wide_spread));
	//半径 1 仍有可见明暗差；半径 4 应显著抹平
	EXPECT_GE(narrow_spread, 60u) << "半径 1 区域过于平坦（用例前提不成立）：" << narrow_spread;
	EXPECT_LT(wide_spread, narrow_spread) << "半径 4 未降低细节（" << wide_spread
		<< " vs " << narrow_spread << "）";
	EXPECT_LE(wide_spread * 2, narrow_spread) << "半径 4 的抹平幅度不足（" << wide_spread
		<< " vs " << narrow_spread << "）";

	renderer.mesh_unload(handle);
	renderer.texture_unload(texture);
	renderer.pipeline_unload(pass);
	renderer.render_target_destroy(target);
}
