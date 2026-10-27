/*
渲染后处理反射测试（第一百六十七轮 · 屏幕空间反射）
覆盖：强度 0 原样输出（与直通逐通道差 ≤ 2）/ 强度 1 在斜置镜面上出现反射差异 / 未初始化设置无副作用。
口径：场景为「绕 X 轴 −30° 的镜面四边形（下前方）＋ 沿反射方向悬在其上方的小立方体」；
      镜面法线 (0, 0.5, 0.87)，反射射线朝相机上方走并在屏幕内命中立方体，
      取样颜色与镜面自身明暗不同，故断言差异像素数（结构性），不做逐字节比对。
      通道序无关：只比较逐通道最大差与像素计数，不假定 R/G/B 顺序。
*/
#include "渲染后处理链测试辅助.h"

namespace
{
	//镜面：绕 X 轴 −30° 的四边形（放大 3 倍、下移 0.6、后移 0.4；列主序矩阵）
	std::array<float, 16> mirror_matrix(void)
	{
		//旋转 −30° 的正余弦
		const float cosine = 0.8660254f;
		const float sine = -0.5f;
		//缩放系数
		const float scale = 3.0f;
		return {
			scale, 0.0f, 0.0f, 0.0f,
			0.0f, cosine * scale, sine * scale, 0.0f,
			0.0f, -sine * scale, cosine * scale, 0.0f,
			0.0f, -0.6f, -0.4f, 1.0f,
		};
	}

	//目标立方体：沿镜面反射方向（0, 0.8, 0.6）悬在上方
	std::array<float, 16> target_matrix(void)
	{
		return {
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 1.1f, 0.9f, 1.0f,
		};
	}

	//登记「镜面 ＋ 目标」两侧几何（每帧重新登记，避免被上一帧消费）
	void scene_mirror(engine::Renderer& renderer, engine::Mesh_Handle mirror, engine::Mesh_Handle target)
	{
		const std::array<float, 16> mirror_position = mirror_matrix();
		const std::array<float, 16> target_position = target_matrix();
		ASSERT_TRUE(renderer.mesh_draw(mirror, mirror_position.data()));
		ASSERT_TRUE(renderer.mesh_draw(target, target_position.data()));
	}

	//统计两帧之间逐通道差达到 threshold 的像素数（通道序无关）
	uint32_t differing_pixels(const std::vector<uint8_t>& left, const std::vector<uint8_t>& right, int threshold)
	{
		uint32_t count = 0;
		for (uint32_t y = 0; y < WINDOW_HEIGHT; ++y)
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
		return count;
	}

	//准备「目标组 ＋ 单级后处理 ＋ 镜面 / 立方体」并渲染一帧
	std::vector<uint8_t> render_with(const char* shader, const std::array<float, 4>& params)
	{
		engine::Renderer renderer;
		EXPECT_TRUE(renderer.init(hidden_config()));
		uint64_t scene_target = 0;
		EXPECT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
		uint64_t pass = post_pipeline_register(renderer, shader);
		EXPECT_NE(pass, 0u);
		//链先于 present_set 设置（与既有遮蔽用例同序）
		EXPECT_TRUE(renderer.post_chain_set(&pass, 1));
		renderer.post_stage_param4_set(0, params[0], params[1], params[2], params[3]);
		renderer.render_target_present_set(scene_target);
		engine::Mesh_Handle mirror = 0;
		EXPECT_TRUE(renderer.mesh_load(mirror, quad_data()));
		engine::Mesh_Handle target = 0;
		EXPECT_TRUE(renderer.mesh_cube_load(target, 0.4f));
		scene_mirror(renderer, mirror, target);
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
		renderer.mesh_unload(mirror);
		renderer.mesh_unload(target);
		renderer.pipeline_unload(pass);
		renderer.render_target_destroy(scene_target);
		return pixels;
	}
}

//强度 0 时反射阶段原样输出：与直通画面逐通道差不超过 2
TEST_F(Renderer_Post_Chain_Test, 反射强度为零原样输出)
{
	engine::Renderer probe;
	ASSERT_TRUE(probe.init(hidden_config()));
	if (!probe.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	std::vector<uint8_t> reference = render_with("后处理.frag.spv", { 0.0f, 0.0f, 0.0f, 0.0f });
	std::vector<uint8_t> disabled = render_with("屏幕空间反射.frag.spv", { 0.0f, 0.5f, 8.0f, 1.0f });
	int difference = max_channel_diff(reference, disabled);
	RecordProperty("ssr_disabled_max_diff", difference);
	EXPECT_LE(difference, 2) << "强度 0 未原样输出（最大差 " << difference << "）";
}

/*
强度 1 时镜面出现反射（第一百六十七轮）
口径：镜面与目标立方体同时入画；反射射线在屏幕空间命中立方体，
      断言差异像素数 > 20 且不超过画面四成（避免整体破坏）。
*/
TEST_F(Renderer_Post_Chain_Test, 反射在镜面上产生差异)
{
	std::vector<uint8_t> reference = render_with("后处理.frag.spv", { 0.0f, 0.0f, 0.0f, 0.0f });
	std::vector<uint8_t> reflected = render_with("屏幕空间反射.frag.spv", { 1.0f, 0.5f, 8.0f, 1.0f });
	uint32_t changes = differing_pixels(reflected, reference, 8);
	RecordProperty("ssr_changed_pixels", static_cast<int>(changes));
	EXPECT_GT(changes, 20u) << "反射未在画面上产生可见差异";
	EXPECT_LT(changes, WINDOW_WIDTH * WINDOW_HEIGHT * 2 / 5) << "反射改变了过多像素（疑似整体替换）";
}

//未初始化设置无副作用
TEST_F(Renderer_Post_Chain_Test, 反射未初始化设置无副作用)
{
	engine::Renderer renderer;
	renderer.post_stage_param4_set(0, 1.0f, 0.5f, 8.0f, 1.0f);
	renderer.post_chain_set(nullptr, 0);
	SUCCEED();
}
