/*
渲染后处理链测试（链生命周期 / 单级与两级链 / 预设：灰度、描边、色调映射及串联）
（第五十九轮按功能域拆分：泛光相关见「渲染后处理泛光测试.cpp」、调色与可观测性见「渲染后处理调色测试.cpp」；逻辑未改）
*/
#include "渲染后处理链测试辅助.h"

TEST_F(Renderer_Post_Chain_Test, 未初始化设置无副作用)
{
	engine::Renderer renderer;
	//未初始化时调用应静默返回 / 返回假（不崩溃）
	EXPECT_FALSE(renderer.render_target_present_chain_set(nullptr, 1));
	EXPECT_FALSE(renderer.post_chain_set(nullptr, 0));
	EXPECT_FALSE(renderer.valid());
}

//未登记句柄被整链拒绝，且无效呈现目标被回落
TEST_F(Renderer_Post_Chain_Test, 无效句柄回落)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//含未登记句柄的链应被整链拒绝
	uint64_t unregistered = 12345;
	EXPECT_FALSE(renderer.post_chain_set(&unregistered, 1));
	//设定一个未登记的呈现目标（应关闭该路径）
	renderer.render_target_present_set(999);
	std::vector<uint8_t> pixels(pixel_bytes());
	uint32_t width = 0;
	uint32_t height = 0;
	//仍应走默认交换链路径并成功出帧
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size()), &width, &height));
	EXPECT_EQ(width, WINDOW_WIDTH);
	EXPECT_EQ(height, WINDOW_HEIGHT);
	EXPECT_EQ(renderer.frame_count(), 1u);
}

//单级链（count = 1，反色）生效：中心（四边形）被反色为黑、角落（清屏色）被反色为亮
TEST_F(Renderer_Post_Chain_Test, 单级链反色生效)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//创建场景目标
	uint64_t scene_target = 0;
	ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
	//登记反色管线并设为单级链
	uint64_t invert = post_pipeline_register(renderer, "反色.frag.spv");
	ASSERT_NE(invert, 0u);
	ASSERT_TRUE(renderer.post_chain_set(&invert, 1));
	//登记场景并设定同帧后处理呈现
	engine::Mesh_Handle handle = 0;
	scene_register(renderer, handle);
	renderer.render_target_present_set(scene_target);
	std::vector<uint8_t> pixels(pixel_bytes());
	//一帧内完成（帧计数仅推进 1）
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	EXPECT_EQ(renderer.frame_count(), 1u);
	//中心为四边形（白 → 反色后为黑）
	std::array<uint8_t, 4> center = pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	EXPECT_LE(*std::max_element(center.begin(), center.begin() + 3), DARK_THRESHOLD) << "中心应为反色后的黑：" << pixel_text(center);
	EXPECT_GE(center[3], SATURATION_THRESHOLD);
	//角落为清屏色（暗 → 反色后为亮）
	std::array<uint8_t, 4> corner = pixel_get(pixels, 1, 1);
	int corner_min = *std::min_element(corner.begin(), corner.begin() + 3);
	EXPECT_GE(corner_min, 200) << "角落应为反色后的亮色：" << pixel_text(corner);
	renderer.mesh_unload(handle);
	renderer.pipeline_unload(invert);
	renderer.render_target_destroy(scene_target);
}

//两级链「反色 → 反色」＝ 原画面：与直接走交换链路径的回读画面一致
TEST_F(Renderer_Post_Chain_Test, 两级反色恒等于原画面)
{
	//参考帧：直接走交换链路径
	std::vector<uint8_t> reference(pixel_bytes());
	{
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(hidden_config()));
		if (!renderer.readback_supported())
			GTEST_SKIP() << "本机交换链图像不具备传输源用法";
		engine::Mesh_Handle handle = 0;
		scene_register(renderer, handle);
		ASSERT_TRUE(renderer.frame_render(reference.data(), static_cast<uint32_t>(reference.size())));
		renderer.mesh_unload(handle);
	}
	//对照帧：两级「反色 → 反色」链
	std::vector<uint8_t> chained(pixel_bytes());
	{
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(hidden_config()));
		if (!renderer.readback_supported())
			GTEST_SKIP() << "本机交换链图像不具备传输源用法";
		//创建 ping-pong 成对目标
		uint64_t scene_target = 0;
		uint64_t second_target = 0;
		ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
		ASSERT_TRUE(renderer.render_target_create(second_target, renderer.extent_width(), renderer.extent_height()));
		//登记两级反色管线（各自独立句柄，共用同一顶点着色器）
		uint64_t stages[2] = { 0, 0 };
		stages[0] = post_pipeline_register(renderer, "反色.frag.spv");
		stages[1] = post_pipeline_register(renderer, "反色.frag.spv");
		ASSERT_NE(stages[0], 0u);
		ASSERT_NE(stages[1], 0u);
		ASSERT_NE(stages[0], stages[1]);
		ASSERT_TRUE(renderer.post_chain_set(stages, 2));
		engine::Mesh_Handle handle = 0;
		scene_register(renderer, handle);
		renderer.render_target_present_set(scene_target);
		uint64_t chain_targets[2] = { scene_target, second_target };
		ASSERT_TRUE(renderer.render_target_present_chain_set(chain_targets, 2));
		ASSERT_TRUE(renderer.frame_render(chained.data(), static_cast<uint32_t>(chained.size())));
		EXPECT_EQ(renderer.frame_count(), 1u);
		renderer.mesh_unload(handle);
		renderer.pipeline_unload(stages[0]);
		renderer.pipeline_unload(stages[1]);
		renderer.render_target_destroy(scene_target);
		renderer.render_target_destroy(second_target);
	}
	//两画面逐通道一致（证明确实按序执行了两级并复合为恒等）
	int difference = max_channel_diff(reference, chained);
	EXPECT_LE(difference, CHAIN_TOLERANCE) << "两级反色链画面与原画面不一致（最大通道差 " << difference << "）";
}

//两级链「反色 → 亮度阈值」：中心（四边形）为纯黑、角落（清屏色）为纯白
TEST_F(Renderer_Post_Chain_Test, 两级反色与阈值叠加)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//创建 ping-pong 成对目标
	uint64_t scene_target = 0;
	uint64_t second_target = 0;
	ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
	ASSERT_TRUE(renderer.render_target_create(second_target, renderer.extent_width(), renderer.extent_height()));
	//登记两级管线：一级反色、二级亮度阈值（顶点复用 后处理.vert）
	uint64_t stages[2] = { 0, 0 };
	stages[0] = post_pipeline_register(renderer, "反色.frag.spv");
	stages[1] = post_pipeline_register(renderer, "亮度阈值.frag.spv");
	ASSERT_NE(stages[0], 0u);
	ASSERT_NE(stages[1], 0u);
	ASSERT_TRUE(renderer.post_chain_set(stages, 2));
	//登记场景并设定成对目标
	engine::Mesh_Handle handle = 0;
	scene_register(renderer, handle);
	renderer.render_target_present_set(scene_target);
	uint64_t chain_targets[2] = { scene_target, second_target };
	ASSERT_TRUE(renderer.render_target_present_chain_set(chain_targets, 2));
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	EXPECT_EQ(renderer.frame_count(), 1u);
	/*
	预期：四边形为白（线性 1.0）→ 反色 0 → 亮度 0 < 0.5 → 纯黑；
	      清屏色为暗（约 0.05）→ 反色约 0.95 → 亮度 ≥ 0.5 → 纯白。
	*/
	std::array<uint8_t, 4> center = pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	EXPECT_LE(*std::max_element(center.begin(), center.begin() + 3), DARK_THRESHOLD) << "中心应为纯黑：" << pixel_text(center);
	EXPECT_GE(center[3], SATURATION_THRESHOLD);
	std::array<uint8_t, 4> corner = pixel_get(pixels, 1, 1);
	EXPECT_GE(*std::min_element(corner.begin(), corner.begin() + 3), SATURATION_THRESHOLD) << "角落应为纯白：" << pixel_text(corner);
	EXPECT_GE(corner[3], SATURATION_THRESHOLD);
	renderer.mesh_unload(handle);
	renderer.pipeline_unload(stages[0]);
	renderer.pipeline_unload(stages[1]);
	renderer.render_target_destroy(scene_target);
	renderer.render_target_destroy(second_target);
}

/*
链长 1 与既有单级默认路径逐字节等价（收口后 `render_to_target_present` 为 `render_chain_present` 的薄封装）
参考帧走「不设链」的既有单级默认路径（内置 后处理.vert / 后处理.frag）；
对照帧走「链长 1 ＋ 经 pipeline_load 登记同一条 后处理 片元着色器」。
两条路径现共用同一套录制实现，故两帧画面应**逐字节相同**（除取整外无任何差异）。
*/
TEST_F(Renderer_Post_Chain_Test, 单级链与既有单级路径等价)
{
	//参考帧：既有单级默认路径（不设链）
	std::vector<uint8_t> reference(pixel_bytes());
	{
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(hidden_config()));
		if (!renderer.readback_supported())
			GTEST_SKIP() << "本机交换链图像不具备传输源用法";
		uint64_t scene_target = 0;
		ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
		engine::Mesh_Handle handle = 0;
		scene_register(renderer, handle);
		renderer.render_target_present_set(scene_target);
		ASSERT_TRUE(renderer.frame_render(reference.data(), static_cast<uint32_t>(reference.size())));
		renderer.mesh_unload(handle);
		renderer.render_target_destroy(scene_target);
	}
	//对照帧：链长 1（同一片元着色器，经 pipeline_load 登记）
	std::vector<uint8_t> chained(pixel_bytes());
	{
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(hidden_config()));
		if (!renderer.readback_supported())
			GTEST_SKIP() << "本机交换链图像不具备传输源用法";
		uint64_t scene_target = 0;
		ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
		uint64_t stage = post_pipeline_register(renderer, "后处理.frag.spv");
		ASSERT_NE(stage, 0u);
		ASSERT_TRUE(renderer.post_chain_set(&stage, 1));
		engine::Mesh_Handle handle = 0;
		scene_register(renderer, handle);
		renderer.render_target_present_set(scene_target);
		ASSERT_TRUE(renderer.frame_render(chained.data(), static_cast<uint32_t>(chained.size())));
		renderer.mesh_unload(handle);
		renderer.pipeline_unload(stage);
		renderer.render_target_destroy(scene_target);
	}
	//两条路径共用同一录制实现 → 画面应逐字节相同
	int difference = max_channel_diff(reference, chained);
	EXPECT_EQ(difference, 0) << "链长 1 与既有单级默认路径画面不一致（最大通道差 " << difference << "）";
}

/*
目标尺寸与交换链不一致**不再回落**（第十七轮放宽；原 R-FACADE-08 / R-FRAME-09 的 1:1 要求已解除）
用比交换链窄 8 像素的目标作为「同帧后处理呈现」目标 ＋ 单级反色链：本帧应**仍在后处理路径上**出帧，
故中心（白四边形）被反色成黑、角落（清屏色）被反色成亮。
*/
TEST_F(Renderer_Post_Chain_Test, 目标尺寸不一致仍走后处理)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//比交换链窄的目标（触发范围不一致）
	uint64_t narrow_target = 0;
	ASSERT_TRUE(renderer.render_target_create(narrow_target, renderer.extent_width() - 8, renderer.extent_height()));
	//链长 1 的反色链（若误走后处理路径则中心会被反色成黑）
	uint64_t invert = post_pipeline_register(renderer, "反色.frag.spv");
	ASSERT_NE(invert, 0u);
	ASSERT_TRUE(renderer.post_chain_set(&invert, 1));
	engine::Mesh_Handle handle = 0;
	scene_register(renderer, handle);
	renderer.render_target_present_set(narrow_target);
	std::vector<uint8_t> pixels(pixel_bytes());
	//本帧应在后处理路径上正常出帧（尺寸不一致不再拒绝）
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	EXPECT_EQ(renderer.frame_count(), 1u);
	//走了后处理：中心被反色成黑、角落被反色成亮
	std::array<uint8_t, 4> center = pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	EXPECT_LE(*std::max_element(center.begin(), center.begin() + 3), DARK_THRESHOLD)
		<< "中心未被反色（尺寸不一致时仍回落了原路径）：" << pixel_text(center);
	std::array<uint8_t, 4> corner = pixel_get(pixels, 1, 1);
	EXPECT_GE(*std::min_element(corner.begin(), corner.begin() + 3), 200)
		<< "角落未被反色：" << pixel_text(corner);
	renderer.mesh_unload(handle);
	renderer.pipeline_unload(invert);
	renderer.render_target_destroy(narrow_target);
}

/*
链状态只读诊断入口（post_chain_count / post_chain_get）
断言「计数与级序可读」「卸载中间级后级序压缩」「容量截断与空指针」三件事；
纯只读接口，不改变渲染行为。
*/
TEST_F(Renderer_Post_Chain_Test, 链计数与级序可读)
{
	engine::Renderer renderer;
	//未初始化时一律 0
	EXPECT_EQ(renderer.post_chain_count(), 0u);
	uint64_t probe[4] = { 0, 0, 0, 0 };
	EXPECT_EQ(renderer.post_chain_get(probe, 4), 0u);
	ASSERT_TRUE(renderer.init(hidden_config()));
	//初始化后仍未设链
	EXPECT_EQ(renderer.post_chain_count(), 0u);

	//登记三条后处理管线
	uint64_t invert = post_pipeline_register(renderer, "反色.frag.spv");
	uint64_t threshold = post_pipeline_register(renderer, "亮度阈值.frag.spv");
	uint64_t invert_again = post_pipeline_register(renderer, "反色.frag.spv");
	ASSERT_NE(invert, 0u);
	ASSERT_NE(threshold, 0u);
	ASSERT_NE(invert_again, 0u);
	ASSERT_NE(invert, threshold);
	ASSERT_NE(threshold, invert_again);

	//设三级链
	uint64_t chain[3] = { invert, threshold, invert_again };
	ASSERT_TRUE(renderer.post_chain_set(chain, 3));
	EXPECT_EQ(renderer.post_chain_count(), 3u);
	//逐级读回须与设置顺序一致
	uint64_t read_back[4] = { 0, 0, 0, 0 };
	ASSERT_EQ(renderer.post_chain_get(read_back, 4), 3u);
	EXPECT_EQ(read_back[0], invert);
	EXPECT_EQ(read_back[1], threshold);
	EXPECT_EQ(read_back[2], invert_again);

	//容量不足：按容量截断，且返回实际写入数
	uint64_t single[2] = { 0, 0 };
	ASSERT_EQ(renderer.post_chain_get(single, 1), 1u);
	EXPECT_EQ(single[0], invert);
	//出参为空：返回 0
	EXPECT_EQ(renderer.post_chain_get(nullptr, 4), 0u);

	//卸载中间一级 → 该句柄被摘除、后续级序前移（计数 3 → 2）
	renderer.pipeline_unload(threshold);
	EXPECT_EQ(renderer.post_chain_count(), 2u);
	uint64_t compressed[4] = { 0, 0, 0, 0 };
	ASSERT_EQ(renderer.post_chain_get(compressed, 4), 2u);
	EXPECT_EQ(compressed[0], invert);
	EXPECT_EQ(compressed[1], invert_again);

	//清空链 → 计数归 0
	ASSERT_TRUE(renderer.post_chain_set(nullptr, 0));
	EXPECT_EQ(renderer.post_chain_count(), 0u);
	EXPECT_EQ(renderer.post_chain_get(compressed, 4), 0u);
	renderer.pipeline_unload(invert);
	renderer.pipeline_unload(invert_again);
}

/*
后处理预设 · 灰度：三通道趋同（去色），白四边形与清屏色均被去色
*/
TEST_F(Renderer_Post_Chain_Test, 灰度预设去色生效)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t scene_target = 0;
	ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
	uint64_t gray = post_pipeline_register(renderer, "灰度.frag.spv");
	ASSERT_NE(gray, 0u);
	ASSERT_TRUE(renderer.post_chain_set(&gray, 1));
	engine::Mesh_Handle handle = 0;
	scene_register(renderer, handle);
	renderer.render_target_present_set(scene_target);
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));

	//中心（白四边形）与角落（清屏色）都应三通道趋同
	std::array<uint8_t, 4> center = pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	std::array<uint8_t, 4> corner = pixel_get(pixels, 1, 1);
	EXPECT_LE(std::abs(static_cast<int>(center[0]) - static_cast<int>(center[1])), 2) << "中心 r/g 未趋同：" << pixel_text(center);
	EXPECT_LE(std::abs(static_cast<int>(center[1]) - static_cast<int>(center[2])), 2) << "中心 g/b 未趋同：" << pixel_text(center);
	EXPECT_LE(std::abs(static_cast<int>(corner[0]) - static_cast<int>(corner[1])), 2) << "角落 r/g 未趋同：" << pixel_text(corner);
	EXPECT_LE(std::abs(static_cast<int>(corner[1]) - static_cast<int>(corner[2])), 2) << "角落 g/b 未趋同：" << pixel_text(corner);
	//白四边形去色后仍应亮
	EXPECT_GE(center[0], 200) << "去色后中心过暗：" << pixel_text(center);
	EXPECT_GT(corner[0], 4) << "角落去色后被压成纯黑（应保留清屏色亮度）：" << pixel_text(corner);

	renderer.mesh_unload(handle);
	renderer.pipeline_unload(gray);
	renderer.render_target_destroy(scene_target);
}

/*
后处理预设 · 描边：与灰度帧对比，边界处出现明显压暗（内部差异）
口径：以「同一场景、同一目标的灰度帧」为参考，统计「比参考暗至少 40」的像素数 > 0，
      且中心（无边缘区域）不被大幅压暗 —— 避免依赖具体边界坐标。
*/
TEST_F(Renderer_Post_Chain_Test, 描边预设压暗边界)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t scene_target = 0;
	ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
	uint64_t gray = post_pipeline_register(renderer, "灰度.frag.spv");
	uint64_t edge = post_pipeline_register(renderer, "描边.frag.spv");
	ASSERT_NE(gray, 0u);
	ASSERT_NE(edge, 0u);
	engine::Mesh_Handle handle = 0;
	engine::Mesh_Handle scene_mesh = 0;
	scene_register(renderer, scene_mesh);
	handle = scene_mesh;
	renderer.render_target_present_set(scene_target);

	//第一帧：灰度（作为参考）
	ASSERT_TRUE(renderer.post_chain_set(&gray, 1));
	std::vector<uint8_t> gray_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(gray_pixels.data(), static_cast<uint32_t>(gray_pixels.size())));

	//第二帧：描边（同一场景重登记绘制项）
	ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
	ASSERT_TRUE(renderer.post_chain_set(&edge, 1));
	std::vector<uint8_t> edge_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(edge_pixels.data(), static_cast<uint32_t>(edge_pixels.size())));

	//边界处应出现明显压暗像素
	uint32_t darkened = darkened_count(edge_pixels, gray_pixels, 40.0f);
	RecordProperty("edge_darkened_pixels", darkened);
	EXPECT_GT(darkened, 0u) << "描边未压暗任何像素（与灰度帧完全一致）";
	//中心（远离四边形边界）不应被明显压暗
	std::array<uint8_t, 4> gray_center = pixel_get(gray_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	std::array<uint8_t, 4> edge_center = pixel_get(edge_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	EXPECT_LE(luma_of(gray_center) - luma_of(edge_center), 20.0f)
		<< "中心被过度压暗（描边泄漏到内部）：灰 " << pixel_text(gray_center) << " / 边 " << pixel_text(edge_center);

	renderer.mesh_unload(scene_mesh);
	renderer.pipeline_unload(gray);
	renderer.pipeline_unload(edge);
	renderer.render_target_destroy(scene_target);
}

/*
后处理预设 · 色调映射：高光被压缩（白 255 → 约 230），不再顶到最大亮度
*/
TEST_F(Renderer_Post_Chain_Test, 色调映射压缩高亮)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t scene_target = 0;
	ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
	uint64_t tone = post_pipeline_register(renderer, "色调映射.frag.spv");
	ASSERT_NE(tone, 0u);
	ASSERT_TRUE(renderer.post_chain_set(&tone, 1));
	engine::Mesh_Handle handle = 0;
	scene_register(renderer, handle);
	renderer.render_target_present_set(scene_target);
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));

	//白四边形（输入线性 1.0）经简化 ACES 后约 0.80 → sRGB 编码约 232
	std::array<uint8_t, 4> center = pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	RecordProperty("tone_mapped_center", static_cast<int>(center[0]));
	EXPECT_GE(center[0], 200) << "高光被压缩过头：" << pixel_text(center);
	EXPECT_LE(center[0], 246) << "高光未被压缩（仍接近 255）：" << pixel_text(center);
	EXPECT_LE(std::abs(static_cast<int>(center[0]) - static_cast<int>(center[1])), 2);

	renderer.mesh_unload(handle);
	renderer.pipeline_unload(tone);
	renderer.render_target_destroy(scene_target);
}

/*
后处理预设可串联：灰度 → 描边 两级链可正常出帧，且中心仍为去色后的亮色
*/
TEST_F(Renderer_Post_Chain_Test, 预设可串联)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t scene_target = 0;
	uint64_t second_target = 0;
	ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
	ASSERT_TRUE(renderer.render_target_create(second_target, renderer.extent_width(), renderer.extent_height()));
	uint64_t gray = post_pipeline_register(renderer, "灰度.frag.spv");
	uint64_t edge = post_pipeline_register(renderer, "描边.frag.spv");
	ASSERT_NE(gray, 0u);
	ASSERT_NE(edge, 0u);
	uint64_t chain[2] = { gray, edge };
	ASSERT_TRUE(renderer.post_chain_set(chain, 2));
	engine::Mesh_Handle handle = 0;
	scene_register(renderer, handle);
	renderer.render_target_present_set(scene_target);
	uint64_t chain_targets[2] = { scene_target, second_target };
	ASSERT_TRUE(renderer.render_target_present_chain_set(chain_targets, 2));
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	EXPECT_EQ(renderer.frame_count(), 1u);

	//中心仍应为去色后的亮色（两级链未破坏内部）
	std::array<uint8_t, 4> center = pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	EXPECT_GE(luma_of(center), 180.0f) << "两级链后中心过暗：" << pixel_text(center);
	EXPECT_LE(std::abs(static_cast<int>(center[0]) - static_cast<int>(center[2])), 4)
		<< "两级链后中心未保持去色：" << pixel_text(center);

	renderer.mesh_unload(handle);
	renderer.pipeline_unload(gray);
	renderer.pipeline_unload(edge);
	renderer.render_target_destroy(second_target);
	renderer.render_target_destroy(scene_target);
}
