/*
渲染后处理查找表测试（第一百四十四轮）
覆盖：恒等 3D LUT 不改变画面、反色 LUT 反转颜色、混合强度半量、
      恒等表生成参数校验、绑定句柄可观测性与无效绑定忽略。
说明：LUT 以 2D 条带存储（宽 = 边长²、高 = 边长），片元按手工三线性采样。
*/
#include "渲染后处理链测试辅助.h"

namespace
{
	//构造反色 3D LUT 条带（每轴 edge 个纹素；线性空间输出 = 1 − 输入，按 sRGB 编码存储）
	std::vector<uint8_t> inverse_lut_pixels(uint32_t edge)
	{
		//条带尺寸（宽 = 边长²、高 = 边长）
		const uint32_t strip_width = edge * edge;
		std::vector<uint8_t> pixels(static_cast<std::size_t>(strip_width) * edge * 4);
		//每级对应的 sRGB 编码值（纹理格式为 SRGB，采样时解码回线性）
		std::vector<uint8_t> 编码表(edge);
		for (uint32_t 级 = 0; 级 < edge; ++级)
		{
			//反色后的线性值
			float 线性 = 1.0f - static_cast<float>(级) * (edge > 1 ? 1.0f / static_cast<float>(edge - 1) : 0.0f);
			//线性 → sRGB 编码
			float 编码 = 线性 <= 0.0031308f ? 线性 * 12.92f
				: 1.055f * std::pow(线性, 1.0f / 2.4f) - 0.055f;
			编码表[级] = static_cast<uint8_t>(编码 * 255.0f + 0.5f);
		}
		for (uint32_t 层 = 0; 层 < edge; ++层)
		{
			for (uint32_t 绿 = 0; 绿 < edge; ++绿)
			{
				for (uint32_t 红 = 0; 红 < edge; ++红)
				{
					//条带坐标（X = 层 × 边长 ＋ 红、Y = 绿）
					std::size_t 偏移 = (static_cast<std::size_t>(绿) * strip_width + 层 * edge + 红) * 4;
					pixels[偏移] = 编码表[红];
					pixels[偏移 + 1] = 编码表[绿];
					pixels[偏移 + 2] = 编码表[层];
					pixels[偏移 + 3] = 255;
				}
			}
		}
		return pixels;
	}
}

//恒等 3D LUT 不改变画面：单级查找链与直接走交换链的回读画面逐通道差不超过链容差
TEST_F(Renderer_Post_Chain_Test, 恒等查找表不改变画面)
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
	//对照帧：单级「恒等查找」链（边长 17）
	std::vector<uint8_t> chained(pixel_bytes());
	{
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(hidden_config()));
		if (!renderer.readback_supported())
			GTEST_SKIP() << "本机交换链图像不具备传输源用法";
		//恒等表（17³）与后处理管线
		uint64_t lut = 0;
		ASSERT_TRUE(renderer.texture_load_lut_identity(lut, 17));
		ASSERT_NE(lut, 0u);
		uint64_t stage = post_pipeline_register(renderer, "色彩查找.frag.spv");
		ASSERT_NE(stage, 0u);
		//绑定查找表并把该管线设为单级链
		renderer.post_lut_bind(lut);
		EXPECT_EQ(renderer.post_lut_get(), lut);
		ASSERT_TRUE(renderer.post_chain_set(&stage, 1));
		//场景目标与同帧呈现
		uint64_t scene_target = 0;
		ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
		engine::Mesh_Handle handle = 0;
		scene_register(renderer, handle);
		renderer.render_target_present_set(scene_target);
		ASSERT_TRUE(renderer.frame_render(chained.data(), static_cast<uint32_t>(chained.size())));
		//恒等表不改变颜色
		int difference = max_channel_diff(reference, chained);
		RecordProperty("lut_identity_max_diff", difference);
		EXPECT_LE(difference, CHAIN_TOLERANCE) << "恒等查找表改变了画面（最大逐通道差 " << difference << "）";
		renderer.mesh_unload(handle);
		renderer.pipeline_unload(stage);
		renderer.render_target_destroy(scene_target);
		renderer.texture_unload(lut);
	}
}

/*
反色 3D LUT 反转颜色（强度 1）
口径：白纹理四边形 ＋ 默认清屏色；单级查找链、逐级参数 x = 1（全量应用）：
      中心（白）应变为黑，角落（清屏暗色）应变亮。
*/
TEST_F(Renderer_Post_Chain_Test, 反色查找表反转颜色)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//边长 2 的反色表（2³ 足以保持端点精确）
	const std::vector<uint8_t> lut_pixels = inverse_lut_pixels(2);
	uint64_t lut = 0;
	ASSERT_TRUE(renderer.texture_load(lut, lut_pixels.data(), 4, 2));
	uint64_t stage = post_pipeline_register(renderer, "色彩查找.frag.spv");
	ASSERT_NE(stage, 0u);
	renderer.post_lut_bind(lut);
	ASSERT_TRUE(renderer.post_chain_set(&stage, 1));
	//逐级参数 x = 1（全量）
	renderer.post_stage_param_set(0, 1.0f, 0.0f, 1.0f);
	//场景目标与同帧呈现
	uint64_t scene_target = 0;
	ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
	engine::Mesh_Handle handle = 0;
	scene_register(renderer, handle);
	renderer.render_target_present_set(scene_target);
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	//中心（白 → 黑）
	std::array<uint8_t, 4> center = pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	EXPECT_LE(*std::max_element(center.begin(), center.begin() + 3), DARK_THRESHOLD)
		<< "中心未反色为黑：" << pixel_text(center);
	//角落（暗清屏 → 亮）
	std::array<uint8_t, 4> corner = pixel_get(pixels, 1, 1);
	EXPECT_GE(*std::min_element(corner.begin(), corner.begin() + 3), 200)
		<< "角落未反色为亮色：" << pixel_text(corner);
	//强度归零 → 关闭该级（中心回白；逐帧登记绘制项后重新出帧）
	renderer.post_stage_param_set(0, 0.0f, 0.0f, 1.0f);
	ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	std::array<uint8_t, 4> restored = pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	EXPECT_GE(*std::min_element(restored.begin(), restored.begin() + 3), SATURATION_THRESHOLD)
		<< "强度归零后中心未回白：" << pixel_text(restored);
	renderer.mesh_unload(handle);
	renderer.pipeline_unload(stage);
	renderer.render_target_destroy(scene_target);
	renderer.texture_unload(lut);
}

/*
反色查找表半量混合
口径：同上场景，逐级参数 x = 0.5：中心应由白变中灰（亮度约 128），
      既不接近黑也不接近白，从而证明强度参数生效。
*/
TEST_F(Renderer_Post_Chain_Test, 反色查找表半量混合)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	const std::vector<uint8_t> lut_pixels = inverse_lut_pixels(2);
	uint64_t lut = 0;
	ASSERT_TRUE(renderer.texture_load(lut, lut_pixels.data(), 4, 2));
	uint64_t stage = post_pipeline_register(renderer, "色彩查找.frag.spv");
	ASSERT_NE(stage, 0u);
	renderer.post_lut_bind(lut);
	ASSERT_TRUE(renderer.post_chain_set(&stage, 1));
	renderer.post_stage_param_set(0, 0.5f, 0.0f, 1.0f);
	uint64_t scene_target = 0;
	ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
	engine::Mesh_Handle handle = 0;
	scene_register(renderer, handle);
	renderer.render_target_present_set(scene_target);
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	std::array<uint8_t, 4> center = pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	uint32_t brightness = (static_cast<uint32_t>(center[0]) + center[1] + center[2]) / 3u;
	RecordProperty("lut_half_brightness", brightness);
	//半量混合：线性 0.5 → sRGB 读回约 188
	EXPECT_GE(brightness, 170u) << "半量混合过暗：" << pixel_text(center);
	EXPECT_LE(brightness, 205u) << "半量混合未生效：" << pixel_text(center);
	renderer.mesh_unload(handle);
	renderer.pipeline_unload(stage);
	renderer.render_target_destroy(scene_target);
	renderer.texture_unload(lut);
}

//恒等表生成参数校验：未初始化 / 边长 0 / 1 / 65 均返回假且句柄归零
TEST_F(Renderer_Post_Chain_Test, 恒等查找表参数校验)
{
	//未初始化
	engine::Renderer uninitialized;
	uint64_t handle = 9999;
	EXPECT_FALSE(uninitialized.texture_load_lut_identity(handle, 17));
	EXPECT_EQ(handle, 0u);
	//已初始化：非法边长
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	handle = 9999;
	EXPECT_FALSE(renderer.texture_load_lut_identity(handle, 0));
	EXPECT_EQ(handle, 0u);
	EXPECT_FALSE(renderer.texture_load_lut_identity(handle, 1));
	EXPECT_EQ(handle, 0u);
	EXPECT_FALSE(renderer.texture_load_lut_identity(handle, 65));
	EXPECT_EQ(handle, 0u);
	//合法边长：尺寸为 边长² × 边长
	uint64_t lut = 0;
	ASSERT_TRUE(renderer.texture_load_lut_identity(lut, 3));
	uint32_t width = 0;
	uint32_t height = 0;
	ASSERT_TRUE(renderer.texture_size(lut, &width, &height));
	EXPECT_EQ(width, 9u);
	EXPECT_EQ(height, 3u);
	renderer.texture_unload(lut);
}

//查找表绑定可观测：未绑定时为 0；无效句柄被忽略且保持原绑定
TEST_F(Renderer_Post_Chain_Test, 查找表绑定可观测)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//未绑定
	EXPECT_EQ(renderer.post_lut_get(), 0u);
	//合法绑定
	uint64_t lut = 0;
	ASSERT_TRUE(renderer.texture_load_lut_identity(lut, 5));
	renderer.post_lut_bind(lut);
	EXPECT_EQ(renderer.post_lut_get(), lut);
	//未登记句柄不改变原绑定
	renderer.post_lut_bind(12345);
	EXPECT_EQ(renderer.post_lut_get(), lut);
	//句柄 0 表示清除绑定（第一百五十七轮）
	renderer.post_lut_bind(0);
	EXPECT_EQ(renderer.post_lut_get(), 0u);
	//未初始化查询为 0
	engine::Renderer uninitialized;
	EXPECT_EQ(uninitialized.post_lut_get(), 0u);
	renderer.texture_unload(lut);
}

/*
运行时切换两张查找表（第一百五十七轮）
口径：同一渲染器、同一单级查找链：先绑恒等表取基线；改绑反色表（强度 1）中心应变黑；
      再绑回恒等表后，画面与基线逐字节一致（切换不残留上一张表）。
*/
TEST_F(Renderer_Post_Chain_Test, 查找表运行时切换)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//恒等表（边长 5）与反色表（边长 2）
	uint64_t identity_lut = 0;
	ASSERT_TRUE(renderer.texture_load_lut_identity(identity_lut, 5));
	const std::vector<uint8_t> inverse_pixels = inverse_lut_pixels(2);
	uint64_t inverse_lut = 0;
	ASSERT_TRUE(renderer.texture_load(inverse_lut, inverse_pixels.data(), 4, 2));
	uint64_t stage = post_pipeline_register(renderer, "色彩查找.frag.spv");
	ASSERT_NE(stage, 0u);
	ASSERT_TRUE(renderer.post_chain_set(&stage, 1));
	renderer.post_stage_param_set(0, 1.0f, 0.0f, 1.0f);
	uint64_t scene_target = 0;
	ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
	engine::Mesh_Handle handle = 0;
	scene_register(renderer, handle);
	renderer.render_target_present_set(scene_target);

	//渲染一帧并返回像素（lambda 内用 EXPECT：ASSERT 不允许出现在有返回值的函数中）
	auto render_frame = [&]()
	{
		EXPECT_TRUE(renderer.mesh_draw(handle, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
		return pixels;
	};

	//基线：恒等表
	renderer.post_lut_bind(identity_lut);
	std::vector<uint8_t> baseline = render_frame();
	std::array<uint8_t, 4> baseline_center = pixel_get(baseline, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	EXPECT_GE(*std::min_element(baseline_center.begin(), baseline_center.begin() + 3), SATURATION_THRESHOLD)
		<< "恒等表下中心未保持亮白";

	//切换到反色表：中心变黑
	renderer.post_lut_bind(inverse_lut);
	EXPECT_EQ(renderer.post_lut_get(), inverse_lut);
	std::vector<uint8_t> inverted = render_frame();
	std::array<uint8_t, 4> inverted_center = pixel_get(inverted, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	EXPECT_LE(*std::max_element(inverted_center.begin(), inverted_center.begin() + 3), DARK_THRESHOLD)
		<< "切换反色表后中心未变黑：" << pixel_text(inverted_center);

	//切回恒等表：逐字节回到基线
	renderer.post_lut_bind(identity_lut);
	EXPECT_EQ(renderer.post_lut_get(), identity_lut);
	std::vector<uint8_t> restored = render_frame();
	EXPECT_EQ(max_channel_diff(restored, baseline), 0) << "切回恒等表后画面未还原";

	renderer.mesh_unload(handle);
	renderer.pipeline_unload(stage);
	renderer.render_target_destroy(scene_target);
	renderer.texture_unload(identity_lut);
	renderer.texture_unload(inverse_lut);
}

/*
清除查找表绑定（第一百五十七轮）
口径：先绑反色表（强度 1）确认画面被反转；再以 post_lut_bind(0) 清除：
      句柄归零，同一查找级退化为原样输出（与直通链逐通道差 ≤ 2）。
*/
TEST_F(Renderer_Post_Chain_Test, 清除查找表绑定)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	const std::vector<uint8_t> inverse_pixels = inverse_lut_pixels(2);
	uint64_t lut = 0;
	ASSERT_TRUE(renderer.texture_load(lut, inverse_pixels.data(), 4, 2));
	uint64_t stage = post_pipeline_register(renderer, "色彩查找.frag.spv");
	ASSERT_NE(stage, 0u);
	ASSERT_TRUE(renderer.post_chain_set(&stage, 1));
	renderer.post_stage_param_set(0, 1.0f, 0.0f, 1.0f);
	uint64_t scene_target = 0;
	ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
	engine::Mesh_Handle handle = 0;
	scene_register(renderer, handle);
	renderer.render_target_present_set(scene_target);

	//渲染一帧并返回像素（lambda 内用 EXPECT：ASSERT 不允许出现在有返回值的函数中）
	auto render_frame = [&]()
	{
		EXPECT_TRUE(renderer.mesh_draw(handle, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
		return pixels;
	};

	//绑定反色表：中心变黑
	renderer.post_lut_bind(lut);
	std::vector<uint8_t> inverted = render_frame();
	std::array<uint8_t, 4> inverted_center = pixel_get(inverted, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	EXPECT_LE(*std::max_element(inverted_center.begin(), inverted_center.begin() + 3), DARK_THRESHOLD);

	//清除绑定：句柄归零，查找级退化为原样输出
	renderer.post_lut_bind(0);
	EXPECT_EQ(renderer.post_lut_get(), 0u) << "清除后句柄未归零";
	std::vector<uint8_t> cleared = render_frame();
	std::array<uint8_t, 4> cleared_center = pixel_get(cleared, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	EXPECT_GE(*std::min_element(cleared_center.begin(), cleared_center.begin() + 3), SATURATION_THRESHOLD)
		<< "清除绑定后中心未恢复亮白：" << pixel_text(cleared_center);
	//与清屏色（角落）的差异应与直通一致（间接验证「原样输出」）
	std::array<uint8_t, 4> corner = pixel_get(cleared, 1, 1);
	EXPECT_GT(cleared_center[2], static_cast<uint8_t>(corner[2] + 100)) << "清除后画面不再区分中心与背景";

	renderer.mesh_unload(handle);
	renderer.pipeline_unload(stage);
	renderer.render_target_destroy(scene_target);
	renderer.texture_unload(lut);
}
