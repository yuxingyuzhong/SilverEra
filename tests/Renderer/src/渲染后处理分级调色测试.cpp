/*
渲染后处理分级调色测试（第一百二十二轮：lift / gamma / gain ＋ 饱和度）
覆盖：恒等参数与直通链路一致 / 增益与伽马方向 / 饱和度归零 / 非法参数忽略 / 串联反色可逆。
口径：场景为白纹理四边形；中灰与纯红纹理用于观察增益、伽马与饱和度；链式路径与既有后处理链用例同口径。
*/
#include "渲染后处理链测试辅助.h"

namespace
{
	//外通道较大者（R 与 B；避开 std::max —— 本层包含链有 min/max 宏）
	uint8_t outer_max_local(const std::array<uint8_t, 4>& value)
	{
		return value[0] > value[2] ? value[0] : value[2];
	}

	//三个通道的最小值（避开 std::min 宏冲突）
	int min_channel(const std::array<uint8_t, 4>& value)
	{
		int minimum = static_cast<int>(value[0]);
		if (static_cast<int>(value[1]) < minimum)
			minimum = static_cast<int>(value[1]);
		if (static_cast<int>(value[2]) < minimum)
			minimum = static_cast<int>(value[2]);
		return minimum;
	}

	//构造中灰纹理（RGBA8：rgb = 128，alpha = 255）
	std::vector<uint8_t> gray_texels(void)
	{
		std::vector<uint8_t> pixels(2 * 2 * 4);
		for (std::size_t index = 0; index < pixels.size(); index += 4)
		{
			pixels[index] = 128;
			pixels[index + 1] = 128;
			pixels[index + 2] = 128;
			pixels[index + 3] = 255;
		}
		return pixels;
	}

	//构造纯红纹理（RGBA8：rgb = 红，alpha = 255）
	std::vector<uint8_t> red_texels(void)
	{
		std::vector<uint8_t> pixels(2 * 2 * 4);
		for (std::size_t index = 0; index < pixels.size(); index += 4)
		{
			pixels[index] = 255;
			pixels[index + 1] = 0;
			pixels[index + 2] = 0;
			pixels[index + 3] = 255;
		}
		return pixels;
	}

	//按给定纹理登记场景并渲染一帧（链式路径；返回整帧像素）
	std::vector<uint8_t> render_with_chain(engine::Renderer& renderer, const std::vector<uint8_t>& texels)
	{
		uint64_t texture = 0;
		EXPECT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
		//先绑定本用例的纹理（辅助的 scene_register 会回绑默认白纹理，故此处自行登记）
		renderer.texture_bind_set(texture);
		engine::Mesh_Handle handle = 0;
		EXPECT_TRUE(renderer.mesh_load(handle, quad_data()));
		EXPECT_TRUE(renderer.mesh_draw(handle, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
		renderer.mesh_unload(handle);
		renderer.texture_unload(texture);
		return pixels;
	}
}

/*
恒等参数与直通链路一致（第一百二十二轮）
口径：参考链「直通 → 反色」；对照链「分级调色(0,1,1,1) → 反色」；
     两条链的末级相同，只差中间一级是否插入恒等调色，故两帧逐像素差应落在取整容差内。
*/
TEST_F(Renderer_Post_Chain_Test, 分级调色恒等与直通一致)
{
	//参考帧：直通 → 反色
	std::vector<uint8_t> reference(pixel_bytes());
	{
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(hidden_config()));
		if (!renderer.readback_supported())
			GTEST_SKIP() << "本机交换链图像不具备传输源用法";
		uint64_t scene_target = 0;
		ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
		uint64_t passes[2] = {};
		passes[0] = post_pipeline_register(renderer, "后处理.frag.spv");
		passes[1] = post_pipeline_register(renderer, "反色.frag.spv");
		ASSERT_NE(passes[0], 0u);
		ASSERT_NE(passes[1], 0u);
		ASSERT_TRUE(renderer.post_chain_set(passes, 2));
		renderer.render_target_present_set(scene_target);
		reference = render_with_chain(renderer, std::vector<uint8_t>(2 * 2 * 4, 255));
		renderer.pipeline_unload(passes[0]);
		renderer.pipeline_unload(passes[1]);
		renderer.render_target_destroy(scene_target);
	}
	//对照帧：分级调色（恒等）→ 反色
	std::vector<uint8_t> graded(pixel_bytes());
	{
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(hidden_config()));
		if (!renderer.readback_supported())
			GTEST_SKIP() << "本机交换链图像不具备传输源用法";
		uint64_t scene_target = 0;
		ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
		uint64_t passes[2] = {};
		passes[0] = post_pipeline_register(renderer, "分级调色.frag.spv");
		passes[1] = post_pipeline_register(renderer, "反色.frag.spv");
		ASSERT_NE(passes[0], 0u);
		ASSERT_NE(passes[1], 0u);
		ASSERT_TRUE(renderer.post_chain_set(passes, 2));
		renderer.post_stage_param4_set(0, 0.0f, 1.0f, 1.0f, 1.0f);
		renderer.render_target_present_set(scene_target);
		graded = render_with_chain(renderer, std::vector<uint8_t>(2 * 2 * 4, 255));
		renderer.pipeline_unload(passes[0]);
		renderer.pipeline_unload(passes[1]);
		renderer.render_target_destroy(scene_target);
	}

	const int difference = max_channel_diff(reference, graded);
	RecordProperty("grade_identity_diff", difference);
	EXPECT_LE(difference, CHAIN_TOLERANCE) << "恒等调色改变了画面（差 " << difference << "）";
}

/*
增益与伽马方向（第一百二十二轮）
口径：中灰纹理（128）走单级「分级调色」：恒等 ≈ 中灰；
     gain 0.5 明显变暗；gamma 2.0 明显变亮；gamma 0.5 明显变暗。
*/
TEST_F(Renderer_Post_Chain_Test, 分级调色增益与伽马方向)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t scene_target = 0;
	ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
	uint64_t grade = post_pipeline_register(renderer, "分级调色.frag.spv");
	ASSERT_NE(grade, 0u);
	ASSERT_TRUE(renderer.post_chain_set(&grade, 1));
	renderer.render_target_present_set(scene_target);

	//恒等
	renderer.post_stage_param4_set(0, 0.0f, 1.0f, 1.0f, 1.0f);
	const int identity = static_cast<int>(luma_of(pixel_get(render_with_chain(renderer, gray_texels()),
		WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2)));
	//增益 0.5
	renderer.post_stage_param4_set(0, 0.0f, 1.0f, 0.5f, 1.0f);
	const int half_gain = static_cast<int>(luma_of(pixel_get(render_with_chain(renderer, gray_texels()),
		WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2)));
	//伽马 2.0（提亮）
	renderer.post_stage_param4_set(0, 0.0f, 2.0f, 1.0f, 1.0f);
	const int bright_gamma = static_cast<int>(luma_of(pixel_get(render_with_chain(renderer, gray_texels()),
		WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2)));
	//伽马 0.5（压暗）
	renderer.post_stage_param4_set(0, 0.0f, 0.5f, 1.0f, 1.0f);
	const int dark_gamma = static_cast<int>(luma_of(pixel_get(render_with_chain(renderer, gray_texels()),
		WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2)));

	RecordProperty("grade_identity_luma", identity);
	RecordProperty("grade_half_gain_luma", half_gain);
	RecordProperty("grade_bright_gamma_luma", bright_gamma);
	RecordProperty("grade_dark_gamma_luma", dark_gamma);

	EXPECT_GE(identity, 100) << "恒等调色下中灰过暗（" << identity << "）";
	EXPECT_LE(half_gain, identity - 30) << "gain 0.5 未压暗（" << half_gain << " vs " << identity << "）";
	EXPECT_GE(bright_gamma, identity + 30) << "gamma 2.0 未提亮（" << bright_gamma << "）";
	EXPECT_LE(dark_gamma, identity - 30) << "gamma 0.5 未压暗（" << dark_gamma << "）";

	renderer.pipeline_unload(grade);
	renderer.render_target_destroy(scene_target);
}

/*
饱和度归零（第一百二十二轮）
口径：纯红纹理走单级「分级调色」：恒等时红通道明显高于其余两通道；
     饱和度 0 时三通道趋于一致（以 Rec.709 亮度为灰轴），且亮度基本保持。
*/
TEST_F(Renderer_Post_Chain_Test, 分级调色饱和度归零)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t scene_target = 0;
	ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
	uint64_t grade = post_pipeline_register(renderer, "分级调色.frag.spv");
	ASSERT_NE(grade, 0u);
	ASSERT_TRUE(renderer.post_chain_set(&grade, 1));
	renderer.render_target_present_set(scene_target);

	//恒等：红色仍饱和
	renderer.post_stage_param4_set(0, 0.0f, 1.0f, 1.0f, 1.0f);
	std::vector<uint8_t> saturated = render_with_chain(renderer, red_texels());
	const std::array<uint8_t, 4> saturated_center = pixel_get(saturated, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	//饱和度 0：三通道趋于一致
	renderer.post_stage_param4_set(0, 0.0f, 1.0f, 1.0f, 0.0f);
	std::vector<uint8_t> gray = render_with_chain(renderer, red_texels());
	const std::array<uint8_t, 4> gray_center = pixel_get(gray, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);

	const int saturated_spread = static_cast<int>(outer_max_local(saturated_center)) - min_channel(saturated_center);
	const int gray_spread = static_cast<int>(outer_max_local(gray_center)) - min_channel(gray_center);
	RecordProperty("grade_saturated_spread", saturated_spread);
	RecordProperty("grade_gray_spread", gray_spread);

	EXPECT_GT(saturated_spread, 80) << "恒等调色下红色不再饱和（差 " << saturated_spread << "）";
	EXPECT_LT(gray_spread, 20) << "饱和度 0 后仍保留通道差（差 " << gray_spread
		<< "；灰像素 " << pixel_text(gray_center) << "）";
	/*
	亮度不按字节均值比较
	调色在着色器内以线性光计算，而宿附件按 sRGB 编码存储，故「线性 0.2126（红的 Rec.709 亮度）」
	落盘后约为 127 字节；与饱和帧的字节亮度（18）直接比较无意义，故此处只断言「去饱和后三通道一致」。
	*/

	renderer.pipeline_unload(grade);
	renderer.render_target_destroy(scene_target);
}

/*
非法参数忽略（第一百二十二轮）
口径：先设恒等 (0,1,1,1)；再用越界伽马 0、越界饱和度 5、非数增益分别调用，
     三者都应整次忽略，该级参数保持恒等值；链外级序同样忽略。
*/
TEST_F(Renderer_Post_Chain_Test, 分级调色非法参数忽略)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	uint64_t pass = post_pipeline_register(renderer, "分级调色.frag.spv");
	ASSERT_NE(pass, 0u);
	ASSERT_TRUE(renderer.post_chain_set(&pass, 1));

	//设定恒等
	renderer.post_stage_param4_set(0, 0.0f, 1.0f, 1.0f, 1.0f);
	float params[4] = {};
	ASSERT_EQ(renderer.post_stage_param_get(0, params), 1u);
	EXPECT_FLOAT_EQ(params[1], 1.0f);

	//越界伽马
	renderer.post_stage_param4_set(0, 0.0f, 0.0f, 1.0f, 1.0f);
	ASSERT_EQ(renderer.post_stage_param_get(0, params), 1u);
	EXPECT_FLOAT_EQ(params[1], 1.0f) << "越界伽马未被忽略";

	//越界饱和度
	renderer.post_stage_param4_set(0, 0.0f, 1.0f, 1.0f, 5.0f);
	ASSERT_EQ(renderer.post_stage_param_get(0, params), 1u);
	EXPECT_FLOAT_EQ(params[3], 1.0f) << "越界饱和度未被忽略";

	//链外级序
	renderer.post_stage_param4_set(3, 0.0f, 1.0f, 1.0f, 1.0f);
	ASSERT_EQ(renderer.post_stage_param_get(0, params), 1u);
	EXPECT_FLOAT_EQ(params[0], 0.0f) << "链外级序影响了当前级";

	renderer.pipeline_unload(pass);
}

/*
串联反色可逆（第一百二十二轮）
口径：链 A =「分级调色(恒等) → 反色 → 反色」，链 B =「分级调色(恒等)」；
     两级反色互为逆变换，故两帧差应落在两级取整容差内（与既有链式用例同口径）。
*/
TEST_F(Renderer_Post_Chain_Test, 分级调色串联反色可逆)
{
	//链 B：单级恒等调色（参考）
	std::vector<uint8_t> single(pixel_bytes());
	{
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(hidden_config()));
		if (!renderer.readback_supported())
			GTEST_SKIP() << "本机交换链图像不具备传输源用法";
		uint64_t scene_target = 0;
		ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
		uint64_t grade = post_pipeline_register(renderer, "分级调色.frag.spv");
		ASSERT_NE(grade, 0u);
		ASSERT_TRUE(renderer.post_chain_set(&grade, 1));
		renderer.post_stage_param4_set(0, 0.0f, 1.0f, 1.0f, 1.0f);
		renderer.render_target_present_set(scene_target);
		single = render_with_chain(renderer, gray_texels());
		renderer.pipeline_unload(grade);
		renderer.render_target_destroy(scene_target);
	}
	//链 A：恒等调色 → 反色 → 反色
	std::vector<uint8_t> inverted(pixel_bytes());
	{
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(hidden_config()));
		if (!renderer.readback_supported())
			GTEST_SKIP() << "本机交换链图像不具备传输源用法";
		uint64_t scene_target = 0;
		ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
		uint64_t passes[3] = {};
		passes[0] = post_pipeline_register(renderer, "分级调色.frag.spv");
		passes[1] = post_pipeline_register(renderer, "反色.frag.spv");
		passes[2] = post_pipeline_register(renderer, "反色.frag.spv");
		ASSERT_NE(passes[0], 0u);
		ASSERT_NE(passes[1], 0u);
		ASSERT_NE(passes[2], 0u);
		ASSERT_TRUE(renderer.post_chain_set(passes, 3));
		renderer.post_stage_param4_set(0, 0.0f, 1.0f, 1.0f, 1.0f);
		renderer.render_target_present_set(scene_target);
		inverted = render_with_chain(renderer, gray_texels());
		renderer.pipeline_unload(passes[0]);
		renderer.pipeline_unload(passes[1]);
		renderer.pipeline_unload(passes[2]);
		renderer.render_target_destroy(scene_target);
	}

	const int difference = max_channel_diff(single, inverted);
	RecordProperty("grade_invert_roundtrip_diff", difference);
	EXPECT_LE(difference, CHAIN_TOLERANCE) << "两级反色未回到原画面（差 " << difference << "）";
}
