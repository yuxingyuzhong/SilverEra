/*
渲染阴影测试（方向光阴影 / 软度 / 强度 / 分辨率 / 按需重绘 / 阴影图读回）
（第五十七轮由「渲染场景数据测试.cpp」按功能域拆分；逻辑未改）
*/
#include "渲染场景测试辅助.h"

TEST_F(Renderer_Scene_Test, 方向光阴影生效)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle receiver = 0;
	engine::Mesh_Handle occluder = 0;
	ASSERT_TRUE(renderer.mesh_load(receiver, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
	ASSERT_TRUE(renderer.mesh_load(occluder, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
	renderer.texture_bind_set(texture);

	//渲染一帧并返回整帧像素（lambda 内用 EXPECT：ASSERT 不允许出现在有返回值的函数中）
	auto render_frame = [&]()
	{
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return pixels;
	};

	//场景：方向光沿 +Z（光源在 -Z 侧），强度 1、环境光 0
	engine::Camera_Data camera{};
	engine::Light_Data light{};
	light.direction[2] = 1.0f;
	light.intensity = 1.0f;
	light.ambient = 0.0f;
	engine::Fog_Data fog{};
	renderer.scene_apply(camera, light, fog);

	//无遮挡基线（只画接收面）
	ASSERT_TRUE(renderer.mesh_draw(receiver, nullptr));
	std::vector<uint8_t> baseline_pixels = render_frame();
	const uint32_t baseline = center_brightness(baseline_pixels);
	//阴影外参考点（中心右侧 150 像素：仍在接收面内、但位于遮挡面投影之外）
	const std::array<uint8_t, 4> baseline_edge =
		pixel_get(baseline_pixels, WINDOW_WIDTH / 2 + 150, WINDOW_HEIGHT / 2);
	const uint32_t baseline_edge_brightness =
		(static_cast<uint32_t>(baseline_edge[0]) + baseline_edge[1] + baseline_edge[2]) / 3;

	//遮挡面：缩放 0.6、平移到 z = -0.5（列主序模型矩阵）
	float occluder_transform[16] = {};
	occluder_transform[0] = 0.6f;
	occluder_transform[5] = 0.6f;
	occluder_transform[10] = 0.6f;
	occluder_transform[14] = -0.5f;
	occluder_transform[15] = 1.0f;

	//关闭阴影：遮挡面在接收面之后，画面应与基线一致
	ASSERT_TRUE(renderer.mesh_draw(occluder, occluder_transform));
	ASSERT_TRUE(renderer.mesh_draw(receiver, nullptr));
	std::vector<uint8_t> unshadowed_pixels = render_frame();
	const uint32_t unshadowed = center_brightness(unshadowed_pixels);

	//开启阴影
	renderer.shadow_enable_set(true);
	ASSERT_TRUE(renderer.shadow_enabled());
	ASSERT_TRUE(renderer.mesh_draw(occluder, occluder_transform));
	ASSERT_TRUE(renderer.mesh_draw(receiver, nullptr));
	std::vector<uint8_t> shadowed_pixels = render_frame();
	const uint32_t shadowed = center_brightness(shadowed_pixels);
	const std::array<uint8_t, 4> shadowed_edge =
		pixel_get(shadowed_pixels, WINDOW_WIDTH / 2 + 150, WINDOW_HEIGHT / 2);
	const uint32_t shadowed_edge_brightness =
		(static_cast<uint32_t>(shadowed_edge[0]) + shadowed_edge[1] + shadowed_edge[2]) / 3;

	//关闭阴影后应恢复基线
	renderer.shadow_enable_set(false);
	ASSERT_FALSE(renderer.shadow_enabled());
	ASSERT_TRUE(renderer.mesh_draw(occluder, occluder_transform));
	ASSERT_TRUE(renderer.mesh_draw(receiver, nullptr));
	const uint32_t restored = center_brightness(render_frame());

	RecordProperty("shadow_baseline", baseline);
	RecordProperty("shadow_unshadowed", unshadowed);
	RecordProperty("shadow_shadowed", shadowed);
	RecordProperty("shadow_edge_baseline", baseline_edge_brightness);
	RecordProperty("shadow_edge_shadowed", shadowed_edge_brightness);
	RecordProperty("shadow_restored", restored);

	EXPECT_GE(baseline, 200u) << "接收面未被方向光照亮（" << baseline << "）";
	EXPECT_NEAR(unshadowed, baseline, 3u) << "关闭阴影时遮挡面不应影响画面（"
		<< unshadowed << " vs " << baseline << "）";
	EXPECT_LT(shadowed, 60u) << "阴影未遮蔽中心（" << shadowed << "）";
	//阴影外参考点与基线一致（该点落在接收面边缘外的背景 / 边缘过渡，只要求不被阴影额外压暗）
	EXPECT_NEAR(shadowed_edge_brightness, baseline_edge_brightness, 3u) << "阴影外区域被误遮蔽（"
		<< shadowed_edge_brightness << " vs " << baseline_edge_brightness << "）";
	EXPECT_NEAR(restored, baseline, 3u) << "关闭阴影后画面未恢复（"
		<< restored << " vs " << baseline << "）";

	renderer.mesh_unload(receiver);
	renderer.mesh_unload(occluder);
	renderer.texture_unload(texture);
}

/*
阴影软度与强度（第五十三轮）
口径：同「方向光阴影生效」用法线 -Z 的接收面与方向光 +Z、环境光 0、遮挡面 z = -0.5；
     强度 0 时阴影完全无效（中心回到基线）、强度 1 时中心完全遮蔽、强度 0.5 时中心介于两者之间；
     软度 1（3×3 PCF）时中心仍被遮蔽；非法强度 1.5 整次忽略且画面不变。
*/
TEST_F(Renderer_Scene_Test, 阴影强度生效)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle receiver = 0;
	engine::Mesh_Handle occluder = 0;
	ASSERT_TRUE(renderer.mesh_load(receiver, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
	ASSERT_TRUE(renderer.mesh_load(occluder, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
	renderer.texture_bind_set(texture);

	//渲染一帧并返回整帧像素
	auto render_frame = [&]()
	{
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return pixels;
	};

	//场景：方向光沿 +Z、强度 1、环境光 0
	engine::Camera_Data camera{};
	engine::Light_Data light{};
	light.direction[2] = 1.0f;
	light.intensity = 1.0f;
	light.ambient = 0.0f;
	engine::Fog_Data fog{};
	renderer.scene_apply(camera, light, fog);

	//遮挡面（缩放 0.6、平移 z = -0.5，列主序）
	float occluder_transform[16] = {};
	occluder_transform[0] = 0.6f;
	occluder_transform[5] = 0.6f;
	occluder_transform[10] = 0.6f;
	occluder_transform[14] = -0.5f;
	occluder_transform[15] = 1.0f;

	//无遮挡且未开阴影的基线
	ASSERT_TRUE(renderer.mesh_draw(receiver, nullptr));
	const uint32_t baseline = center_brightness(render_frame());

	//开启阴影、强度 1：中心完全遮蔽
	renderer.shadow_enable_set(true);
	renderer.shadow_strength_set(1.0f);
	ASSERT_TRUE(renderer.mesh_draw(occluder, occluder_transform));
	ASSERT_TRUE(renderer.mesh_draw(receiver, nullptr));
	const uint32_t full_shadow = center_brightness(render_frame());

	//强度 0：阴影完全无效，中心回到基线
	renderer.shadow_strength_set(0.0f);
	ASSERT_TRUE(renderer.mesh_draw(occluder, occluder_transform));
	ASSERT_TRUE(renderer.mesh_draw(receiver, nullptr));
	const uint32_t no_shadow = center_brightness(render_frame());

	//强度 0.5：中心介于两者之间
	renderer.shadow_strength_set(0.5f);
	ASSERT_TRUE(renderer.mesh_draw(occluder, occluder_transform));
	ASSERT_TRUE(renderer.mesh_draw(receiver, nullptr));
	const uint32_t half_shadow = center_brightness(render_frame());

	//非法强度 1.5：整次忽略，画面与 0.5 一致
	renderer.shadow_strength_set(1.5f);
	EXPECT_FLOAT_EQ(renderer.shadow_strength(), 0.5f);
	ASSERT_TRUE(renderer.mesh_draw(occluder, occluder_transform));
	ASSERT_TRUE(renderer.mesh_draw(receiver, nullptr));
	const uint32_t rejected = center_brightness(render_frame());

	//软度 1（3×3 PCF）、强度恢复 1：中心仍处于遮蔽区，亮度保持低位
	renderer.shadow_strength_set(1.0f);
	renderer.shadow_softness_set(1.0f);
	ASSERT_TRUE(renderer.mesh_draw(occluder, occluder_transform));
	ASSERT_TRUE(renderer.mesh_draw(receiver, nullptr));
	const uint32_t soft_shadow = center_brightness(render_frame());

	RecordProperty("strength_baseline", baseline);
	RecordProperty("strength_full", full_shadow);
	RecordProperty("strength_zero", no_shadow);
	RecordProperty("strength_half", half_shadow);
	RecordProperty("strength_rejected", rejected);
	RecordProperty("softness_full", soft_shadow);

	EXPECT_GE(baseline, 200u) << "接收面未被方向光照亮（" << baseline << "）";
	EXPECT_LT(full_shadow, 60u) << "强度 1 时中心未完全遮蔽（" << full_shadow << "）";
	EXPECT_NEAR(no_shadow, baseline, 4u) << "强度 0 时阴影未失效（"
		<< no_shadow << " vs " << baseline << "）";
	EXPECT_GT(half_shadow, 100u) << "强度 0.5 时中心过暗（" << half_shadow << "）";
	EXPECT_LT(half_shadow, baseline - 50u) << "强度 0.5 时中心未变暗（"
		<< half_shadow << " vs " << baseline << "）";
	EXPECT_NEAR(rejected, half_shadow, 4u) << "非法强度未整次忽略（"
		<< rejected << " vs " << half_shadow << "）";
	EXPECT_LT(soft_shadow, 60u) << "软度 1 时中心未被遮蔽（" << soft_shadow << "）";

	renderer.mesh_unload(receiver);
	renderer.mesh_unload(occluder);
	renderer.texture_unload(texture);
}

/*
阴影分辨率可配（第五十四轮）
口径：同「方向光阴影生效」场景；切换 512 后阴影仍遮蔽中心、关闭后恢复基线；
     非法 300 整次忽略（分辨率保持 512）。
*/
TEST_F(Renderer_Scene_Test, 阴影分辨率可配)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle receiver = 0;
	engine::Mesh_Handle occluder = 0;
	ASSERT_TRUE(renderer.mesh_load(receiver, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
	ASSERT_TRUE(renderer.mesh_load(occluder, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
	renderer.texture_bind_set(texture);

	//渲染一帧并返回整帧像素
	auto render_frame = [&]()
	{
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return pixels;
	};

	//场景：方向光沿 +Z、强度 1、环境光 0、阴影开启
	engine::Camera_Data camera{};
	engine::Light_Data light{};
	light.direction[2] = 1.0f;
	light.intensity = 1.0f;
	light.ambient = 0.0f;
	engine::Fog_Data fog{};
	renderer.scene_apply(camera, light, fog);
	renderer.shadow_enable_set(true);

	//基线（无遮挡）
	ASSERT_TRUE(renderer.mesh_draw(receiver, nullptr));
	const uint32_t baseline = center_brightness(render_frame());

	//切到 512 后阴影仍应生效
	renderer.shadow_resolution_set(512);
	ASSERT_EQ(renderer.shadow_resolution(), 512u);
	float occluder_transform[16] = {};
	occluder_transform[0] = 0.6f;
	occluder_transform[5] = 0.6f;
	occluder_transform[10] = 0.6f;
	occluder_transform[14] = -0.5f;
	occluder_transform[15] = 1.0f;
	ASSERT_TRUE(renderer.mesh_draw(occluder, occluder_transform));
	ASSERT_TRUE(renderer.mesh_draw(receiver, nullptr));
	const uint32_t shadowed = center_brightness(render_frame());

	//非法分辨率整次忽略
	renderer.shadow_resolution_set(300);
	EXPECT_EQ(renderer.shadow_resolution(), 512u);

	//关闭阴影恢复基线
	renderer.shadow_enable_set(false);
	ASSERT_TRUE(renderer.mesh_draw(occluder, occluder_transform));
	ASSERT_TRUE(renderer.mesh_draw(receiver, nullptr));
	const uint32_t restored = center_brightness(render_frame());

	RecordProperty("resolution_baseline", baseline);
	RecordProperty("resolution_shadowed_512", shadowed);
	RecordProperty("resolution_restored", restored);

	EXPECT_GE(baseline, 200u) << "接收面未被方向光照亮（" << baseline << "）";
	EXPECT_LT(shadowed, 60u) << "512 分辨率下阴影未遮蔽中心（" << shadowed << "）";
	EXPECT_NEAR(restored, baseline, 4u) << "关闭阴影后未恢复基线（"
		<< restored << " vs " << baseline << "）";

	renderer.mesh_unload(receiver);
	renderer.mesh_unload(occluder);
	renderer.texture_unload(texture);
}

/*
阴影 pass 按需重绘（第五十五轮）
口径：同一静态场景连续两帧——第二帧输入签名不变，阴影 pass 不应再执行（计数不增）且画面一致；
     平移遮挡面后签名变化，第三帧必须重绘（计数 +1）且阴影离开中心（中心明显变亮）。
*/
TEST_F(Renderer_Scene_Test, 阴影按需重绘)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle receiver = 0;
	engine::Mesh_Handle occluder = 0;
	ASSERT_TRUE(renderer.mesh_load(receiver, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
	ASSERT_TRUE(renderer.mesh_load(occluder, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
	renderer.texture_bind_set(texture);

	//渲染一帧并返回整帧像素
	auto render_frame = [&]()
	{
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return pixels;
	};

	//场景：方向光沿 +Z、强度 1、环境光 0、阴影开启
	engine::Camera_Data camera{};
	engine::Light_Data light{};
	light.direction[2] = 1.0f;
	light.intensity = 1.0f;
	light.ambient = 0.0f;
	engine::Fog_Data fog{};
	renderer.scene_apply(camera, light, fog);
	renderer.shadow_enable_set(true);

	//遮挡面（缩放 0.6、居中、z = -0.5）
	float occluder_transform[16] = {};
	occluder_transform[0] = 0.6f;
	occluder_transform[5] = 0.6f;
	occluder_transform[10] = 0.6f;
	occluder_transform[14] = -0.5f;
	occluder_transform[15] = 1.0f;

	//第一帧：阴影 pass 必须执行
	ASSERT_TRUE(renderer.mesh_draw(occluder, occluder_transform));
	ASSERT_TRUE(renderer.mesh_draw(receiver, nullptr));
	const uint32_t first_brightness = center_brightness(render_frame());
	const uint64_t first_count = renderer.shadow_pass_count();

	//第二帧：输入完全一致，阴影 pass 应被跳过且画面一致
	ASSERT_TRUE(renderer.mesh_draw(occluder, occluder_transform));
	ASSERT_TRUE(renderer.mesh_draw(receiver, nullptr));
	const uint32_t second_brightness = center_brightness(render_frame());
	const uint64_t second_count = renderer.shadow_pass_count();

	//第三帧：遮挡面平移，签名变化 → 必须重绘且中心阴影移开
	float moved_transform[16] = {};
	for (std::size_t index = 0; index < 16; ++index)
		moved_transform[index] = occluder_transform[index];
	moved_transform[12] = 0.9f;
	ASSERT_TRUE(renderer.mesh_draw(occluder, moved_transform));
	ASSERT_TRUE(renderer.mesh_draw(receiver, nullptr));
	const uint32_t moved_brightness = center_brightness(render_frame());
	const uint64_t moved_count = renderer.shadow_pass_count();

	RecordProperty("reuse_first", first_count);
	RecordProperty("reuse_second", second_count);
	RecordProperty("reuse_moved", moved_count);
	RecordProperty("reuse_first_brightness", first_brightness);
	RecordProperty("reuse_second_brightness", second_brightness);
	RecordProperty("reuse_moved_brightness", moved_brightness);

	EXPECT_GE(first_count, 1u) << "首帧未执行阴影 pass";
	EXPECT_EQ(second_count, first_count) << "输入未变仍重绘了阴影图";
	EXPECT_LE(second_brightness > first_brightness ? second_brightness - first_brightness : first_brightness - second_brightness, 1u)
		<< "复用阴影图导致画面变化（" << first_brightness << " vs " << second_brightness << "）";
	EXPECT_EQ(moved_count, second_count + 1) << "输入变化后未重绘阴影图";
	EXPECT_GT(moved_brightness, first_brightness + 20u) << "遮挡面平移后阴影未移开（"
		<< moved_brightness << " vs " << first_brightness << "）";

	renderer.mesh_unload(receiver);
	renderer.mesh_unload(occluder);
	renderer.texture_unload(texture);
}

/*
阴影软度连续可调（第五十六轮）
口径：同「方向光阴影生效」场景；软度 0.3（3×3）、0.6（5×5）、1.0（7×7）三档下中心仍被遮蔽，
     关闭阴影后恢复基线（三档均不引入额外偏移或漏光）。
*/
TEST_F(Renderer_Scene_Test, 阴影软度连续可调)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle receiver = 0;
	engine::Mesh_Handle occluder = 0;
	ASSERT_TRUE(renderer.mesh_load(receiver, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
	ASSERT_TRUE(renderer.mesh_load(occluder, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
	renderer.texture_bind_set(texture);

	//渲染一帧并返回整帧像素
	auto render_frame = [&]()
	{
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return pixels;
	};

	//场景：方向光沿 +Z、强度 1、环境光 0、阴影开启
	engine::Camera_Data camera{};
	engine::Light_Data light{};
	light.direction[2] = 1.0f;
	light.intensity = 1.0f;
	light.ambient = 0.0f;
	engine::Fog_Data fog{};
	renderer.scene_apply(camera, light, fog);
	renderer.shadow_enable_set(true);

	//遮挡面（缩放 0.6、居中、z = -0.5）
	float occluder_transform[16] = {};
	occluder_transform[0] = 0.6f;
	occluder_transform[5] = 0.6f;
	occluder_transform[10] = 0.6f;
	occluder_transform[14] = -0.5f;
	occluder_transform[15] = 1.0f;

	//基线（无遮挡）
	ASSERT_TRUE(renderer.mesh_draw(receiver, nullptr));
	const uint32_t baseline = center_brightness(render_frame());

	//三档软度下中心均被遮蔽
	const float softness_list[3] = { 0.3f, 0.6f, 1.0f };
	uint32_t soft_brightness[3] = {};
	for (std::size_t index = 0; index < 3; ++index)
	{
		renderer.shadow_softness_set(softness_list[index]);
		ASSERT_TRUE(renderer.mesh_draw(occluder, occluder_transform));
		ASSERT_TRUE(renderer.mesh_draw(receiver, nullptr));
		soft_brightness[index] = center_brightness(render_frame());
	}

	//关闭阴影恢复基线
	renderer.shadow_enable_set(false);
	ASSERT_TRUE(renderer.mesh_draw(occluder, occluder_transform));
	ASSERT_TRUE(renderer.mesh_draw(receiver, nullptr));
	const uint32_t restored = center_brightness(render_frame());

	RecordProperty("soft_baseline", baseline);
	RecordProperty("soft_0_3", soft_brightness[0]);
	RecordProperty("soft_0_6", soft_brightness[1]);
	RecordProperty("soft_1_0", soft_brightness[2]);
	RecordProperty("soft_restored", restored);

	EXPECT_GE(baseline, 200u) << "接收面未被方向光照亮（" << baseline << "）";
	EXPECT_LT(soft_brightness[0], 60u) << "软度 0.3 时中心未被遮蔽（" << soft_brightness[0] << "）";
	EXPECT_LT(soft_brightness[1], 60u) << "软度 0.6 时中心未被遮蔽（" << soft_brightness[1] << "）";
	EXPECT_LT(soft_brightness[2], 60u) << "软度 1.0 时中心未被遮蔽（" << soft_brightness[2] << "）";
	EXPECT_NEAR(restored, baseline, 4u) << "关闭阴影后未恢复基线（"
		<< restored << " vs " << baseline << "）";

	renderer.mesh_unload(receiver);
	renderer.mesh_unload(occluder);
	renderer.texture_unload(texture);
}

/*
阴影图读回（第五十七轮）
口径：开阴影画遮挡面 ＋ 接收面后读回阴影图；尺寸等于当前分辨率；
     中心像素含几何深度（非全零），左上角为清屏值（全零）；切 512 后尺寸随之为 512；
     容量不足（小于 边长² × 4）返回假。
*/
TEST_F(Renderer_Scene_Test, 阴影图读回)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle receiver = 0;
	engine::Mesh_Handle occluder = 0;
	ASSERT_TRUE(renderer.mesh_load(receiver, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
	ASSERT_TRUE(renderer.mesh_load(occluder, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
	renderer.texture_bind_set(texture);

	//场景：方向光沿 +Z、强度 1、环境光 0、阴影开启
	engine::Camera_Data camera{};
	engine::Light_Data light{};
	light.direction[2] = 1.0f;
	light.intensity = 1.0f;
	light.ambient = 0.0f;
	engine::Fog_Data fog{};
	renderer.scene_apply(camera, light, fog);
	renderer.shadow_enable_set(true);

	//遮挡面（缩放 0.6、居中、z = -0.5）与接收面
	float occluder_transform[16] = {};
	occluder_transform[0] = 0.6f;
	occluder_transform[5] = 0.6f;
	occluder_transform[10] = 0.6f;
	occluder_transform[14] = -0.5f;
	occluder_transform[15] = 1.0f;
	ASSERT_TRUE(renderer.mesh_draw(occluder, occluder_transform));
	ASSERT_TRUE(renderer.mesh_draw(receiver, nullptr));
	ASSERT_TRUE(renderer.frame_render());

	//读回 1024 阴影图
	const uint32_t shadow_size = renderer.shadow_resolution();
	std::vector<uint8_t> shadow_pixels(static_cast<std::size_t>(shadow_size) * shadow_size * 4);
	uint32_t read_width = 0;
	uint32_t read_height = 0;
	ASSERT_TRUE(renderer.shadow_readback(shadow_pixels.data(),
		static_cast<uint32_t>(shadow_pixels.size()), &read_width, &read_height));
	EXPECT_EQ(read_width, shadow_size);
	EXPECT_EQ(read_height, shadow_size);

	//中心像素（含几何深度）与左上角（清屏值）
	const std::size_t center_index = (static_cast<std::size_t>(read_height / 2) * read_width + read_width / 2) * 4;
	const std::size_t corner_index = 0;
	const uint32_t center_sum = static_cast<uint32_t>(shadow_pixels[center_index])
		+ shadow_pixels[center_index + 1] + shadow_pixels[center_index + 2] + shadow_pixels[center_index + 3];
	const uint32_t corner_sum = static_cast<uint32_t>(shadow_pixels[corner_index])
		+ shadow_pixels[corner_index + 1] + shadow_pixels[corner_index + 2] + shadow_pixels[corner_index + 3];
	RecordProperty("shadow_readback_size", read_width);
	RecordProperty("shadow_readback_center", center_sum);
	RecordProperty("shadow_readback_corner", corner_sum);

	EXPECT_GT(center_sum, 0u) << "阴影图中心没有几何深度";
	EXPECT_EQ(corner_sum, 0u) << "阴影图角落不是清屏值";

	//容量不足被拒
	EXPECT_FALSE(renderer.shadow_readback(shadow_pixels.data(), 1024, nullptr, nullptr));

	//切 512 后尺寸随之变化
	renderer.shadow_resolution_set(512);
	ASSERT_TRUE(renderer.mesh_draw(occluder, occluder_transform));
	ASSERT_TRUE(renderer.mesh_draw(receiver, nullptr));
	ASSERT_TRUE(renderer.frame_render());
	std::vector<uint8_t> small_pixels(512 * 512 * 4);
	ASSERT_TRUE(renderer.shadow_readback(small_pixels.data(),
		static_cast<uint32_t>(small_pixels.size()), &read_width, &read_height));
	EXPECT_EQ(read_width, 512u);
	EXPECT_EQ(read_height, 512u);

	renderer.mesh_unload(receiver);
	renderer.mesh_unload(occluder);
	renderer.texture_unload(texture);
}

/*
方向光与补光逐光高光缩放（第五十轮）
口径：四边形法线朝 +Z、环境光 0、全局高光 1.0 / 16：
      方向光强度 0.4 时「缩放 0」仅漫反射、「缩放 1」叠加高光明显更亮；
      补光强度 0.4 时同理（主光关闭）。
*/

/*
斜法线不产生自阴影（第五十九轮）
口径：接收面绕 Y 轴倾斜 40°（法线不再平行光源方向），方向光沿 +Z、环境光 0、无遮挡面；
     阴影开启后中心亮度应与关闭时一致（法线偏移抵消掠射角自阴影），且保持受光（≥150）。
*/
TEST_F(Renderer_Scene_Test, 斜法线不产生自阴影)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle receiver = 0;
	ASSERT_TRUE(renderer.mesh_load(receiver, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
	renderer.texture_bind_set(texture);

	//场景：方向光沿 +Z（光源在 -Z 侧）、强度 1、环境光 0
	engine::Camera_Data camera{};
	engine::Light_Data light{};
	light.direction[2] = 1.0f;
	light.intensity = 1.0f;
	light.ambient = 0.0f;
	engine::Fog_Data fog{};
	renderer.scene_apply(camera, light, fog);

	//接收面绕 Y 轴倾斜 40°（法线经模型矩阵变换后与光线成约 50° 掠射）
	std::array<float, 16> model = rotation_y_matrix(0.6981f);

	//渲染一帧并返回中心亮度（lambda 内用 EXPECT：ASSERT 不允许出现在有返回值的函数中）
	auto render_brightness = [&]()
	{
		EXPECT_TRUE(renderer.mesh_draw(receiver, model.data()));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return center_brightness(pixels);
	};

	//关闭阴影（参考帧）
	renderer.shadow_enable_set(false);
	const uint32_t without_shadow = render_brightness();
	//开启阴影（无遮挡面：不得出现自阴影压暗）
	renderer.shadow_enable_set(true);
	const uint32_t with_shadow = render_brightness();
	RecordProperty("slanted_without_shadow", without_shadow);
	RecordProperty("slanted_with_shadow", with_shadow);

	EXPECT_GE(with_shadow, 150u) << "斜法线接收面在阴影开启后被自阴影压暗（" << with_shadow << "）";
	EXPECT_NEAR(with_shadow, without_shadow, 8u) << "法线偏移未消除自阴影（"
		<< with_shadow << " vs " << without_shadow << "）";

	renderer.shadow_enable_set(false);
	renderer.mesh_unload(receiver);
	renderer.texture_unload(texture);
}

/*
多线程下阴影输出与单线程一致（第二百零九轮）
口径：同一阴影场景分别在「0 线程（单线程）」与「4 线程」下渲染一帧，整帧像素逐字节一致；
      两绘制项的交换链直通路径不达并行阈值、阴影 pass 本身不并行，故两者 parallel_segments 均应为 0。
*/
TEST_F(Renderer_Scene_Test, 多线程下阴影输出与单线程一致)
{
	//以指定渲染线程数跑一帧固定阴影场景（返回整帧像素；失败返回空）
	auto render_shadow = [&](uint32_t thread_count, uint32_t& segments_out)
	{
		//空像素表示初始化 / 回读 / 渲染失败
		std::vector<uint8_t> pixels;
		engine::Renderer renderer;
		engine::Renderer_Config config = hidden_config();
		config.render_threads = thread_count;
		if (!renderer.init(config) || !renderer.readback_supported())
			return pixels;
		uint64_t texture = 0;
		std::vector<uint8_t> texels = solid_white();
		if (!renderer.texture_load(texture, texels.data(), 2, 2))
			return pixels;
		engine::Mesh_Handle receiver = 0;
		engine::Mesh_Handle occluder = 0;
		if (!renderer.mesh_load(receiver, quad_data_with_normal(0.0f, 0.0f, -1.0f))
			|| !renderer.mesh_load(occluder, quad_data_with_normal(0.0f, 0.0f, -1.0f)))
			return pixels;
		renderer.texture_bind_set(texture);

		//场景：方向光沿 +Z、强度 1、环境光 0，阴影开启
		engine::Camera_Data camera{};
		engine::Light_Data light{};
		light.direction[2] = 1.0f;
		light.intensity = 1.0f;
		light.ambient = 0.0f;
		engine::Fog_Data fog{};
		renderer.scene_apply(camera, light, fog);
		renderer.shadow_enable_set(true);

		//遮挡面：缩放 0.6、平移到 z = -0.5（列主序模型矩阵）
		float occluder_transform[16] = {};
		occluder_transform[0] = 0.6f;
		occluder_transform[5] = 0.6f;
		occluder_transform[10] = 0.6f;
		occluder_transform[14] = -0.5f;
		occluder_transform[15] = 1.0f;
		if (!renderer.mesh_draw(occluder, occluder_transform) || !renderer.mesh_draw(receiver, nullptr))
			return pixels;

		//渲染一帧并取并行段数
		pixels.resize(pixel_bytes());
		if (!renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())))
			pixels.clear();
		segments_out = renderer.frame_stats_get().parallel_segments;

		renderer.mesh_unload(occluder);
		renderer.mesh_unload(receiver);
		renderer.texture_unload(texture);
		return pixels;
	};

	//单线程基线
	uint32_t single_segments = 99;
	std::vector<uint8_t> single_pixels = render_shadow(0, single_segments);
	ASSERT_FALSE(single_pixels.empty()) << "单线程阴影帧渲染失败";
	//四线程（线程数只影响并行录制，不应影响阴影 pass 的画面）
	uint32_t multi_segments = 99;
	std::vector<uint8_t> multi_pixels = render_shadow(4, multi_segments);
	ASSERT_FALSE(multi_pixels.empty()) << "四线程阴影帧渲染失败";

	RecordProperty("shadow_single_segments", single_segments);
	RecordProperty("shadow_multi_segments", multi_segments);
	EXPECT_EQ(single_segments, 0u) << "单线程录制不应产生并行段";
	EXPECT_EQ(multi_segments, 0u) << "两绘制项未达并行阈值，且阴影 pass 不并行";
	ASSERT_EQ(single_pixels.size(), multi_pixels.size());
	//逐字节一致
	EXPECT_TRUE(single_pixels == multi_pixels) << "多线程下阴影输出与单线程不一致";
}
