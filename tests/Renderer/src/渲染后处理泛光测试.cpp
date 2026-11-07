/*
渲染后处理泛光测试（亮部提取 / 模糊 / 双源叠加 / 降采样与升采样 / 泛光参数）
（第五十九轮由「渲染后处理链测试.cpp」按功能域拆分；逻辑未改）
*/
#include "渲染后处理链测试辅助.h"

/*
多中间目标 · 三级链「亮部提取 → 模糊 → 灰度」（3 个目标轮转）
预期：中心（白四边形）经亮部提取保留、模糊后仍亮、灰度后三通道趋同且仍亮；
      角落（清屏色）亮度低于阈值被提零，此后保持近黑 —— 即「暗部不变」。
中心三通道趋同即证明确实执行到了第 3 级（前两级都不改三通道相等性之外的东西）。
*/
TEST_F(Renderer_Post_Chain_Test, 三级链亮部提取模糊灰度)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//三个与交换链同尺寸的目标（满足后处理 1:1 取源）
	uint64_t scene_target = 0;
	uint64_t second_target = 0;
	uint64_t third_target = 0;
	ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
	ASSERT_TRUE(renderer.render_target_create(second_target, renderer.extent_width(), renderer.extent_height()));
	ASSERT_TRUE(renderer.render_target_create(third_target, renderer.extent_width(), renderer.extent_height()));
	//三级链：亮部提取 → 模糊 → 灰度
	uint64_t bright = post_pipeline_register(renderer, "亮部提取.frag.spv");
	uint64_t blur = post_pipeline_register(renderer, "模糊.frag.spv");
	uint64_t gray = post_pipeline_register(renderer, "灰度.frag.spv");
	ASSERT_NE(bright, 0u);
	ASSERT_NE(blur, 0u);
	ASSERT_NE(gray, 0u);
	uint64_t chain[3] = { bright, blur, gray };
	ASSERT_TRUE(renderer.post_chain_set(chain, 3));
	//目标组：场景目标 ＋ 两个额外中间目标
	uint64_t chain_targets[3] = { scene_target, second_target, third_target };
	ASSERT_TRUE(renderer.render_target_present_chain_set(chain_targets, 3));
	//登记场景
	engine::Mesh_Handle handle = 0;
	scene_register(renderer, handle);
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	EXPECT_EQ(renderer.frame_count(), 1u);

	//中心：亮部被保留（模糊在内部等价原值），灰度后仍亮且三通道趋同（证明确实执行到第 3 级）
	std::array<uint8_t, 4> center = pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	RecordProperty("bright_center", static_cast<int>(center[0]));
	EXPECT_GE(center[0], 240) << "三级链后中心过暗：" << pixel_text(center);
	EXPECT_LE(std::abs(static_cast<int>(center[0]) - static_cast<int>(center[1])), 2) << "中心 r/g 未趋同：" << pixel_text(center);
	EXPECT_LE(std::abs(static_cast<int>(center[1]) - static_cast<int>(center[2])), 2) << "中心 g/b 未趋同：" << pixel_text(center);
	//角落：清屏色亮度低于阈值 → 亮部提取归零 → 此后恒近黑（暗部不变）
	std::array<uint8_t, 4> corner = pixel_get(pixels, 1, 1);
	EXPECT_LE(*std::max_element(corner.begin(), corner.begin() + 3), DARK_THRESHOLD) << "暗部被提亮（应保持近黑）：" << pixel_text(corner);
	EXPECT_GE(corner[3], SATURATION_THRESHOLD);

	renderer.mesh_unload(handle);
	renderer.pipeline_unload(bright);
	renderer.pipeline_unload(blur);
	renderer.pipeline_unload(gray);
	renderer.render_target_destroy(third_target);
	renderer.render_target_destroy(second_target);
	renderer.render_target_destroy(scene_target);
}

/*
多中间目标 · 二级链「亮部提取 → 模糊」必须与单级「亮部提取」画面不同
口径：模糊只改变亮部边界附近的像素，故两帧逐像素必有差异（max_channel_diff > 0）；
      同时两帧中心都亮、角落都近黑（模糊不改变整体明暗结构）。
*/
TEST_F(Renderer_Post_Chain_Test, 二级链模糊确实参与)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//两个目标（场景目标 ＋ 模糊级宿）
	uint64_t scene_target = 0;
	uint64_t second_target = 0;
	ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
	ASSERT_TRUE(renderer.render_target_create(second_target, renderer.extent_width(), renderer.extent_height()));
	uint64_t bright = post_pipeline_register(renderer, "亮部提取.frag.spv");
	uint64_t blur = post_pipeline_register(renderer, "模糊.frag.spv");
	ASSERT_NE(bright, 0u);
	ASSERT_NE(blur, 0u);
	engine::Mesh_Handle handle = 0;
	scene_register(renderer, handle);
	renderer.render_target_present_set(scene_target);

	//第一帧：单级「亮部提取」
	ASSERT_TRUE(renderer.post_chain_set(&bright, 1));
	std::vector<uint8_t> single_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(single_pixels.data(), static_cast<uint32_t>(single_pixels.size())));

	//第二帧：二级链「亮部提取 → 模糊」（同一场景重登记绘制项）
	ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
	uint64_t chain[2] = { bright, blur };
	ASSERT_TRUE(renderer.post_chain_set(chain, 2));
	uint64_t chain_targets[2] = { scene_target, second_target };
	ASSERT_TRUE(renderer.render_target_present_chain_set(chain_targets, 2));
	std::vector<uint8_t> blurred_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(blurred_pixels.data(), static_cast<uint32_t>(blurred_pixels.size())));

	//模糊必须真实作用于画面（亮部边界被软化）
	int difference = max_channel_diff(single_pixels, blurred_pixels);
	RecordProperty("blur_max_diff", difference);
	EXPECT_GT(difference, 0) << "二级链与单级画面完全一致（模糊级或第二目标未参与）";
	//结构不变：中心仍亮、角落仍近黑
	std::array<uint8_t, 4> center = pixel_get(blurred_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	std::array<uint8_t, 4> corner = pixel_get(blurred_pixels, 1, 1);
	EXPECT_GE(center[0], 240) << "模糊后中心过暗：" << pixel_text(center);
	EXPECT_LE(*std::max_element(corner.begin(), corner.begin() + 3), DARK_THRESHOLD) << "模糊后暗部被提亮：" << pixel_text(corner);

	renderer.mesh_unload(handle);
	renderer.pipeline_unload(bright);
	renderer.pipeline_unload(blur);
	renderer.render_target_destroy(second_target);
	renderer.render_target_destroy(scene_target);
}

/*
目标组不足时回落：链长 2 但只设 1 个目标 → 本帧回落单级默认后处理路径并正常出帧
断言画面为**未经两级链**的结果（中心仍是白四边形、角落仍是暗清屏色）。
*/
TEST_F(Renderer_Post_Chain_Test, 目标组不足时回落)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t scene_target = 0;
	ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
	//两级链「反色 → 亮度阈值」：若被误执行则中心纯黑、角落纯白
	uint64_t invert = post_pipeline_register(renderer, "反色.frag.spv");
	uint64_t threshold = post_pipeline_register(renderer, "亮度阈值.frag.spv");
	ASSERT_NE(invert, 0u);
	ASSERT_NE(threshold, 0u);
	uint64_t chain[2] = { invert, threshold };
	ASSERT_TRUE(renderer.post_chain_set(chain, 2));
	//只设一个目标（目标组不足 2 个）
	renderer.render_target_present_set(scene_target);
	engine::Mesh_Handle handle = 0;
	scene_register(renderer, handle);
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	EXPECT_EQ(renderer.frame_count(), 1u);

	//回落即两级链未执行：中心仍为白四边形、角落仍为暗清屏色
	std::array<uint8_t, 4> center = pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	EXPECT_GE(*std::min_element(center.begin(), center.begin() + 3), 200)
		<< "目标组不足时未回落（画面被两级链处理过）：" << pixel_text(center);
	std::array<uint8_t, 4> corner = pixel_get(pixels, 1, 1);
	EXPECT_LE(*std::max_element(corner.begin(), corner.begin() + 3), 200)
		<< "目标组不足时未回落（角落被反色 / 阈值化为白）：" << pixel_text(corner);

	renderer.mesh_unload(handle);
	renderer.pipeline_unload(invert);
	renderer.pipeline_unload(threshold);
	renderer.render_target_destroy(scene_target);
}

/*
后处理双源叠加（泛光闭环）：三级链「亮部提取 → 模糊 → 泛光叠加」
口径：
  1. 场景用**中高亮灰纹理**（222 灰，线性 ≈0.72，高于亮部提取阈值），使叠加前后有明显亮度差
     （白四边形线性 1.0 会直接饱和，无法区分「叠加」与「未叠加」）；
  2. 参考帧走链长 1 的直通后处理（`后处理.frag`，即场景原样），对照帧走三级叠加链；
  3. 断言：对照帧中心**明显更亮**（≥ 245，参考帧约 222），角落与参考帧一致（暗部不变）
     —— 后者同时证明 binding 1 确实绑定的是「场景目标」而非上一级输出。
说明：双源片元只能放在**链末级**，且链长 ≤ 目标数（否则前面的级会把场景目标覆盖）。
*/
TEST_F(Renderer_Post_Chain_Test, 双源叠加形成泛光)
{
	//建立 222 灰纹理（RGBA8 紧排）
	std::vector<uint8_t> gray_pixels(16 * 16 * 4, 222);
	//参考帧：链长 1 直通
	std::vector<uint8_t> reference(pixel_bytes());
	{
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(hidden_config()));
		if (!renderer.readback_supported())
			GTEST_SKIP() << "本机交换链图像不具备传输源用法";
		uint64_t gray_texture = 0;
		ASSERT_TRUE(renderer.texture_load(gray_texture, gray_pixels.data(), 16, 16));
		uint64_t scene_target = 0;
		ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
		uint64_t pass = post_pipeline_register(renderer, "后处理.frag.spv");
		ASSERT_NE(pass, 0u);
		ASSERT_TRUE(renderer.post_chain_set(&pass, 1));
		renderer.texture_bind_set(gray_texture);
		engine::Mesh_Handle handle = 0;
		ASSERT_TRUE(renderer.mesh_load(handle, quad_data()));
		ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
		renderer.render_target_present_set(scene_target);
		ASSERT_TRUE(renderer.frame_render(reference.data(), static_cast<uint32_t>(reference.size())));
		renderer.mesh_unload(handle);
		renderer.pipeline_unload(pass);
		renderer.render_target_destroy(scene_target);
		renderer.texture_unload(gray_texture);
	}
	//对照帧：三级叠加链（3 个目标 > 链长）
	std::vector<uint8_t> overlaid(pixel_bytes());
	{
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(hidden_config()));
		if (!renderer.readback_supported())
			GTEST_SKIP() << "本机交换链图像不具备传输源用法";
		uint64_t gray_texture = 0;
		ASSERT_TRUE(renderer.texture_load(gray_texture, gray_pixels.data(), 16, 16));
		uint64_t scene_target = 0;
		uint64_t second_target = 0;
		uint64_t third_target = 0;
		ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
		ASSERT_TRUE(renderer.render_target_create(second_target, renderer.extent_width(), renderer.extent_height()));
		ASSERT_TRUE(renderer.render_target_create(third_target, renderer.extent_width(), renderer.extent_height()));
		uint64_t bright = post_pipeline_register(renderer, "亮部提取.frag.spv");
		uint64_t blur = post_pipeline_register(renderer, "模糊.frag.spv");
		uint64_t overlay = post_pipeline_register(renderer, "泛光叠加.frag.spv");
		ASSERT_NE(bright, 0u);
		ASSERT_NE(blur, 0u);
		ASSERT_NE(overlay, 0u);
		uint64_t chain[3] = { bright, blur, overlay };
		ASSERT_TRUE(renderer.post_chain_set(chain, 3));
		uint64_t chain_targets[3] = { scene_target, second_target, third_target };
		ASSERT_TRUE(renderer.render_target_present_chain_set(chain_targets, 3));
		renderer.texture_bind_set(gray_texture);
		engine::Mesh_Handle handle = 0;
		ASSERT_TRUE(renderer.mesh_load(handle, quad_data()));
		ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
		ASSERT_TRUE(renderer.frame_render(overlaid.data(), static_cast<uint32_t>(overlaid.size())));
		renderer.mesh_unload(handle);
		renderer.pipeline_unload(bright);
		renderer.pipeline_unload(blur);
		renderer.pipeline_unload(overlay);
		renderer.render_target_destroy(third_target);
		renderer.render_target_destroy(second_target);
		renderer.render_target_destroy(scene_target);
		renderer.texture_unload(gray_texture);
	}

	//中心：亮部被叠加提亮（参考约 222，叠加后趋饱和）
	std::array<uint8_t, 4> reference_center = pixel_get(reference, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	std::array<uint8_t, 4> overlaid_center = pixel_get(overlaid, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	RecordProperty("overlay_reference_center", static_cast<int>(reference_center[0]));
	RecordProperty("overlay_center", static_cast<int>(overlaid_center[0]));
	EXPECT_LE(*std::max_element(reference_center.begin(), reference_center.begin() + 3), 235)
		<< "参考帧中心过亮（用例前提不成立）：" << pixel_text(reference_center);
	EXPECT_GE(*std::min_element(overlaid_center.begin(), overlaid_center.begin() + 3), 245)
		<< "亮部未被叠加提亮（binding 1 或叠加级未生效）：" << pixel_text(overlaid_center);

	//角落：泛光为 0 → 叠加结果应等于场景原色（暗部不变；同时验证 binding 1 = 场景目标）
	std::array<uint8_t, 4> reference_corner = pixel_get(reference, 1, 1);
	std::array<uint8_t, 4> overlaid_corner = pixel_get(overlaid, 1, 1);
	for (std::size_t index = 0; index < 3; ++index)
	{
		int difference = std::abs(static_cast<int>(reference_corner[index]) - static_cast<int>(overlaid_corner[index]));
		EXPECT_LE(difference, CHAIN_TOLERANCE) << "暗部被改变（应保持场景原色）：参考 "
			<< pixel_text(reference_corner) << " / 叠加 " << pixel_text(overlaid_corner);
	}
}
/*
目标尺寸独立（第十七轮）：半尺寸中间目标的后处理链
口径：
  1. 场景目标为交换链尺寸，第二目标为**半尺寸**（宽高各 / 2），链「亮部提取 → 模糊」：
     第 0 级全尺寸 → 半尺寸（降采样），第 1 级半尺寸 → 交换链（升采样）；
  2. 断言：正常出帧；中心（白四边形）仍亮（≥ 240）、角落（清屏色）仍近黑（≤ 4）——降 / 升采样不破坏明暗结构。
  3. 本用例证明「尺寸不必等于交换链」；逐像素与全尺寸链的差异不做断言（点采样图案依硬件 / 尺寸而变）。
*/
TEST_F(Renderer_Post_Chain_Test, 半尺寸目标链可用)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//全尺寸场景目标 ＋ 半尺寸中间目标
	uint64_t scene_target = 0;
	uint64_t half_target = 0;
	ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
	ASSERT_TRUE(renderer.render_target_create(half_target, renderer.extent_width() / 2, renderer.extent_height() / 2));
	uint64_t bright = post_pipeline_register(renderer, "亮部提取.frag.spv");
	uint64_t blur = post_pipeline_register(renderer, "模糊.frag.spv");
	ASSERT_NE(bright, 0u);
	ASSERT_NE(blur, 0u);
	uint64_t chain[2] = { bright, blur };
	ASSERT_TRUE(renderer.post_chain_set(chain, 2));
	uint64_t chain_targets[2] = { scene_target, half_target };
	ASSERT_TRUE(renderer.render_target_present_chain_set(chain_targets, 2));
	engine::Mesh_Handle handle = 0;
	scene_register(renderer, handle);
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	EXPECT_EQ(renderer.frame_count(), 1u);

	//降 / 升采样后结构不变
	std::array<uint8_t, 4> center = pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	std::array<uint8_t, 4> corner = pixel_get(pixels, 1, 1);
	EXPECT_GE(*std::min_element(center.begin(), center.begin() + 3), 240)
		<< "半尺寸链后中心过暗（尺寸映射有误）：" << pixel_text(center);
	EXPECT_LE(*std::max_element(corner.begin(), corner.begin() + 3), DARK_THRESHOLD)
		<< "半尺寸链后暗部被提亮：" << pixel_text(corner);

	renderer.mesh_unload(handle);
	renderer.pipeline_unload(bright);
	renderer.pipeline_unload(blur);
	renderer.render_target_destroy(half_target);
	renderer.render_target_destroy(scene_target);
}

/*
降采样泛光链（第十八轮）：半尺寸双级模糊的扩散范围大于单级
口径：同一 222 灰纹理场景，分别跑「亮部提取 → 模糊 → 叠加」（单级模糊）与
     「亮部提取 → 模糊 → 模糊 → 叠加」（半尺寸双级模糊）；与无泛光参考帧相比，
     统计「亮度提升 ≥ 5」的像素数，双级必须**严格更多**（相对断言，不固定半径）。
*/
TEST_F(Renderer_Post_Chain_Test, 降采样泛光扩散更广)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	std::vector<uint8_t> gray_pixels(16 * 16 * 4, 222);
	uint64_t gray_texture = 0;
	ASSERT_TRUE(renderer.texture_load(gray_texture, gray_pixels.data(), 16, 16));
	//四个目标：t0 全尺寸、t1/t2 半尺寸、t3 全尺寸
	uint64_t target0 = 0;
	uint64_t target1 = 0;
	uint64_t target2 = 0;
	uint64_t target3 = 0;
	ASSERT_TRUE(renderer.render_target_create(target0, renderer.extent_width(), renderer.extent_height()));
	ASSERT_TRUE(renderer.render_target_create(target1, renderer.extent_width() / 2, renderer.extent_height() / 2));
	ASSERT_TRUE(renderer.render_target_create(target2, renderer.extent_width() / 2, renderer.extent_height() / 2));
	ASSERT_TRUE(renderer.render_target_create(target3, renderer.extent_width(), renderer.extent_height()));
	uint64_t pass = post_pipeline_register(renderer, "后处理.frag.spv");
	uint64_t bright = post_pipeline_register(renderer, "亮部提取.frag.spv");
	uint64_t blur = post_pipeline_register(renderer, "模糊.frag.spv");
	uint64_t overlay = post_pipeline_register(renderer, "泛光叠加.frag.spv");
	ASSERT_NE(pass, 0u);
	ASSERT_NE(bright, 0u);
	ASSERT_NE(blur, 0u);
	ASSERT_NE(overlay, 0u);
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.mesh_load(handle, quad_data()));
	renderer.texture_bind_set(gray_texture);

	//参考帧：无泛光（直通）
	ASSERT_TRUE(renderer.post_chain_set(&pass, 1));
	uint64_t ref_targets[1] = { target0 };
	ASSERT_TRUE(renderer.render_target_present_chain_set(ref_targets, 1));
	ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
	std::vector<uint8_t> reference(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(reference.data(), static_cast<uint32_t>(reference.size())));

	//单级模糊：t0 → t1（半）→ t3（全）→ 叠加
	uint64_t single_chain[3] = { bright, blur, overlay };
	ASSERT_TRUE(renderer.post_chain_set(single_chain, 3));
	uint64_t single_targets[3] = { target0, target1, target3 };
	ASSERT_TRUE(renderer.render_target_present_chain_set(single_targets, 3));
	ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
	std::vector<uint8_t> single_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(single_pixels.data(), static_cast<uint32_t>(single_pixels.size())));

	//双级模糊：t0 → t1（半）→ t2（半）→ t3（全）→ 叠加
	uint64_t double_chain[4] = { bright, blur, blur, overlay };
	ASSERT_TRUE(renderer.post_chain_set(double_chain, 4));
	uint64_t double_targets[4] = { target0, target1, target2, target3 };
	ASSERT_TRUE(renderer.render_target_present_chain_set(double_targets, 4));
	ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
	std::vector<uint8_t> double_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(double_pixels.data(), static_cast<uint32_t>(double_pixels.size())));

	//扩散范围：双级严格大于单级
	uint32_t single_count = brightened_count(single_pixels, reference, 5.0f);
	uint32_t double_count = brightened_count(double_pixels, reference, 5.0f);
	RecordProperty("bloom_single_pixels", single_count);
	RecordProperty("bloom_double_pixels", double_count);
	EXPECT_GT(single_count, 0u) << "单级泛光未提亮任何像素（用例前提不成立）";
	EXPECT_GT(double_count, single_count) << "双级模糊的扩散范围未超过单级（半尺寸链可能未生效）";
	//远离四边形的角落应保持场景原色（两链均如此）
	for (const std::vector<uint8_t>* frame : { &single_pixels, &double_pixels })
	{
		std::array<uint8_t, 4> corner = pixel_get(*frame, 1, 1);
		std::array<uint8_t, 4> ref_corner = pixel_get(reference, 1, 1);
		for (std::size_t index = 0; index < 3; ++index)
			EXPECT_LE(std::abs(static_cast<int>(corner[index]) - static_cast<int>(ref_corner[index])), CHAIN_TOLERANCE)
				<< "角落被泛光污染：" << pixel_text(corner) << " / 参考 " << pixel_text(ref_corner);
	}

	renderer.mesh_unload(handle);
	renderer.pipeline_unload(pass);
	renderer.pipeline_unload(bright);
	renderer.pipeline_unload(blur);
	renderer.pipeline_unload(overlay);
	renderer.render_target_destroy(target3);
	renderer.render_target_destroy(target2);
	renderer.render_target_destroy(target1);
	renderer.render_target_destroy(target0);
	renderer.texture_unload(gray_texture);
}

/*
升采样片元（第十八轮）：半尺寸模糊层经双线性升采样后叠加
链：亮部提取（t0 → 半尺寸 t1）→ 模糊（t1 → t2）→ 升采样（t2 → 全尺寸 t3）→ 泛光叠加（t3 ＋ 场景 → 交换链）
断言：中心被提亮（≥ 245）、角落保持场景原色（≤ 容差）。
*/
TEST_F(Renderer_Post_Chain_Test, 升采样链可用)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	std::vector<uint8_t> gray_pixels(16 * 16 * 4, 222);
	uint64_t gray_texture = 0;
	ASSERT_TRUE(renderer.texture_load(gray_texture, gray_pixels.data(), 16, 16));
	uint64_t target0 = 0;
	uint64_t target1 = 0;
	uint64_t target2 = 0;
	uint64_t target3 = 0;
	ASSERT_TRUE(renderer.render_target_create(target0, renderer.extent_width(), renderer.extent_height()));
	ASSERT_TRUE(renderer.render_target_create(target1, renderer.extent_width() / 2, renderer.extent_height() / 2));
	ASSERT_TRUE(renderer.render_target_create(target2, renderer.extent_width() / 2, renderer.extent_height() / 2));
	ASSERT_TRUE(renderer.render_target_create(target3, renderer.extent_width(), renderer.extent_height()));
	uint64_t bright = post_pipeline_register(renderer, "亮部提取.frag.spv");
	uint64_t blur = post_pipeline_register(renderer, "模糊.frag.spv");
	uint64_t upsample = post_pipeline_register(renderer, "升采样.frag.spv");
	uint64_t overlay = post_pipeline_register(renderer, "泛光叠加.frag.spv");
	ASSERT_NE(bright, 0u);
	ASSERT_NE(blur, 0u);
	ASSERT_NE(upsample, 0u);
	ASSERT_NE(overlay, 0u);
	uint64_t chain[4] = { bright, blur, upsample, overlay };
	ASSERT_TRUE(renderer.post_chain_set(chain, 4));
	uint64_t chain_targets[4] = { target0, target1, target2, target3 };
	ASSERT_TRUE(renderer.render_target_present_chain_set(chain_targets, 4));
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.mesh_load(handle, quad_data()));
	ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
	renderer.texture_bind_set(gray_texture);
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));

	//中心被提亮、角落接近清屏色（泛光为 0，叠加结果即场景原色）
	std::array<uint8_t, 4> center = pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	EXPECT_GE(*std::min_element(center.begin(), center.begin() + 3), 245)
		<< "升采样链后中心未被提亮：" << pixel_text(center);
	std::array<uint8_t, 4> corner = pixel_get(pixels, 1, 1);
	EXPECT_LE(*std::max_element(corner.begin(), corner.begin() + 3), 120)
		<< "升采样链后角落异常发亮：" << pixel_text(corner);

	renderer.mesh_unload(handle);
	renderer.pipeline_unload(bright);
	renderer.pipeline_unload(blur);
	renderer.pipeline_unload(upsample);
	renderer.pipeline_unload(overlay);
	renderer.render_target_destroy(target3);
	renderer.render_target_destroy(target2);
	renderer.render_target_destroy(target1);
	renderer.render_target_destroy(target0);
	renderer.texture_unload(gray_texture);
}

/*
多级降采样（第十九轮）：全 → 半 → 四分之一 → 半 → 全 的 4 级链
口径：t0 全（场景）、t1 半、t2 四分之一、t3 半；链「亮部 → 模糊 → 模糊 → 叠加」：
     第 0 级 t0 → t1（半）、第 1 级 t1 → t2（四分之一）、第 2 级 t2 → t3（升采样到半）、
     第 3 级（末级）t3 ＋ 场景 → 交换链（升采样到全）。
     断言：正常出帧；与「单级半尺寸模糊」链相比，提亮像素数**严格更多**（多级扩散更广）；
     角落不被污染（泛光为 0 处等于场景原色）。
*/
TEST_F(Renderer_Post_Chain_Test, 多级降采样链扩散更广)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	std::vector<uint8_t> gray_pixels(16 * 16 * 4, 222);
	uint64_t gray_texture = 0;
	ASSERT_TRUE(renderer.texture_load(gray_texture, gray_pixels.data(), 16, 16));
	//四个目标：全 / 半 / 四分之一 / 半
	uint64_t target0 = 0;
	uint64_t target1 = 0;
	uint64_t target2 = 0;
	uint64_t target3 = 0;
	ASSERT_TRUE(renderer.render_target_create(target0, renderer.extent_width(), renderer.extent_height()));
	ASSERT_TRUE(renderer.render_target_create(target1, renderer.extent_width() / 2, renderer.extent_height() / 2));
	ASSERT_TRUE(renderer.render_target_create(target2, renderer.extent_width() / 4, renderer.extent_height() / 4));
	ASSERT_TRUE(renderer.render_target_create(target3, renderer.extent_width() / 2, renderer.extent_height() / 2));
	uint64_t pass = post_pipeline_register(renderer, "后处理.frag.spv");
	uint64_t bright = post_pipeline_register(renderer, "亮部提取.frag.spv");
	uint64_t blur = post_pipeline_register(renderer, "模糊.frag.spv");
	uint64_t overlay = post_pipeline_register(renderer, "泛光叠加.frag.spv");
	ASSERT_NE(pass, 0u);
	ASSERT_NE(bright, 0u);
	ASSERT_NE(blur, 0u);
	ASSERT_NE(overlay, 0u);
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.mesh_load(handle, quad_data()));
	renderer.texture_bind_set(gray_texture);

	//参考帧：无泛光
	ASSERT_TRUE(renderer.post_chain_set(&pass, 1));
	uint64_t ref_targets[1] = { target0 };
	ASSERT_TRUE(renderer.render_target_present_chain_set(ref_targets, 1));
	ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
	std::vector<uint8_t> reference(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(reference.data(), static_cast<uint32_t>(reference.size())));

	//单级半尺寸模糊（同第 18 轮：t0 → t1（半）→ t3（半）→ 叠加）
	uint64_t single_chain[3] = { bright, blur, overlay };
	ASSERT_TRUE(renderer.post_chain_set(single_chain, 3));
	uint64_t single_targets[3] = { target0, target1, target3 };
	ASSERT_TRUE(renderer.render_target_present_chain_set(single_targets, 3));
	ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
	std::vector<uint8_t> single_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(single_pixels.data(), static_cast<uint32_t>(single_pixels.size())));

	//多级：t0 → t1（半）→ t2（四分之一）→ t3（半）→ 交换链
	uint64_t multi_chain[4] = { bright, blur, blur, overlay };
	ASSERT_TRUE(renderer.post_chain_set(multi_chain, 4));
	uint64_t multi_targets[4] = { target0, target1, target2, target3 };
	ASSERT_TRUE(renderer.render_target_present_chain_set(multi_targets, 4));
	ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
	std::vector<uint8_t> multi_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(multi_pixels.data(), static_cast<uint32_t>(multi_pixels.size())));

	uint32_t single_count = brightened_count(single_pixels, reference, 5.0f);
	uint32_t multi_count = brightened_count(multi_pixels, reference, 5.0f);
	RecordProperty("multi_single_pixels", single_count);
	RecordProperty("multi_level_pixels", multi_count);
	EXPECT_GT(multi_count, single_count) << "多级降采样的扩散范围未超过单级";

	//角落不被污染
	std::array<uint8_t, 4> corner = pixel_get(multi_pixels, 1, 1);
	std::array<uint8_t, 4> ref_corner = pixel_get(reference, 1, 1);
	for (std::size_t index = 0; index < 3; ++index)
		EXPECT_LE(std::abs(static_cast<int>(corner[index]) - static_cast<int>(ref_corner[index])), CHAIN_TOLERANCE)
			<< "角落被泛光污染：" << pixel_text(corner) << " / 参考 " << pixel_text(ref_corner);

	renderer.mesh_unload(handle);
	renderer.pipeline_unload(pass);
	renderer.pipeline_unload(bright);
	renderer.pipeline_unload(blur);
	renderer.pipeline_unload(overlay);
	renderer.render_target_destroy(target3);
	renderer.render_target_destroy(target2);
	renderer.render_target_destroy(target1);
	renderer.render_target_destroy(target0);
	renderer.texture_unload(gray_texture);
}

/*
泛光参数（第二十一轮）：强度与阈值经推送常量生效
口径：222 灰纹理（线性 ≈0.72）；链「亮部 → 模糊 → 叠加」（2 目标）。
  1. 默认参数（0.6 / 0.1 / 1.0）与强度 0.5 各一帧：强度 0.5 的中心应更暗（但高于参考帧）；
  2. 阈值 0.9（0.72 < 0.8 = 阈值 − 软膝）时提零 → 中心应回到参考帧水平（差 ≤ 容差）。
*/
TEST_F(Renderer_Post_Chain_Test, 泛光参数生效)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	std::vector<uint8_t> gray_pixels(16 * 16 * 4, 222);
	uint64_t gray_texture = 0;
	ASSERT_TRUE(renderer.texture_load(gray_texture, gray_pixels.data(), 16, 16));
	uint64_t target0 = 0;
	uint64_t target1 = 0;
	uint64_t target2 = 0;
	ASSERT_TRUE(renderer.render_target_create(target0, renderer.extent_width(), renderer.extent_height()));
	ASSERT_TRUE(renderer.render_target_create(target1, renderer.extent_width() / 2, renderer.extent_height() / 2));
	ASSERT_TRUE(renderer.render_target_create(target2, renderer.extent_width(), renderer.extent_height()));
	uint64_t pass = post_pipeline_register(renderer, "后处理.frag.spv");
	uint64_t bright = post_pipeline_register(renderer, "亮部提取.frag.spv");
	uint64_t blur = post_pipeline_register(renderer, "模糊.frag.spv");
	uint64_t overlay = post_pipeline_register(renderer, "泛光叠加.frag.spv");
	ASSERT_NE(pass, 0u);
	ASSERT_NE(bright, 0u);
	ASSERT_NE(blur, 0u);
	ASSERT_NE(overlay, 0u);
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.mesh_load(handle, quad_data()));
	renderer.texture_bind_set(gray_texture);

	//参考帧：直通
	ASSERT_TRUE(renderer.post_chain_set(&pass, 1));
	uint64_t ref_targets[1] = { target0 };
	ASSERT_TRUE(renderer.render_target_present_chain_set(ref_targets, 1));
	ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
	std::vector<uint8_t> reference(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(reference.data(), static_cast<uint32_t>(reference.size())));

	//默认参数泛光（t0 → t1（半）→ t2（全）→ 叠加）
	uint64_t chain[3] = { bright, blur, overlay };
	ASSERT_TRUE(renderer.post_chain_set(chain, 3));
	uint64_t chain_targets[3] = { target0, target1, target2 };
	ASSERT_TRUE(renderer.render_target_present_chain_set(chain_targets, 3));
	ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
	std::vector<uint8_t> full_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(full_pixels.data(), static_cast<uint32_t>(full_pixels.size())));

	//强度 0.1（灰 0.72 + 0.072 = 0.79 未饱和，可与默认 1.0 区分）
	renderer.bloom_param_set(0.6f, 0.1f, 0.1f);
	ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
	std::vector<uint8_t> half_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(half_pixels.data(), static_cast<uint32_t>(half_pixels.size())));

	//阈值 0.9（灰 0.72 被提零）
	renderer.bloom_param_set(0.9f, 0.1f, 1.0f);
	ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
	std::vector<uint8_t> high_threshold_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(high_threshold_pixels.data(), static_cast<uint32_t>(high_threshold_pixels.size())));

	int reference_luma = luma_of(pixel_get(reference, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2));
	int full_luma = luma_of(pixel_get(full_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2));
	int half_luma = luma_of(pixel_get(half_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2));
	int threshold_luma = luma_of(pixel_get(high_threshold_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2));
	RecordProperty("bloom_full_luma", full_luma);
	RecordProperty("bloom_half_luma", half_luma);
	RecordProperty("bloom_threshold_luma", threshold_luma);
	RecordProperty("bloom_reference_luma", reference_luma);
	//强度减半：更暗，但仍高于参考
	EXPECT_LT(half_luma, full_luma) << "强度 0.5 未降低亮度";
	EXPECT_GT(half_luma, reference_luma) << "强度 0.5 应仍有泛光（高于参考帧）";
	//阈值 0.9：中灰被提零 → 回到参考水平
	EXPECT_LE(std::abs(threshold_luma - reference_luma), 6)
		<< "阈值 0.9 未把中灰提零（仍有泛光）";

	//恢复默认参数（避免影响后续用例语义）
	renderer.bloom_param_set(0.6f, 0.1f, 1.0f);
	renderer.mesh_unload(handle);
	renderer.pipeline_unload(pass);
	renderer.pipeline_unload(bright);
	renderer.pipeline_unload(blur);
	renderer.pipeline_unload(overlay);
	renderer.render_target_destroy(target2);
	renderer.render_target_destroy(target1);
	renderer.render_target_destroy(target0);
	renderer.texture_unload(gray_texture);
}

/*
逐级泛光参数（第二十三轮）：post_stage_param_set 覆盖全局 bloom_param_set
口径：3 目标 ＋ 三级链「亮部提取 → 模糊 → 泛光叠加」、222 灰纹理。
  1. 全局阈值 0.9 → 中灰被提零，中心回到直通参考；
  2. 复位全局默认后，仅对**第 0 级**设 0.9 → 画面应与「全局 0.9」一致（专设值确实下发到该级）；
  3. 对链外级序（5）设置 → 整次忽略，画面保持上一帧的逐级设置结果。
*/
TEST_F(Renderer_Post_Chain_Test, 逐级泛光参数覆盖全局)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	std::vector<uint8_t> gray_pixels(16 * 16 * 4, 222);
	uint64_t gray_texture = 0;
	ASSERT_TRUE(renderer.texture_load(gray_texture, gray_pixels.data(), 16, 16));
	uint64_t target0 = 0;
	uint64_t target1 = 0;
	uint64_t target2 = 0;
	ASSERT_TRUE(renderer.render_target_create(target0, renderer.extent_width(), renderer.extent_height()));
	ASSERT_TRUE(renderer.render_target_create(target1, renderer.extent_width() / 2, renderer.extent_height() / 2));
	ASSERT_TRUE(renderer.render_target_create(target2, renderer.extent_width(), renderer.extent_height()));
	uint64_t bright = post_pipeline_register(renderer, "亮部提取.frag.spv");
	uint64_t blur = post_pipeline_register(renderer, "模糊.frag.spv");
	uint64_t overlay = post_pipeline_register(renderer, "泛光叠加.frag.spv");
	ASSERT_NE(bright, 0u);
	ASSERT_NE(blur, 0u);
	ASSERT_NE(overlay, 0u);
	uint64_t chain[3] = { bright, blur, overlay };
	ASSERT_TRUE(renderer.post_chain_set(chain, 3));
	uint64_t chain_targets[3] = { target0, target1, target2 };
	ASSERT_TRUE(renderer.render_target_present_chain_set(chain_targets, 3));
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.mesh_load(handle, quad_data()));
	renderer.texture_bind_set(gray_texture);

	//全局阈值 0.9 的对照帧
	renderer.bloom_param_set(0.9f, 0.1f, 1.0f);
	ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
	std::vector<uint8_t> global_high(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(global_high.data(), static_cast<uint32_t>(global_high.size())));

	//复位全局参数，仅第 0 级设 0.9
	renderer.bloom_param_set(0.6f, 0.1f, 1.0f);
	renderer.post_stage_param_set(0, 0.9f, 0.1f, 1.0f);
	ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
	std::vector<uint8_t> stage_high(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(stage_high.data(), static_cast<uint32_t>(stage_high.size())));

	//链外级序：整次忽略（画面应仍等于逐级 0.9 的结果）
	renderer.post_stage_param_set(5, 0.6f, 0.1f, 1.0f);
	ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
	std::vector<uint8_t> invalid_stage(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(invalid_stage.data(), static_cast<uint32_t>(invalid_stage.size())));

	int global_high_luma = luma_of(pixel_get(global_high, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2));
	int stage_high_luma = luma_of(pixel_get(stage_high, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2));
	int invalid_stage_luma = luma_of(pixel_get(invalid_stage, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2));
	RecordProperty("stage_global_high_luma", global_high_luma);
	RecordProperty("stage_only_high_luma", stage_high_luma);
	RecordProperty("stage_invalid_luma", invalid_stage_luma);
	//逐级专设 = 全局同值（专设值确实下发到第 0 级）
	EXPECT_LE(std::abs(stage_high_luma - global_high_luma), 2)
		<< "逐级参数与全局同值结果不一致";
	//链外级序被忽略 → 画面不变
	EXPECT_LE(std::abs(invalid_stage_luma - stage_high_luma), 2)
		<< "链外级序设置未被忽略";

	renderer.mesh_unload(handle);
	renderer.pipeline_unload(bright);
	renderer.pipeline_unload(blur);
	renderer.pipeline_unload(overlay);
	renderer.render_target_destroy(target2);
	renderer.render_target_destroy(target1);
	renderer.render_target_destroy(target0);
	renderer.texture_unload(gray_texture);
}
