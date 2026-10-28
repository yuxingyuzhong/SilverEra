/*
渲染深度描边测试（第一百七十六轮 · 二次元风格化阶段 B）
覆盖：强度 0 原样输出 / 强度 1 在深度断层处产生描边差异 / 未初始化设置无副作用。
口径：场景为单个立方体（轮廓与背景之间产生深度断层）；断言差异像素数（结构性），不做逐字节比对；
      颜色比较只用逐通道最大差与像素计数，不假定回读通道序。
*/
#include "渲染后处理链测试辅助.h"

namespace
{
	//准备「目标组 ＋ 单级后处理 ＋ 立方体」并渲染一帧（可选描边色；第一百八十一轮）
	std::vector<uint8_t> render_outline(const char* shader, const std::array<float, 4>& params,
		const std::array<float, 3>& color = { 0.0f, 0.0f, 0.0f })
	{
		engine::Renderer renderer;
		EXPECT_TRUE(renderer.init(hidden_config()));
		uint64_t scene_target = 0;
		EXPECT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
		uint64_t pass = post_pipeline_register(renderer, shader);
		EXPECT_NE(pass, 0u);
		//链先于 present_set 设置（与既有遮蔽 / 反射用例同序）
		EXPECT_TRUE(renderer.post_chain_set(&pass, 1));
		renderer.post_stage_param4_set(0, params[0], params[1], params[2], params[3]);
		//描边色（默认黑 = 压暗轮廓）
		renderer.post_stage_color_set(0, color[0], color[1], color[2]);
		renderer.render_target_present_set(scene_target);
		engine::Mesh_Handle cube = 0;
		EXPECT_TRUE(renderer.mesh_cube_load(cube, 0.4f));
		EXPECT_TRUE(renderer.mesh_draw(cube, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
		renderer.mesh_unload(cube);
		renderer.pipeline_unload(pass);
		renderer.render_target_destroy(scene_target);
		return pixels;
	}
}

//强度 0 时深度描边原样输出：与直通画面逐通道差不超过 2
TEST_F(Renderer_Post_Chain_Test, 深度描边强度为零原样输出)
{
	engine::Renderer probe;
	ASSERT_TRUE(probe.init(hidden_config()));
	if (!probe.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	std::vector<uint8_t> reference = render_outline("后处理.frag.spv", { 0.0f, 0.0f, 0.0f, 0.0f });
	std::vector<uint8_t> disabled = render_outline("深度描边.frag.spv", { 0.0f, 1.0f, 2.0f, 1.0f });
	int difference = max_channel_diff(reference, disabled);
	RecordProperty("outline_disabled_max_diff", difference);
	EXPECT_LE(difference, 2) << "强度 0 未原样输出（最大差 " << difference << "）";
}

/*
强度 1 时深度断层处产生描边（第一百七十六轮）
口径：立方体轮廓与背景之间深度突变，Laplacian 响应应压暗一批像素；
      断言差异像素数 > 30 且不超过画面四成（避免整体压黑）。
*/
TEST_F(Renderer_Post_Chain_Test, 深度描边在轮廓处产生差异)
{
	std::vector<uint8_t> reference = render_outline("后处理.frag.spv", { 0.0f, 0.0f, 0.0f, 0.0f });
	std::vector<uint8_t> outlined = render_outline("深度描边.frag.spv", { 1.0f, 1.0f, 2.0f, 1.0f });
	uint32_t changes = darkened_count(outlined, reference, 0.05f);
	RecordProperty("outline_changed_pixels", static_cast<int>(changes));
	EXPECT_GT(changes, 30u) << "深度描边未在画面上产生可见差异";
	EXPECT_LT(changes, WINDOW_WIDTH * WINDOW_HEIGHT * 2 / 5) << "深度描边压暗了过多像素（疑似整体变黑）";
}

//未初始化设置无副作用
TEST_F(Renderer_Post_Chain_Test, 深度描边未初始化设置无副作用)
{
	engine::Renderer renderer;
	renderer.post_stage_param4_set(0, 1.0f, 1.0f, 2.0f, 1.0f);
	renderer.post_stage_color_set(0, 1.0f, 0.0f, 0.0f);
	renderer.post_chain_set(nullptr, 0);
	SUCCEED();
}

/*
描边色可配（第一百八十一轮）
口径：同一立方体 + 强度 1；默认黑与红 (1,0,0) 两帧对比——红色描边会把轮廓像素混向红，
      故「外通道（R 或 B）较大者」应有一批像素明显抬升；黑描边只会压暗，不会抬升。
*/
TEST_F(Renderer_Post_Chain_Test, 深度描边描边色可配)
{
	std::vector<uint8_t> black = render_outline("深度描边.frag.spv", { 1.0f, 1.0f, 2.0f, 1.0f });
	std::vector<uint8_t> red = render_outline("深度描边.frag.spv", { 1.0f, 1.0f, 2.0f, 1.0f },
		std::array<float, 3>{ 1.0f, 0.0f, 0.0f });
	//统计外通道被抬升的像素数（红描边应使轮廓像素外通道上升到 255 附近）
	uint32_t raised = 0;
	for (std::size_t index = 0; index + 3 < black.size(); index += 4)
	{
		//本像素的外通道（R 与 B 中较大者；避开回读通道序）
		int black_outer = black[index] > black[index + 2] ? black[index] : black[index + 2];
		int red_outer = red[index] > red[index + 2] ? red[index] : red[index + 2];
		if (red_outer - black_outer >= 20)
			++raised;
	}
	RecordProperty("outline_color_raised_pixels", static_cast<int>(raised));
	EXPECT_GT(raised, 20u) << "描边色未生效（外通道抬升像素 " << raised << "）";
}

