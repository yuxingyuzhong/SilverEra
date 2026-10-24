/*
渲染聚光灯测试（聚光灯锥、聚光灯参数校验、距离衰减模式与逐光源衰减覆盖）
（第五十八轮由「渲染光照测试.cpp」按功能域进一步拆分；逻辑未改）
*/
#include "渲染场景测试辅助.h"

/*
聚光灯只照亮锥内（第三十四轮）
口径：白纹理 ＋ 法线 (0,0,1) 全屏四边形；光源位于 (0,0,1) 朝 -Z（正对四边形）：
      中心像素与光轴夹角 0°、右上角样本（41, 9）约 51°。窄锥（10°/20°）只保留中心，
      宽锥（60°/80°）连角部一起照亮；两次中心亮度应基本一致。
*/

TEST_F(Renderer_Scene_Test, 聚光灯只照亮锥内)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data_with_normal(0.0f, 0.0f, 1.0f)));
	renderer.texture_bind_set(texture);
	const float white_light[3] = { 1.0f, 1.0f, 1.0f };
	const float forward[3] = { 0.0f, 0.0f, -1.0f };
	//关闭方向光（本用例只观察聚光灯）
	renderer.light_set(forward, white_light, 0.0f, 0.0f);

	//指定像素的 RGB 均值（亮度粗量；通道序无关）
	auto brightness_at = [](const std::vector<uint8_t>& pixels, uint32_t x, uint32_t y)
	{
		std::array<uint8_t, 4> value = pixel_get(pixels, x, y);
		return (static_cast<uint32_t>(value[0]) + value[1] + value[2]) / 3;
	};
	//渲染一帧并返回「中心 / 右上角样本」亮度（lambda 内用 EXPECT：ASSERT 不允许出现在有返回值的函数中）
	auto render_pair = [&]()
	{
		EXPECT_TRUE(renderer.mesh_draw(mesh, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return std::array<uint32_t, 2>{ brightness_at(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2),
			brightness_at(pixels, 41, 9) };
	};

	const float position[3] = { 0.0f, 0.0f, 1.0f };
	//窄锥（10° / 20°）
	ASSERT_TRUE(renderer.light_add_spot(position, forward, white_light, 10.0f, 1.0f, 10.0f, 20.0f, 0.0f));
	std::array<uint32_t, 2> narrow = render_pair();
	//清空后改宽锥（同一盏灯、同一四边形；先恢复方向光关闭，再登记）
	renderer.lights_clear();
	renderer.light_set(forward, white_light, 0.0f, 0.0f);
	ASSERT_TRUE(renderer.light_add_spot(position, forward, white_light, 10.0f, 1.0f, 60.0f, 80.0f, 0.0f));
	std::array<uint32_t, 2> wide = render_pair();
	RecordProperty("narrow_center", narrow[0]);
	RecordProperty("narrow_corner", narrow[1]);
	RecordProperty("wide_center", wide[0]);
	RecordProperty("wide_corner", wide[1]);
	EXPECT_GE(narrow[0], 200u) << "窄锥中心未被照亮（亮度 " << narrow[0] << "）";
	//中心亮度差（锥形过渡不影响光轴方向）
	uint32_t center_gap = narrow[0] > wide[0] ? narrow[0] - wide[0] : wide[0] - narrow[0];
	EXPECT_LE(center_gap, 6u) << "锥角收窄改变了中心亮度（" << narrow[0] << " vs " << wide[0] << "）";
	EXPECT_LE(narrow[1], 40u) << "锥外区域未被压暗（亮度 " << narrow[1] << "）";
	EXPECT_GE(wide[1], 150u) << "宽锥未照亮角部（亮度 " << wide[1] << "）";

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

/*
聚光灯参数校验与数量上限（第三十四轮；第六十二轮上限 4 → 8）
口径：内角 ≥ 外角、外角越界（> 90°）、方向退化为零向量均被拒绝；
      合法登记 8 盏后第 9 盏被拒；清空后可重新登记。
*/
TEST_F(Renderer_Scene_Test, 聚光灯参数校验与上限)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	const float position[3] = { 0.0f, 0.0f, 1.0f };
	const float forward[3] = { 0.0f, 0.0f, -1.0f };
	const float zero_direction[3] = { 0.0f, 0.0f, 0.0f };
	const float white_light[3] = { 1.0f, 1.0f, 1.0f };
	//内角等于外角 → 拒绝
	EXPECT_FALSE(renderer.light_add_spot(position, forward, white_light, 10.0f, 1.0f, 30.0f, 30.0f, 0.0f));
	//内角大于外角 → 拒绝
	EXPECT_FALSE(renderer.light_add_spot(position, forward, white_light, 10.0f, 1.0f, 40.0f, 30.0f, 0.0f));
	//外角越界（> 90°）→ 拒绝
	EXPECT_FALSE(renderer.light_add_spot(position, forward, white_light, 10.0f, 1.0f, 20.0f, 120.0f, 0.0f));
	//方向退化（零向量）→ 拒绝
	EXPECT_FALSE(renderer.light_add_spot(position, zero_direction, white_light, 10.0f, 1.0f, 10.0f, 20.0f, 0.0f));
	//合法登记 8 盏（共用点光源上限）
	for (uint32_t index = 0; index < 8; ++index)
		EXPECT_TRUE(renderer.light_add_spot(position, forward, white_light, 10.0f, 1.0f, 10.0f, 20.0f, 0.0f));
	//第 9 盏被拒
	EXPECT_FALSE(renderer.light_add_spot(position, forward, white_light, 10.0f, 1.0f, 10.0f, 20.0f, 0.0f));
	//清空后可重新登记
	renderer.lights_clear();
	EXPECT_TRUE(renderer.light_add_spot(position, forward, white_light, 10.0f, 1.0f, 10.0f, 20.0f, 0.0f));
}

/*
场景描述携带聚光灯只照亮锥内（第三十五轮）
口径：白纹理 ＋ 法线 (0,0,1) 四边形，方向光关闭；同一 Scene_Data 的聚光灯（(0,0,1) 朝 -Z）：
      窄锥（10°/20°）中心亮、角部样本 (41,9) 暗；宽锥（60°/80°）角部被照亮，中心亮度不变。
*/
TEST_F(Renderer_Scene_Test, 场景描述携带聚光灯只照亮锥内)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data_with_normal(0.0f, 0.0f, 1.0f)));
	renderer.texture_bind_set(texture);

	//指定像素的 RGB 均值（亮度粗量；通道序无关）
	auto brightness_at = [](const std::vector<uint8_t>& pixels, uint32_t x, uint32_t y)
	{
		std::array<uint8_t, 4> value = pixel_get(pixels, x, y);
		return (static_cast<uint32_t>(value[0]) + value[1] + value[2]) / 3;
	};
	//渲染一帧并返回「中心 / 右上角样本」亮度（lambda 内用 EXPECT：ASSERT 不允许出现在有返回值的函数中）
	auto render_pair = [&]()
	{
		EXPECT_TRUE(renderer.mesh_draw(mesh, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return std::array<uint32_t, 2>{ brightness_at(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2),
			brightness_at(pixels, 41, 9) };
	};

	//场景：方向光关闭 ＋ 一盏正对四边形的聚光灯
	engine::Scene_Data scene{};
	scene.light_intensity = 0.0f;
	scene.spot_light_count = 1;
	scene.spot_light_position[0][2] = 1.0f;
	scene.spot_light_direction[0][0] = 0.0f;
	scene.spot_light_direction[0][1] = 0.0f;
	scene.spot_light_direction[0][2] = -1.0f;
	scene.spot_light_radius[0] = 10.0f;
	scene.spot_light_intensity[0] = 1.0f;
	scene.spot_light_inner_deg[0] = 10.0f;
	scene.spot_light_outer_deg[0] = 20.0f;
	renderer.scene_apply(scene);
	std::array<uint32_t, 2> narrow = render_pair();
	//同一场景改宽锥
	scene.spot_light_inner_deg[0] = 60.0f;
	scene.spot_light_outer_deg[0] = 80.0f;
	renderer.scene_apply(scene);
	std::array<uint32_t, 2> wide = render_pair();
	RecordProperty("scene_narrow_center", narrow[0]);
	RecordProperty("scene_narrow_corner", narrow[1]);
	RecordProperty("scene_wide_center", wide[0]);
	RecordProperty("scene_wide_corner", wide[1]);
	EXPECT_GE(narrow[0], 200u) << "场景聚光灯未照亮锥内（亮度 " << narrow[0] << "）";
	EXPECT_LE(narrow[1], 40u) << "锥外区域未被压暗（亮度 " << narrow[1] << "）";
	EXPECT_GE(wide[1], 150u) << "宽锥未照亮角部（亮度 " << wide[1] << "）";
	//中心亮度差（锥形过渡不影响光轴方向）
	uint32_t center_gap = narrow[0] > wide[0] ? narrow[0] - wide[0] : wide[0] - narrow[0];
	EXPECT_LE(center_gap, 6u) << "锥角变化改变了中心亮度（" << narrow[0] << " vs " << wide[0] << "）";

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

/*
场景描述中的非法聚光灯被跳过（第三十五轮）
口径：默认方向光（满照）；先应用默认场景回读，再把一盏「内角 ≥ 外角」的聚光灯写进 Scene_Data：
      该条被 `light_add_spot` 拒绝，画面应与默认场景逐字节一致（方向光仍生效）。
*/
TEST_F(Renderer_Scene_Test, 场景描述聚光灯非法参数被跳过)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data_with_normal(0.0f, 0.0f, 1.0f)));
	renderer.texture_bind_set(texture);

	//渲染一帧并返回像素（lambda 内用 EXPECT：ASSERT 不允许出现在有返回值的函数中）
	auto render_frame = [&]()
	{
		EXPECT_TRUE(renderer.mesh_draw(mesh, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return pixels;
	};

	//基线：默认场景（方向光满照）
	engine::Scene_Data baseline{};
	renderer.scene_apply(baseline);
	std::vector<uint8_t> plain = render_frame();
	//非法聚光灯（内角 40° ≥ 外角 30°）应被跳过
	engine::Scene_Data invalid = baseline;
	invalid.spot_light_count = 1;
	invalid.spot_light_position[0][2] = 1.0f;
	invalid.spot_light_inner_deg[0] = 40.0f;
	invalid.spot_light_outer_deg[0] = 30.0f;
	renderer.scene_apply(invalid);
	std::vector<uint8_t> after = render_frame();
	RecordProperty("invalid_spot_center", center_brightness(after));
	EXPECT_EQ(after, plain) << "非法聚光灯改变了画面";
	EXPECT_GE(center_brightness(after), 200u) << "方向光未继续生效";

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

/*
平方反比衰减模式可切换（第三十六轮）
口径：中灰纹理 ＋ 法线 (0,0,1) 四边形，方向光关闭，点光源 (0,0,2.9)、半径 3：
      线性模式衰减约 0.03（很暗）；切到平方反比（1/(1+(d/r)²) ≈ 0.52）明显变亮；切回线性恢复原亮度。
*/
TEST_F(Renderer_Scene_Test, 平方反比衰减模式可切换)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_gray();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data_with_normal(0.0f, 0.0f, 1.0f)));
	renderer.texture_bind_set(texture);
	//关闭方向光，只观察点光源
	const float forward[3] = { 0.0f, 0.0f, -1.0f };
	const float white_light[3] = { 1.0f, 1.0f, 1.0f };
	renderer.light_set(forward, white_light, 0.0f, 0.0f);
	const float position[3] = { 0.0f, 0.0f, 2.9f };
	ASSERT_TRUE(renderer.light_add_point(position, white_light, 3.0f, 1.0f, 0.0f));

	//渲染一帧并返回中心亮度（lambda 内用 EXPECT：ASSERT 不允许出现在有返回值的函数中）
	auto render_brightness = [&]()
	{
		EXPECT_TRUE(renderer.mesh_draw(mesh, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return center_brightness(pixels);
	};

	//默认线性模式
	EXPECT_EQ(renderer.light_attenuation_get(), 0u);
	uint32_t linear = render_brightness();
	//切到平方反比
	EXPECT_TRUE(renderer.light_attenuation_set(1));
	EXPECT_EQ(renderer.light_attenuation_get(), 1u);
	uint32_t inverse = render_brightness();
	//切回线性
	EXPECT_TRUE(renderer.light_attenuation_set(0));
	uint32_t restored = render_brightness();
	RecordProperty("atten_linear", linear);
	RecordProperty("atten_inverse", inverse);
	RecordProperty("atten_restored", restored);
	EXPECT_GE(inverse, linear + 40u) << "平方反比模式未显著改变衰减（" << linear << " → " << inverse << "）";
	//恢复后与首次线性亮度基本一致（通道取整允许 ±2）
	uint32_t gap = restored > linear ? restored - linear : linear - restored;
	EXPECT_LE(gap, 2u) << "切回线性后亮度未恢复（" << linear << " vs " << restored << "）";

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

/*
非法衰减模式被拒（第三十六轮）
口径：非法模式返回假且模式记录不变；画面与拒绝前逐字节一致。
*/
TEST_F(Renderer_Scene_Test, 非法衰减模式被拒)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_gray();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data_with_normal(0.0f, 0.0f, 1.0f)));
	renderer.texture_bind_set(texture);
	const float forward[3] = { 0.0f, 0.0f, -1.0f };
	const float white_light[3] = { 1.0f, 1.0f, 1.0f };
	renderer.light_set(forward, white_light, 0.0f, 0.0f);
	const float position[3] = { 0.0f, 0.0f, 1.0f };
	ASSERT_TRUE(renderer.light_add_point(position, white_light, 3.0f, 1.0f, 0.0f));

	//渲染一帧并返回像素（lambda 内用 EXPECT：ASSERT 不允许出现在有返回值的函数中）
	auto render_frame = [&]()
	{
		EXPECT_TRUE(renderer.mesh_draw(mesh, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return pixels;
	};

	std::vector<uint8_t> baseline = render_frame();
	//非法模式（2）返回假且模式保持 0
	EXPECT_FALSE(renderer.light_attenuation_set(2));
	EXPECT_EQ(renderer.light_attenuation_get(), 0u);
	EXPECT_EQ(render_frame(), baseline) << "非法衰减模式改变了画面";

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

/*
场景描述携带衰减模式（第三十七轮）
口径：中灰纹理 ＋ 法线 (0,0,1) 四边形，方向光关闭，点光源 (0,0,2.9)、半径 3：
      同一 Scene_Data 的 attenuation_mode 由 0（线性）改为 1（平方反比）后中心亮度显著提升。
*/
TEST_F(Renderer_Scene_Test, 场景描述携带衰减模式)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_gray();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data_with_normal(0.0f, 0.0f, 1.0f)));
	renderer.texture_bind_set(texture);

	//渲染一帧并返回中心亮度（lambda 内用 EXPECT：ASSERT 不允许出现在有返回值的函数中）
	auto render_brightness = [&]()
	{
		EXPECT_TRUE(renderer.mesh_draw(mesh, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return center_brightness(pixels);
	};

	//场景：方向光关闭 ＋ 一盏远距点光源（距离 2.9、半径 3）
	engine::Scene_Data scene{};
	scene.light_intensity = 0.0f;
	scene.light_count = 1;
	scene.point_light_position[0][2] = 2.9f;
	scene.point_light_radius[0] = 3.0f;
	scene.point_light_intensity[0] = 1.0f;
	scene.attenuation_mode = 0;
	renderer.scene_apply(scene);
	uint32_t linear = render_brightness();
	//同一场景切平方反比
	scene.attenuation_mode = 1;
	renderer.scene_apply(scene);
	uint32_t inverse = render_brightness();
	RecordProperty("scene_atten_linear", linear);
	RecordProperty("scene_atten_inverse", inverse);
	EXPECT_GE(inverse, linear + 40u) << "场景描述中的衰减模式未生效（" << linear << " → " << inverse << "）";

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

/*
场景描述中的非法衰减模式被忽略（第三十七轮）
口径：默认场景回读基线；把 attenuation_mode 写为 3 再应用：门面保持线性模式，画面逐字节一致。
*/
TEST_F(Renderer_Scene_Test, 场景描述非法衰减模式被忽略)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_gray();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data_with_normal(0.0f, 0.0f, 1.0f)));
	renderer.texture_bind_set(texture);

	//渲染一帧并返回像素（lambda 内用 EXPECT：ASSERT 不允许出现在有返回值的函数中）
	auto render_frame = [&]()
	{
		EXPECT_TRUE(renderer.mesh_draw(mesh, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return pixels;
	};

	engine::Scene_Data baseline{};
	renderer.scene_apply(baseline);
	std::vector<uint8_t> plain = render_frame();
	//非法模式（3）应被忽略，模式保持 0
	engine::Scene_Data invalid = baseline;
	invalid.attenuation_mode = 3;
	renderer.scene_apply(invalid);
	EXPECT_EQ(renderer.light_attenuation_get(), 0u);
	EXPECT_EQ(render_frame(), plain) << "场景描述中的非法衰减模式改变了画面";

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

/*
距离雾压暗远处物体（第三十八轮）
口径：白纹理 ＋ 法线 (0,0,1) 四边形、默认方向光满照；相机距四边形 3：
      雾色黑、密度 1、区间 [1, 4] → t = (3 − 1) / 3 ≈ 0.667、因子 t² ≈ 0.444，
      中心亮度由约 255 降至约 195；`fog_clear` 后恢复。
*/

TEST_F(Renderer_Scene_Test, 平方反比下聚光灯仍只照锥内)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data_with_normal(0.0f, 0.0f, 1.0f)));
	renderer.texture_bind_set(texture);
	const float white_light[3] = { 1.0f, 1.0f, 1.0f };
	const float forward[3] = { 0.0f, 0.0f, -1.0f };
	//关闭方向光并切平方反比衰减
	renderer.light_set(forward, white_light, 0.0f, 0.0f);
	ASSERT_TRUE(renderer.light_attenuation_set(1));
	const float position[3] = { 0.0f, 0.0f, 1.0f };
	ASSERT_TRUE(renderer.light_add_spot(position, forward, white_light, 10.0f, 1.0f, 10.0f, 20.0f, 0.0f));

	//渲染一帧并返回「中心 / 右上角样本」亮度（lambda 内用 EXPECT：ASSERT 不允许出现在有返回值的函数中）
	auto render_pair = [&]()
	{
		EXPECT_TRUE(renderer.mesh_draw(mesh, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		std::array<uint8_t, 4> center = pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
		std::array<uint8_t, 4> corner = pixel_get(pixels, 41, 9);
		return std::array<uint32_t, 2>{
			(static_cast<uint32_t>(center[0]) + center[1] + center[2]) / 3,
			(static_cast<uint32_t>(corner[0]) + corner[1] + corner[2]) / 3 };
	};

	std::array<uint32_t, 2> pair = render_pair();
	RecordProperty("spot_inverse_center", pair[0]);
	RecordProperty("spot_inverse_corner", pair[1]);
	EXPECT_GE(pair[0], 200u) << "平方反比下锥内未被照亮（亮度 " << pair[0] << "）";
	EXPECT_LE(pair[1], 40u) << "平方反比削弱了锥形（锥外亮度 " << pair[1] << "）";

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

/*
聚光灯锥内闪烁两种衰减可区分（第四十二轮）
口径：白纹理 ＋ 法线 (0,0,1)、方向光关闭；聚光灯 (0,0,2.9) 朝 -Z、半径 3、宽锥：
      线性衰减约 0.03（暗）、平方反比约 0.52（亮），中心亮度差 ≥ 40。
*/
TEST_F(Renderer_Scene_Test, 聚光灯锥内两种衰减可区分)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data_with_normal(0.0f, 0.0f, 1.0f)));
	renderer.texture_bind_set(texture);
	const float white_light[3] = { 1.0f, 1.0f, 1.0f };
	const float forward[3] = { 0.0f, 0.0f, -1.0f };
	renderer.light_set(forward, white_light, 0.0f, 0.0f);
	const float position[3] = { 0.0f, 0.0f, 2.9f };
	ASSERT_TRUE(renderer.light_add_spot(position, forward, white_light, 3.0f, 1.0f, 60.0f, 80.0f, 0.0f));

	//渲染一帧并返回中心亮度（lambda 内用 EXPECT：ASSERT 不允许出现在有返回值的函数中）
	auto render_brightness = [&]()
	{
		EXPECT_TRUE(renderer.mesh_draw(mesh, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return center_brightness(pixels);
	};

	//线性（默认）
	uint32_t linear = render_brightness();
	//平方反比
	ASSERT_TRUE(renderer.light_attenuation_set(1));
	uint32_t inverse = render_brightness();
	RecordProperty("spot_atten_linear", linear);
	RecordProperty("spot_atten_inverse", inverse);
	EXPECT_GE(inverse, linear + 40u) << "聚光灯未随衰减模式变亮（" << linear << " → " << inverse << "）";

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

/*
雾场景上的文字仍可见（第四十三轮）
口径：白纹理四边形 ＋ 黑雾（密度 1、区间 [1,4]）压暗场景后叠白色文字：
      文字走独立着色器（不读雾参数），左上 1/9 区域仍应有明显亮像素；字体缺失则跳过。
*/

TEST_F(Renderer_Scene_Test, 逐光源衰减模式覆盖全局)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, cube_data(0.5f)));
	renderer.texture_bind_set(texture);

	//渲染一帧并返回中心亮度（lambda 内用 EXPECT：ASSERT 不允许出现在有返回值的函数中）
	auto render_brightness = [&]()
	{
		EXPECT_TRUE(renderer.mesh_draw(mesh, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return center_brightness(pixels);
	};

	//关闭方向光，仅保留点光源
	const float white_light[3] = { 1.0f, 1.0f, 1.0f };
	const float light_direction[3] = { 0.0f, 0.0f, -1.0f };
	const float light_position[3] = { 0.0f, 0.0f, 2.5f };
	//(1) 全局线性 ＋ 逐盏平方反比
	renderer.lights_clear();
	renderer.light_set(light_direction, white_light, 0.0f, 0.0f);
	renderer.light_attenuation_set(0);
	ASSERT_TRUE(renderer.light_add_point(light_position, white_light, 2.0f, 1.0f, 0.0f, 2));
	uint32_t inverse = render_brightness();
	//(2) 全局平方反比 ＋ 逐盏线性
	renderer.lights_clear();
	renderer.light_set(light_direction, white_light, 0.0f, 0.0f);
	renderer.light_attenuation_set(1);
	ASSERT_TRUE(renderer.light_add_point(light_position, white_light, 2.0f, 1.0f, 0.0f, 1));
	uint32_t linear = render_brightness();
	//(3) 逐盏 0 = 跟随全局（全局平方反比）
	renderer.lights_clear();
	renderer.light_set(light_direction, white_light, 0.0f, 0.0f);
	renderer.light_attenuation_set(1);
	ASSERT_TRUE(renderer.light_add_point(light_position, white_light, 2.0f, 1.0f, 0.0f, 0));
	uint32_t follow = render_brightness();
	//(4) 越界值 9 按 2 处理（与 (1) 同口径）
	renderer.lights_clear();
	renderer.light_set(light_direction, white_light, 0.0f, 0.0f);
	renderer.light_attenuation_set(0);
	ASSERT_TRUE(renderer.light_add_point(light_position, white_light, 2.0f, 1.0f, 0.0f, 9));
	uint32_t clamped = render_brightness();
	RecordProperty("per_light_inverse", inverse);
	RecordProperty("per_light_linear", linear);
	RecordProperty("follow_global", follow);

	EXPECT_GT(inverse, linear + 40u) << "逐盏平方反比未覆盖全局线性（" << inverse << " vs " << linear << "）";
	EXPECT_LT(linear, 120u) << "逐盏线性未覆盖全局平方反比（中心仍亮：" << linear << "）";
	EXPECT_NEAR(follow, inverse, 4u) << "逐盏模式 0 未跟随全局（" << follow << " vs " << inverse << "）";
	EXPECT_NEAR(clamped, inverse, 4u) << "逐光源模式越界未被钳到 2（" << clamped << " vs " << inverse << "）";

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

/*
三组入参与扁平场景等价（第四十七轮）
口径：同一份「相机 ＋ 单点光源 ＋ 高光 ＋ 衰减模式 ＋ 雾 ＋ 环境光颜色」分别经
      扁平 Scene_Data 与 Camera_Data / Light_Data / Fog_Data 三组提交，两帧画面应逐字节一致。
*/