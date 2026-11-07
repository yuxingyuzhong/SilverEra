/*
渲染后处理遮蔽测试（第一百四十七轮 · 屏幕空间环境光遮蔽）
覆盖：强度 0 时原样输出 / 强度 1 时在近物周围形成压暗环 / 中心（近物内部）保持明亮。
口径：场景目标含单采样深度图；场景为「全屏接收面（距相机 3）＋ 居中小四边形（距相机 2.5）」；
     遮蔽半径 10 宿像素、偏置 0.5、幂次 1；半径越大邻域跨越近物轮廓，故轮廓外一圈被压暗。
*/
#include "渲染后处理链测试辅助.h"

namespace
{
	//构造缩放并前移的小四边形模型矩阵（列主序；缩放 0.45、z = +0.5 更靠近相机）
	std::array<float, 16> near_quad_matrix(void)
	{
		return {
			0.45f, 0.0f, 0.0f, 0.0f,
			0.0f, 0.45f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.5f, 1.0f,
		};
	}

	//登记「接收面 ＋ 近物」两侧几何（每帧重新登记，避免被上一帧消费）
	void scene_two_layers(engine::Renderer& renderer, engine::Mesh_Handle& receiver, engine::Mesh_Handle& occluder)
	{
		const std::array<float, 16> near_matrix = near_quad_matrix();
		ASSERT_TRUE(renderer.mesh_draw(receiver, nullptr));
		ASSERT_TRUE(renderer.mesh_draw(occluder, near_matrix.data()));
	}

	//统计「本帧比参考帧暗至少 threshold」的像素数（复用 luma 口径）
	uint32_t darker_pixels(const std::vector<uint8_t>& tested, const std::vector<uint8_t>& reference, float threshold)
	{
		return darkened_count(tested, reference, threshold);
	}
}

//强度 0 时遮蔽阶段原样输出：与直通链画面逐通道差不超过 2
TEST_F(Renderer_Post_Chain_Test, 遮蔽强度为零原样输出)
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
		engine::Mesh_Handle receiver = 0;
		engine::Mesh_Handle occluder = 0;
		ASSERT_TRUE(renderer.mesh_load(receiver, quad_data()));
		ASSERT_TRUE(renderer.mesh_load(occluder, quad_data()));
		scene_two_layers(renderer, receiver, occluder);
		ASSERT_TRUE(renderer.frame_render(reference.data(), static_cast<uint32_t>(reference.size())));
		renderer.mesh_unload(receiver);
		renderer.mesh_unload(occluder);
		renderer.pipeline_unload(pass);
		renderer.render_target_destroy(scene_target);
	}
	//对照帧：遮蔽（强度 0）
	std::vector<uint8_t> disabled(pixel_bytes());
	{
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(hidden_config()));
		if (!renderer.readback_supported())
			GTEST_SKIP() << "本机交换链图像不具备传输源用法";
		uint64_t scene_target = 0;
		ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
		uint64_t pass = post_pipeline_register(renderer, "屏幕空间遮蔽.frag.spv");
		ASSERT_NE(pass, 0u);
		ASSERT_TRUE(renderer.post_chain_set(&pass, 1));
		renderer.post_stage_param4_set(0, 0.0f, 10.0f, 0.5f, 1.0f);
		renderer.render_target_present_set(scene_target);
		engine::Mesh_Handle receiver = 0;
		engine::Mesh_Handle occluder = 0;
		ASSERT_TRUE(renderer.mesh_load(receiver, quad_data()));
		ASSERT_TRUE(renderer.mesh_load(occluder, quad_data()));
		scene_two_layers(renderer, receiver, occluder);
		ASSERT_TRUE(renderer.frame_render(disabled.data(), static_cast<uint32_t>(disabled.size())));
		renderer.mesh_unload(receiver);
		renderer.mesh_unload(occluder);
		renderer.pipeline_unload(pass);
		renderer.render_target_destroy(scene_target);
	}
	int difference = max_channel_diff(reference, disabled);
	RecordProperty("ssao_disabled_max_diff", difference);
	EXPECT_LE(difference, 2) << "强度 0 未原样输出（最大差 " << difference << "）";
}

/*
强度 1 时形成遮蔽环（第一百四十七轮）
口径：同上场景；遮蔽强度 1、半径 10、偏置 0.5、幂次 1：
      近物轮廓外一圈应出现压暗（压暗像素数 > 20），而近物中心仍保持明亮（≥ 200）。
*/
TEST_F(Renderer_Post_Chain_Test, 遮蔽在一圈轮廓外压暗)
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
		engine::Mesh_Handle receiver = 0;
		engine::Mesh_Handle occluder = 0;
		ASSERT_TRUE(renderer.mesh_load(receiver, quad_data()));
		ASSERT_TRUE(renderer.mesh_load(occluder, quad_data()));
		scene_two_layers(renderer, receiver, occluder);
		ASSERT_TRUE(renderer.frame_render(reference.data(), static_cast<uint32_t>(reference.size())));
		renderer.mesh_unload(receiver);
		renderer.mesh_unload(occluder);
		renderer.pipeline_unload(pass);
		renderer.render_target_destroy(scene_target);
	}
	//对照帧：遮蔽（强度 1）
	std::vector<uint8_t> shaded(pixel_bytes());
	{
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(hidden_config()));
		if (!renderer.readback_supported())
			GTEST_SKIP() << "本机交换链图像不具备传输源用法";
		uint64_t scene_target = 0;
		ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
		uint64_t pass = post_pipeline_register(renderer, "屏幕空间遮蔽.frag.spv");
		ASSERT_NE(pass, 0u);
		ASSERT_TRUE(renderer.post_chain_set(&pass, 1));
		renderer.post_stage_param4_set(0, 1.0f, 10.0f, 0.5f, 1.0f);
		renderer.render_target_present_set(scene_target);
		engine::Mesh_Handle receiver = 0;
		engine::Mesh_Handle occluder = 0;
		ASSERT_TRUE(renderer.mesh_load(receiver, quad_data()));
		ASSERT_TRUE(renderer.mesh_load(occluder, quad_data()));
		scene_two_layers(renderer, receiver, occluder);
		ASSERT_TRUE(renderer.frame_render(shaded.data(), static_cast<uint32_t>(shaded.size())));
		renderer.mesh_unload(receiver);
		renderer.mesh_unload(occluder);
		renderer.pipeline_unload(pass);
		renderer.render_target_destroy(scene_target);
	}
	//轮廓外压暗像素数
	const uint32_t darker = darker_pixels(shaded, reference, 8.0f);
	//近物中心亮度
	const std::array<uint8_t, 4> center = pixel_get(shaded, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	const uint32_t center_brightness_value =
		(static_cast<uint32_t>(center[0]) + center[1] + center[2]) / 3u;
	RecordProperty("ssao_darker_pixels", static_cast<int>(darker));
	RecordProperty("ssao_center_brightness", static_cast<int>(center_brightness_value));
	EXPECT_GT(darker, 20u) << "遮蔽未在轮廓外形成压暗（压暗像素 " << darker << "）";
	EXPECT_GE(center_brightness_value, 200u) << "遮蔽把近物中心压得过暗（" << center_brightness_value << "）";
}
