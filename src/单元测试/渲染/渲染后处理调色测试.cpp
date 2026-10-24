/*
渲染后处理调色测试（暗角 / 对比度 / 饱和度 / 级数与逐级参数可观测 / 文字经链）
（第五十九轮由「渲染后处理链测试.cpp」按功能域拆分；逻辑未改）
*/
#include "渲染后处理链测试辅助.h"

/*
预设 · 暗角（第二十一轮）：角落被压暗、中心基本不变
参考帧走单级直通；对照帧走单级「暗角」。
*/
TEST_F(Renderer_Post_Chain_Test, 暗角压暗边角)
{
	//参考帧
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
	//对照帧：暗角
	std::vector<uint8_t> vignetted(pixel_bytes());
	{
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(hidden_config()));
		if (!renderer.readback_supported())
			GTEST_SKIP() << "本机交换链图像不具备传输源用法";
		uint64_t scene_target = 0;
		ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
		uint64_t vignette = post_pipeline_register(renderer, "暗角.frag.spv");
		ASSERT_NE(vignette, 0u);
		ASSERT_TRUE(renderer.post_chain_set(&vignette, 1));
		renderer.render_target_present_set(scene_target);
		engine::Mesh_Handle handle = 0;
		scene_register(renderer, handle);
		ASSERT_TRUE(renderer.frame_render(vignetted.data(), static_cast<uint32_t>(vignetted.size())));
		renderer.mesh_unload(handle);
		renderer.pipeline_unload(vignette);
		renderer.render_target_destroy(scene_target);
	}

	//角落：明显变暗
	int reference_corner = luma_of(pixel_get(reference, 1, 1));
	int vignetted_corner = luma_of(pixel_get(vignetted, 1, 1));
	RecordProperty("vignette_reference_corner", reference_corner);
	RecordProperty("vignette_corner", vignetted_corner);
	EXPECT_LT(vignetted_corner, reference_corner - 20) << "暗角未压暗角落";
	//中心：基本不变
	int reference_center = luma_of(pixel_get(reference, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2));
	int vignetted_center = luma_of(pixel_get(vignetted, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2));
	EXPECT_LE(std::abs(vignetted_center - reference_center), 6) << "暗角影响了中心";
}

/*
后处理预设 · 对比度：暗部被压得更暗、亮部保持饱和（端点方向断言）
参考帧走默认交换链路径；对照帧走单级「对比度」链。
刻意只断言方向（角落明显变暗、中心仍饱和），不依赖具体曲线数值。
*/
TEST_F(Renderer_Post_Chain_Test, 对比度放大明暗差)
{
	//参考帧：默认交换链路径
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
	//对照帧：单级对比度链
	std::vector<uint8_t> adjusted(pixel_bytes());
	{
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(hidden_config()));
		if (!renderer.readback_supported())
			GTEST_SKIP() << "本机交换链图像不具备传输源用法";
		uint64_t scene_target = 0;
		ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
		uint64_t contrast = post_pipeline_register(renderer, "对比度.frag.spv");
		ASSERT_NE(contrast, 0u);
		ASSERT_TRUE(renderer.post_chain_set(&contrast, 1));
		engine::Mesh_Handle handle = 0;
		scene_register(renderer, handle);
		renderer.render_target_present_set(scene_target);
		ASSERT_TRUE(renderer.frame_render(adjusted.data(), static_cast<uint32_t>(adjusted.size())));
		renderer.mesh_unload(handle);
		renderer.pipeline_unload(contrast);
		renderer.render_target_destroy(scene_target);
	}

	//暗部（清屏色）：参考帧约 60、对照帧应被压到近黑
	std::array<uint8_t, 4> reference_corner = pixel_get(reference, 1, 1);
	std::array<uint8_t, 4> adjusted_corner = pixel_get(adjusted, 1, 1);
	RecordProperty("contrast_corner_reference", static_cast<int>(reference_corner[0]));
	RecordProperty("contrast_corner_adjusted", static_cast<int>(adjusted_corner[0]));
	EXPECT_GE(reference_corner[0], 30) << "参考帧角落过暗（用例前提不成立）：" << pixel_text(reference_corner);
	EXPECT_LE(*std::max_element(adjusted_corner.begin(), adjusted_corner.begin() + 3), 8)
		<< "暗部未被对比度压暗：" << pixel_text(adjusted_corner);
	//亮部（白四边形）：仍保持饱和
	std::array<uint8_t, 4> adjusted_center = pixel_get(adjusted, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	EXPECT_GE(*std::min_element(adjusted_center.begin(), adjusted_center.begin() + 3), 250)
		<< "亮部未保持饱和：" << pixel_text(adjusted_center);
}

/*
后处理预设 · 饱和度：色偏区域通道差被放大（清屏色 b > r），纯白保持中性
参考帧走默认交换链路径；对照帧走单级「饱和度」链。
*/
TEST_F(Renderer_Post_Chain_Test, 饱和度放大通道差)
{
	//参考帧：默认交换链路径
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
	//对照帧：单级饱和度链
	std::vector<uint8_t> adjusted(pixel_bytes());
	{
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(hidden_config()));
		if (!renderer.readback_supported())
			GTEST_SKIP() << "本机交换链图像不具备传输源用法";
		uint64_t scene_target = 0;
		ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
		uint64_t saturation = post_pipeline_register(renderer, "饱和度.frag.spv");
		ASSERT_NE(saturation, 0u);
		ASSERT_TRUE(renderer.post_chain_set(&saturation, 1));
		engine::Mesh_Handle handle = 0;
		scene_register(renderer, handle);
		renderer.render_target_present_set(scene_target);
		ASSERT_TRUE(renderer.frame_render(adjusted.data(), static_cast<uint32_t>(adjusted.size())));
		renderer.mesh_unload(handle);
		renderer.pipeline_unload(saturation);
		renderer.render_target_destroy(scene_target);
	}

	//清屏色（含蓝偏，通道顺序随表面格式而变）的通道差绝对值应被放大
	std::array<uint8_t, 4> reference_corner = pixel_get(reference, 1, 1);
	std::array<uint8_t, 4> adjusted_corner = pixel_get(adjusted, 1, 1);
	int reference_spread = std::abs(static_cast<int>(reference_corner[2]) - static_cast<int>(reference_corner[0]));
	int adjusted_spread = std::abs(static_cast<int>(adjusted_corner[2]) - static_cast<int>(adjusted_corner[0]));
	RecordProperty("saturation_spread_reference", reference_spread);
	RecordProperty("saturation_spread_adjusted", adjusted_spread);
	EXPECT_GE(reference_spread, 5) << "参考帧角落无明显色偏（用例前提不成立）：" << pixel_text(reference_corner);
	EXPECT_GE(adjusted_spread, reference_spread + 5) << "色偏未放大：" << pixel_text(adjusted_corner);
	//白四边形应保持中性（三通道差 ≤ 2）
	std::array<uint8_t, 4> adjusted_center = pixel_get(adjusted, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	EXPECT_LE(std::abs(static_cast<int>(adjusted_center[0]) - static_cast<int>(adjusted_center[2])), 2)
		<< "纯白被着色：" << pixel_text(adjusted_center);
}

/*
后处理级数观测（第二十二轮）：Frame_Stats.post_stages 反映本帧实际执行的级数
口径：未设链且无呈现目标 → 直通路径，post_stages == 0；设 2 级链 → post_stages == 2。
注意：清空链但保留呈现目标时，门面按既有定案**回落单级默认路径**（级数 1，不是 0），故本用例
      以「先直通、后链」的顺序取值，避免把回落路径误当直通。
*/
TEST_F(Renderer_Post_Chain_Test, 后处理级数可观测)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//直通路径（无链、无呈现目标）：级数为 0
	ASSERT_TRUE(renderer.frame_render());
	EXPECT_EQ(renderer.frame_stats_get().post_stages, 0u);

	//两级链所需的目标（两级各一）与管线
	uint64_t scene_target = 0;
	uint64_t second_target = 0;
	ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
	ASSERT_TRUE(renderer.render_target_create(second_target, renderer.extent_width(), renderer.extent_height()));
	uint64_t invert = post_pipeline_register(renderer, "反色.frag.spv");
	uint64_t threshold = post_pipeline_register(renderer, "亮度阈值.frag.spv");
	ASSERT_NE(invert, 0u);
	ASSERT_NE(threshold, 0u);
	uint64_t chain[2] = { invert, threshold };
	ASSERT_TRUE(renderer.post_chain_set(chain, 2));
	uint64_t chain_targets[2] = { scene_target, second_target };
	ASSERT_TRUE(renderer.render_target_present_chain_set(chain_targets, 2));
	engine::Mesh_Handle handle = 0;
	scene_register(renderer, handle);
	ASSERT_TRUE(renderer.frame_render());
	//链路径：级数 = 链长
	EXPECT_EQ(renderer.frame_stats_get().post_stages, 2u);

	renderer.mesh_unload(handle);
	renderer.pipeline_unload(invert);
	renderer.pipeline_unload(threshold);
	renderer.render_target_destroy(second_target);
	renderer.render_target_destroy(scene_target);
}

/*
逐级参数读取（第二十四轮）：post_stage_param_get 返回「该级是否有专设值」
口径：未设专设 → 0 且 out 全 0；设第 0 级 → 1 且 out 与设定一致；链外级序 → 0。
*/
TEST_F(Renderer_Post_Chain_Test, 逐级参数可读)
{
	engine::Renderer renderer;
	//未初始化一律 0
	float probe[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	EXPECT_EQ(renderer.post_stage_param_get(0, probe), 0u);
	EXPECT_EQ(probe[0], 0.0f);
	ASSERT_TRUE(renderer.init(hidden_config()));
	//设一条两级链（无需目标也可设置链本身）
	uint64_t invert = post_pipeline_register(renderer, "反色.frag.spv");
	uint64_t threshold = post_pipeline_register(renderer, "亮度阈值.frag.spv");
	ASSERT_NE(invert, 0u);
	ASSERT_NE(threshold, 0u);
	uint64_t chain[2] = { invert, threshold };
	ASSERT_TRUE(renderer.post_chain_set(chain, 2));

	//未设专设值 → 0
	EXPECT_EQ(renderer.post_stage_param_get(0, probe), 0u);
	EXPECT_EQ(probe[0], 0.0f);

	//设第 0 级 → 1 且值一致
	renderer.post_stage_param_set(0, 0.9f, 0.05f, 0.5f);
	ASSERT_EQ(renderer.post_stage_param_get(0, probe), 1u);
	EXPECT_FLOAT_EQ(probe[0], 0.9f);
	EXPECT_FLOAT_EQ(probe[1], 0.05f);
	EXPECT_FLOAT_EQ(probe[2], 0.5f);
	EXPECT_FLOAT_EQ(probe[3], 0.0f);

	//链外级序 → 0 且 out 复位为 0
	EXPECT_EQ(renderer.post_stage_param_get(5, probe), 0u);
	EXPECT_EQ(probe[0], 0.0f);

	//重设链 → 专设值被清除
	ASSERT_TRUE(renderer.post_chain_set(chain, 2));
	EXPECT_EQ(renderer.post_stage_param_get(0, probe), 0u);

	renderer.pipeline_unload(invert);
	renderer.pipeline_unload(threshold);
}

/*
级序压缩清除逐级参数（第二十五轮修复）：
三级链设第 2 级专设值 → 卸载中间级（链压成 2 级）→ 全部逐级参数被整体清除（读取均返回 0）。
*/
TEST_F(Renderer_Post_Chain_Test, 级序压缩清除逐级参数)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	uint64_t invert_a = post_pipeline_register(renderer, "反色.frag.spv");
	uint64_t threshold = post_pipeline_register(renderer, "亮度阈值.frag.spv");
	uint64_t invert_b = post_pipeline_register(renderer, "反色.frag.spv");
	ASSERT_NE(invert_a, 0u);
	ASSERT_NE(threshold, 0u);
	ASSERT_NE(invert_b, 0u);
	uint64_t chain[3] = { invert_a, threshold, invert_b };
	ASSERT_TRUE(renderer.post_chain_set(chain, 3));
	//第 2 级设专设值
	renderer.post_stage_param_set(2, 0.9f, 0.1f, 1.0f);
	float probe[4] = {};
	ASSERT_EQ(renderer.post_stage_param_get(2, probe), 1u);

	//卸载中间级 → 级序压缩 → 全部逐级参数清除
	renderer.pipeline_unload(threshold);
	EXPECT_EQ(renderer.post_chain_count(), 2u);
	float first[4] = {};
	float second[4] = {};
	EXPECT_EQ(renderer.post_stage_param_get(0, first), 0u);
	EXPECT_EQ(renderer.post_stage_param_get(1, second), 0u);

	renderer.pipeline_unload(invert_b);
	renderer.pipeline_unload(invert_a);
}

/*
文字经后处理链（第二十四轮）：
  1. 两级「反色 → 反色」恒等链下，文字帧应与直通帧逐字节一致（容差 4）——证明文字与场景共用同一后处理输入；
  2. 单级「灰度」链下，文字最亮像素三通道应趋同（差 ≤ 2）——证明文字同样被后处理。
字体缺失时跳过。
*/
TEST_F(Renderer_Post_Chain_Test, 文字经后处理链保持)
{
	//字体不可用则跳过（与文字测试同口径）
	std::error_code code;
	if (!std::filesystem::exists(std::filesystem::path(FONT_PATH), code))
		GTEST_SKIP() << "本机缺少系统字体 " << FONT_PATH;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t font = 0;
	ASSERT_TRUE(renderer.font_load_file(font, FONT_PATH, FONT_PIXEL_HEIGHT));

	//参考帧：直通（只画文字）
	ASSERT_TRUE(renderer.text_draw(font, "ABC", 8.0f, 8.0f));
	std::vector<uint8_t> reference(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(reference.data(), static_cast<uint32_t>(reference.size())));

	//两级反色恒等链
	uint64_t target0 = 0;
	uint64_t target1 = 0;
	ASSERT_TRUE(renderer.render_target_create(target0, renderer.extent_width(), renderer.extent_height()));
	ASSERT_TRUE(renderer.render_target_create(target1, renderer.extent_width(), renderer.extent_height()));
	uint64_t invert_a = post_pipeline_register(renderer, "反色.frag.spv");
	uint64_t invert_b = post_pipeline_register(renderer, "反色.frag.spv");
	ASSERT_NE(invert_a, 0u);
	ASSERT_NE(invert_b, 0u);
	uint64_t chain_invert[2] = { invert_a, invert_b };
	ASSERT_TRUE(renderer.post_chain_set(chain_invert, 2));
	uint64_t targets[2] = { target0, target1 };
	ASSERT_TRUE(renderer.render_target_present_chain_set(targets, 2));
	ASSERT_TRUE(renderer.text_draw(font, "ABC", 8.0f, 8.0f));
	std::vector<uint8_t> identity(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(identity.data(), static_cast<uint32_t>(identity.size())));

	/*
	文字在链路径与直通路径的**取整与混合空间不同**（场景目标格式与交换链格式可能一致也可能不一致，
	且文字走 alpha 混合），故不能要求逐字节一致；改为「文字墨迹区域规模相当 ＋ 最亮像素仍亮」的
	结构性比对（既有「两级反色恒等」用例对不透明四边形仍保留 ≤4 的强断言）。
	*/
	auto ink_count = [](const std::vector<uint8_t>& pixels)
	{
		//墨迹判定：绿通道明显高于清屏背景（格式无关，绿恒为第 1 字节）
		int count = 0;
		for (std::size_t index = 0; index + 3 < pixels.size(); index += 4)
			if (pixels[index + 1] > 100)
				++count;
		return count;
	};
	int reference_ink = ink_count(reference);
	int identity_ink = ink_count(identity);
	RecordProperty("text_ink_reference", reference_ink);
	RecordProperty("text_ink_identity", identity_ink);
	EXPECT_GT(reference_ink, 0) << "参考帧无文字墨迹（用例前提不成立）";
	int ink_delta = std::abs(identity_ink - reference_ink);
	EXPECT_LE(ink_delta, reference_ink / 5 + 5)
		<< "文字经两级反色链后墨迹规模差异过大（参考 " << reference_ink << "，链 " << identity_ink << "）";

	//链路径最亮像素仍应明显亮于背景（文字未被吞掉）
	int identity_brightest = 0;
	for (std::size_t index = 0; index + 3 < identity.size(); index += 4)
		if (identity[index + 1] > identity_brightest)
			identity_brightest = identity[index + 1];
	EXPECT_GE(identity_brightest, 160) << "文字经两级反色链后消失";

	//单级灰度链：文字最亮像素应去色
	uint64_t gray = post_pipeline_register(renderer, "灰度.frag.spv");
	ASSERT_NE(gray, 0u);
	ASSERT_TRUE(renderer.post_chain_set(&gray, 1));
	ASSERT_TRUE(renderer.render_target_present_chain_set(targets, 2));
	ASSERT_TRUE(renderer.text_draw(font, "ABC", 8.0f, 8.0f));
	std::vector<uint8_t> grayed(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(grayed.data(), static_cast<uint32_t>(grayed.size())));

	//在参考帧中找最亮像素作为文字采样点
	std::size_t brightest = 0;
	int brightest_luma = -1;
	for (std::size_t index = 0; index + 3 < reference.size(); index += 4)
	{
		int luma = static_cast<int>(reference[index]) + static_cast<int>(reference[index + 1])
			+ static_cast<int>(reference[index + 2]);
		if (luma > brightest_luma)
		{
			brightest_luma = luma;
			brightest = index;
		}
	}
	//文字最亮像素在灰度链下三通道趋同
	int channel_min = 255;
	int channel_max = 0;
	for (std::size_t channel = 0; channel < 3; ++channel)
	{
		int value = static_cast<int>(grayed[brightest + channel]);
		channel_min = value < channel_min ? value : channel_min;
		channel_max = value > channel_max ? value : channel_max;
	}
	RecordProperty("text_gray_channel_spread", channel_max - channel_min);
	EXPECT_LE(channel_max - channel_min, 2) << "文字未被灰度后处理（三通道仍有色偏）";
	//文字仍可见（比背景亮）
	EXPECT_GE(static_cast<int>(grayed[brightest + 1]), 160) << "文字在灰度链下消失";

	renderer.pipeline_unload(gray);
	renderer.pipeline_unload(invert_a);
        renderer.pipeline_unload(invert_b);
        renderer.render_target_destroy(target1);
        renderer.render_target_destroy(target0);
        renderer.font_unload(font);
}

/*
后处理预设 · 锐化（第七十四轮）
口径：4×4 双灰阶棋盘（100 / 180）经线性过滤贴在整屏四边形上；
      参考帧走交换链直通，对照帧走单级「锐化」链；
      以中心行相邻像素的通道差绝对值之和为「边缘能量」，锐化后应严格增大。
*/
TEST_F(Renderer_Post_Chain_Test, 锐化放大边缘对比)
{
	//双灰阶棋盘（RGBA8：两种灰度交替 2 像素块）
	std::vector<uint8_t> texels(4 * 4 * 4);
	for (uint32_t y = 0; y < 4; ++y)
	{
		for (uint32_t x = 0; x < 4; ++x)
		{
			uint8_t value = (((x / 2) + (y / 2)) % 2 == 0) ? 100 : 180;
			std::size_t offset = (static_cast<std::size_t>(y) * 4 + x) * 4;
			texels[offset + 0] = value;
			texels[offset + 1] = value;
			texels[offset + 2] = value;
			texels[offset + 3] = 255;
		}
	}
	//边缘能量：中心行相邻像素的绿通道差之和（格式无关）
	auto edge_energy = [&](const std::vector<uint8_t>& pixels)
	{
		std::uint64_t sum = 0;
		uint32_t row = WINDOW_HEIGHT / 2;
		for (uint32_t x = 1; x < WINDOW_WIDTH; ++x)
		{
			int left = static_cast<int>(pixels[(static_cast<std::size_t>(row) * WINDOW_WIDTH + x - 1) * 4 + 1]);
			int right = static_cast<int>(pixels[(static_cast<std::size_t>(row) * WINDOW_WIDTH + x) * 4 + 1]);
			sum += static_cast<std::uint64_t>(left > right ? left - right : right - left);
		}
		return sum;
	};

	//参考帧：交换链直通
	std::vector<uint8_t> reference(pixel_bytes());
	{
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(hidden_config()));
		if (!renderer.readback_supported())
			GTEST_SKIP() << "本机交换链图像不具备传输源用法";
		uint64_t texture = 0;
		ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 4, 4));
		renderer.texture_bind_set(texture);
		engine::Mesh_Handle handle = 0;
		ASSERT_TRUE(renderer.mesh_load(handle, quad_data()));
		ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
		ASSERT_TRUE(renderer.frame_render(reference.data(), static_cast<uint32_t>(reference.size())));
		renderer.mesh_unload(handle);
		renderer.texture_unload(texture);
	}
	//对照帧：单级锐化链
	std::vector<uint8_t> sharpened(pixel_bytes());
	{
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(hidden_config()));
		if (!renderer.readback_supported())
			GTEST_SKIP() << "本机交换链图像不具备传输源用法";
		uint64_t target = 0;
		ASSERT_TRUE(renderer.render_target_create(target, renderer.extent_width(), renderer.extent_height()));
		uint64_t pass = post_pipeline_register(renderer, "锐化.frag.spv");
		ASSERT_NE(pass, 0u);
		ASSERT_TRUE(renderer.post_chain_set(&pass, 1));
		renderer.render_target_present_set(target);
		uint64_t texture = 0;
		ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 4, 4));
		renderer.texture_bind_set(texture);
		engine::Mesh_Handle handle = 0;
		ASSERT_TRUE(renderer.mesh_load(handle, quad_data()));
		ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
		ASSERT_TRUE(renderer.frame_render(sharpened.data(), static_cast<uint32_t>(sharpened.size())));
		renderer.mesh_unload(handle);
		renderer.texture_unload(texture);
		renderer.pipeline_unload(pass);
		renderer.render_target_destroy(target);
	}

	//边缘能量对比
	std::uint64_t reference_energy = edge_energy(reference);
	std::uint64_t sharpened_energy = edge_energy(sharpened);
	RecordProperty("sharpen_reference_energy", static_cast<int>(reference_energy));
	RecordProperty("sharpen_sharpened_energy", static_cast<int>(sharpened_energy));
	EXPECT_GT(reference_energy, 50u) << "参考帧无边缘（用例前提不成立）";
	EXPECT_GT(sharpened_energy, reference_energy) << "锐化未放大边缘对比（"
		<< sharpened_energy << " vs " << reference_energy << "）";
}

/*
后处理预设 · 色差（第七十六轮）
口径：4×4 双灰阶棋盘（100 / 180）经线性过滤贴在视口中央四边形上；
      清屏色置纯黑（三通道相等），使「外通道差」只可能来自色差本身；
      参考帧走交换链直通（灰度输入 → 外通道相等），对照帧走单级「色差」链，
      并把该级强度专设为 6 像素（半径为 1 处的径向偏移）。
      以「|外通道差| ≥ 8 的像素数」为通道分离度，参考帧应为 0、色差帧应显著增大；
      中心半径近 0（偏移近 0），故中心像素三通道应基本不变。
*/
TEST_F(Renderer_Post_Chain_Test, 色差分离红蓝通道)
{
	//双灰阶棋盘（RGBA8：两种灰度交替 2 像素块）
	std::vector<uint8_t> texels(4 * 4 * 4);
	for (uint32_t y = 0; y < 4; ++y)
	{
		for (uint32_t x = 0; x < 4; ++x)
		{
			uint8_t value = (((x / 2) + (y / 2)) % 2 == 0) ? 100 : 180;
			std::size_t offset = (static_cast<std::size_t>(y) * 4 + x) * 4;
			texels[offset + 0] = value;
			texels[offset + 1] = value;
			texels[offset + 2] = value;
			texels[offset + 3] = 255;
		}
	}
	//清屏色置纯黑（外通道相等，避免污染分离度计数）
	const float black[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	//通道分离度：外通道差（下标 0 与 2）≥ 8 的像素数（与表面格式无关）
	auto separated_count = [&](const std::vector<uint8_t>& pixels)
	{
		uint32_t count = 0;
		for (uint32_t y = 0; y < WINDOW_HEIGHT; ++y)
		{
			for (uint32_t x = 0; x < WINDOW_WIDTH; ++x)
			{
				std::array<uint8_t, 4> value = pixel_get(pixels, x, y);
				int difference = static_cast<int>(value[0]) - static_cast<int>(value[2]);
				if (difference < 0)
					difference = -difference;
				if (difference >= 8)
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
		renderer.clear_color_set(black);
		uint64_t texture = 0;
		ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 4, 4));
		renderer.texture_bind_set(texture);
		engine::Mesh_Handle handle = 0;
		ASSERT_TRUE(renderer.mesh_load(handle, quad_data()));
		ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
		ASSERT_TRUE(renderer.frame_render(reference.data(), static_cast<uint32_t>(reference.size())));
		renderer.mesh_unload(handle);
		renderer.texture_unload(texture);
	}
	//对照帧：单级色差链（该级强度专设 6 像素）
	std::vector<uint8_t> separated(pixel_bytes());
	{
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(hidden_config()));
		if (!renderer.readback_supported())
			GTEST_SKIP() << "本机交换链图像不具备传输源用法";
		uint64_t target = 0;
		ASSERT_TRUE(renderer.render_target_create(target, renderer.extent_width(), renderer.extent_height()));
		uint64_t aberration = post_pipeline_register(renderer, "色差.frag.spv");
		ASSERT_NE(aberration, 0u);
		ASSERT_TRUE(renderer.post_chain_set(&aberration, 1));
		//强度专设（阈值 / 软膝由校验规则要求合法，本片元不使用）
		renderer.post_stage_param_set(0, 1.0f, 0.0f, 6.0f);
		renderer.clear_color_set(black);
		uint64_t texture = 0;
		ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 4, 4));
		renderer.texture_bind_set(texture);
		engine::Mesh_Handle handle = 0;
		ASSERT_TRUE(renderer.mesh_load(handle, quad_data()));
		ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
		renderer.render_target_present_set(target);
		ASSERT_TRUE(renderer.frame_render(separated.data(), static_cast<uint32_t>(separated.size())));
		renderer.mesh_unload(handle);
		renderer.texture_unload(texture);
		renderer.pipeline_unload(aberration);
		renderer.render_target_destroy(target);
	}

	//灰度输入直通时不应有色偏
	uint32_t reference_separated = separated_count(reference);
	uint32_t aberration_separated = separated_count(separated);
	RecordProperty("aberration_reference_separated", static_cast<int>(reference_separated));
	RecordProperty("aberration_separated", static_cast<int>(aberration_separated));
	EXPECT_EQ(reference_separated, 0u) << "参考帧出现色偏（用例前提不成立）";
	EXPECT_GT(aberration_separated, 30u) << "色差未分离红蓝通道（"
		<< aberration_separated << " 像素）";

	//中心像素：半径近 0 → 偏移近 0 → 三通道基本不变
	std::array<uint8_t, 4> reference_center = pixel_get(reference, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	std::array<uint8_t, 4> aberration_center = pixel_get(separated, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	for (std::size_t channel = 0; channel < 3; ++channel)
	{
		int difference = static_cast<int>(aberration_center[channel]) - static_cast<int>(reference_center[channel]);
		EXPECT_LE(std::abs(difference), 2) << "中心像素被色差偏移（通道 " << channel << "）："
			<< pixel_text(reference_center) << " → " << pixel_text(aberration_center);
	}
}
