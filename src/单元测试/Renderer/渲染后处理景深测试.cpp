/*
渲染后处理景深测试（第一百二十三轮）
覆盖：对焦面像素保持清晰 / 离焦使边缘变软 / 最大模糊近零时近似直通。
口径：单级「景深.frag」链；场景为白纹理四边形（默认相机在 +Z 3 处，故该面距相机 3）；
     背景为清屏色（深色），边缘过渡处可观测模糊扩散。
注意：景深读取单采样深度图，故用例在 1× 采样（默认）下运行。
*/
#include "渲染后处理链测试辅助.h"

namespace
{
	//按固定列自上而下找「第一条进入四边形的行」（亮度阈值 100）
	int first_quad_row(const std::vector<uint8_t>& pixels)
	{
		for (uint32_t y = 0; y < WINDOW_HEIGHT; ++y)
			if (luma_of(pixel_get(pixels, WINDOW_WIDTH / 2, y)) > 100.0f)
				return static_cast<int>(y);
		return -1;
	}
}

/*
对焦面像素保持清晰（第一百二十三轮）
口径：对焦距离 = 四边形到相机的距离（3），光圈 1、最大模糊 6；
     四边形内部像素的弥散圆为 0，故与「直通」链的中心像素逐字节一致（容差 2）。
*/
TEST_F(Renderer_Post_Chain_Test, 景深对焦面保持清晰)
{
	//参考帧：直通
	std::vector<uint8_t> reference(pixel_bytes());
	{
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(hidden_config()));
		if (!renderer.readback_supported())
			GTEST_SKIP() << "本机交换链图像不具备传输源用法";
		uint64_t scene_target = 0;
		ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
		uint64_t pass = post_pipeline_register(renderer, "后处理.frag.spv");
		ASSERT_NE(pass, 0u);
		ASSERT_TRUE(renderer.post_chain_set(&pass, 1));
		renderer.render_target_present_set(scene_target);
		engine::Mesh_Handle handle = 0;
		scene_register(renderer, handle);
		ASSERT_TRUE(renderer.frame_render(reference.data(), static_cast<uint32_t>(reference.size())));
		renderer.mesh_unload(handle);
		renderer.pipeline_unload(pass);
		renderer.render_target_destroy(scene_target);
	}
	//对照帧：景深（对焦在 3）
	std::vector<uint8_t> dof(pixel_bytes());
	{
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(hidden_config()));
		if (!renderer.readback_supported())
			GTEST_SKIP() << "本机交换链图像不具备传输源用法";
		uint64_t scene_target = 0;
		ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
		uint64_t pass = post_pipeline_register(renderer, "景深.frag.spv");
		ASSERT_NE(pass, 0u);
		ASSERT_TRUE(renderer.post_chain_set(&pass, 1));
		renderer.post_stage_param_set(0, 3.0f, 1.0f, 6.0f);
		renderer.render_target_present_set(scene_target);
		engine::Mesh_Handle handle = 0;
		scene_register(renderer, handle);
		ASSERT_TRUE(renderer.frame_render(dof.data(), static_cast<uint32_t>(dof.size())));
		renderer.mesh_unload(handle);
		renderer.pipeline_unload(pass);
		renderer.render_target_destroy(scene_target);
	}

	//中心像素（四边形内部）
	const std::array<uint8_t, 4> reference_center = pixel_get(reference, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	const std::array<uint8_t, 4> dof_center = pixel_get(dof, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	int center_difference = 0;
	for (std::size_t index = 0; index < 3; ++index)
	{
		int delta = static_cast<int>(reference_center[index]) - static_cast<int>(dof_center[index]);
		if (delta < 0)
			delta = -delta;
		if (delta > center_difference)
			center_difference = delta;
	}
	RecordProperty("dof_focus_center_diff", center_difference);

	EXPECT_GE(static_cast<int>(luma_of(reference_center)), 200) << "四边形中心未被照亮";
	EXPECT_LE(center_difference, 2) << "对焦面像素被模糊（通道差 " << center_difference << "）";
}

/*
离焦使边缘变软（第一百二十三轮）
口径：对焦距离 0.6（远小于四边形到相机的 3）、光圈 1、最大模糊 6；
     四边形上边缘之上的背景像素因模糊扩散而变亮，且边缘向背景的过渡行数增加。
*/
TEST_F(Renderer_Post_Chain_Test, 景深离焦使边缘变软)
{
	//参考帧：直通
	std::vector<uint8_t> reference(pixel_bytes());
	{
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(hidden_config()));
		if (!renderer.readback_supported())
			GTEST_SKIP() << "本机交换链图像不具备传输源用法";
		uint64_t scene_target = 0;
		ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
		uint64_t pass = post_pipeline_register(renderer, "后处理.frag.spv");
		ASSERT_NE(pass, 0u);
		ASSERT_TRUE(renderer.post_chain_set(&pass, 1));
		renderer.render_target_present_set(scene_target);
		engine::Mesh_Handle handle = 0;
		scene_register(renderer, handle);
		ASSERT_TRUE(renderer.frame_render(reference.data(), static_cast<uint32_t>(reference.size())));
		renderer.mesh_unload(handle);
		renderer.pipeline_unload(pass);
		renderer.render_target_destroy(scene_target);
	}
	//对照帧：景深（严重离焦）
	std::vector<uint8_t> dof(pixel_bytes());
	{
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(hidden_config()));
		if (!renderer.readback_supported())
			GTEST_SKIP() << "本机交换链图像不具备传输源用法";
		uint64_t scene_target = 0;
		ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
		uint64_t pass = post_pipeline_register(renderer, "景深.frag.spv");
		ASSERT_NE(pass, 0u);
		ASSERT_TRUE(renderer.post_chain_set(&pass, 1));
		renderer.post_stage_param_set(0, 0.6f, 1.0f, 6.0f);
		renderer.render_target_present_set(scene_target);
		engine::Mesh_Handle handle = 0;
		scene_register(renderer, handle);
		ASSERT_TRUE(renderer.frame_render(dof.data(), static_cast<uint32_t>(dof.size())));
		renderer.mesh_unload(handle);
		renderer.pipeline_unload(pass);
		renderer.render_target_destroy(scene_target);
	}

	//按参考帧定位四边形上边缘，再在同一行比较（避免模糊改变阈值穿越位置）
	const int edge_row = first_quad_row(reference);
	ASSERT_GE(edge_row, 3) << "未找到四边形上边缘";
	const uint32_t sample_row = static_cast<uint32_t>(edge_row - 2);
	const int reference_above = static_cast<int>(luma_of(pixel_get(reference, WINDOW_WIDTH / 2, sample_row)));
	const int dof_above = static_cast<int>(luma_of(pixel_get(dof, WINDOW_WIDTH / 2, sample_row)));
	//四边形中心仍应保持明亮
	const int dof_center = static_cast<int>(luma_of(pixel_get(dof, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2)));
	RecordProperty("dof_edge_row", edge_row);
	RecordProperty("dof_edge_reference", reference_above);
	RecordProperty("dof_edge_blurred", dof_above);
	RecordProperty("dof_edge_center", dof_center);

	EXPECT_GE(dof_above, reference_above + 8) << "离焦未使边缘外像素变亮（"
		<< dof_above << " vs " << reference_above << "）";
	EXPECT_GE(dof_center, 200) << "离焦后四边形中心过暗（" << dof_center << "）";
}

/*
最大模糊近零时近似直通（第一百二十三轮）
口径：对焦 0.6、光圈 1、最大模糊 0.001（半径 < 0.5 直接写回），画面应与直通一致（容差 2）。
*/
TEST_F(Renderer_Post_Chain_Test, 景深最大模糊近零近似直通)
{
	//参考帧：直通
	std::vector<uint8_t> reference(pixel_bytes());
	{
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(hidden_config()));
		if (!renderer.readback_supported())
			GTEST_SKIP() << "本机交换链图像不具备传输源用法";
		uint64_t scene_target = 0;
		ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
		uint64_t pass = post_pipeline_register(renderer, "后处理.frag.spv");
		ASSERT_NE(pass, 0u);
		ASSERT_TRUE(renderer.post_chain_set(&pass, 1));
		renderer.render_target_present_set(scene_target);
		engine::Mesh_Handle handle = 0;
		scene_register(renderer, handle);
		ASSERT_TRUE(renderer.frame_render(reference.data(), static_cast<uint32_t>(reference.size())));
		renderer.mesh_unload(handle);
		renderer.pipeline_unload(pass);
		renderer.render_target_destroy(scene_target);
	}
	//对照帧：景深（模糊半径近零）
	std::vector<uint8_t> dof(pixel_bytes());
	{
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(hidden_config()));
		if (!renderer.readback_supported())
			GTEST_SKIP() << "本机交换链图像不具备传输源用法";
		uint64_t scene_target = 0;
		ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
		uint64_t pass = post_pipeline_register(renderer, "景深.frag.spv");
		ASSERT_NE(pass, 0u);
		ASSERT_TRUE(renderer.post_chain_set(&pass, 1));
		renderer.post_stage_param_set(0, 0.6f, 1.0f, 0.001f);
		renderer.render_target_present_set(scene_target);
		engine::Mesh_Handle handle = 0;
		scene_register(renderer, handle);
		ASSERT_TRUE(renderer.frame_render(dof.data(), static_cast<uint32_t>(dof.size())));
		renderer.mesh_unload(handle);
		renderer.pipeline_unload(pass);
		renderer.render_target_destroy(scene_target);
	}

	const int difference = max_channel_diff(reference, dof);
	RecordProperty("dof_near_zero_blur_diff", difference);
	EXPECT_LE(difference, 2) << "模糊半径近零时画面被改变（差 " << difference << "）";
}
