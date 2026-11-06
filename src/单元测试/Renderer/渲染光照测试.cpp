/*
渲染光照测试（方向光 / 点光源 / 环境光 / 镜面高光与光照参数校验）
（第五十八轮由「渲染场景数据测试.cpp」拆出后再次细分：聚光灯与衰减见「渲染聚光灯测试.cpp」）
*/
#include "渲染场景测试辅助.h"


TEST_F(Renderer_Scene_Test, 朝向光源更亮)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	renderer.texture_bind_set(texture);
	//正面朝相机（受光）与背面（背光）两种网格
	engine::Mesh_Handle lit_mesh = 0;
	engine::Mesh_Handle dark_mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(lit_mesh, quad_data_with_normal(0.0f, 0.0f, 1.0f)));
	ASSERT_TRUE(renderer.mesh_load(dark_mesh, quad_data_with_normal(0.0f, 0.0f, -1.0f)));

	//渲染一帧并返回中心亮度（lambda 内用 EXPECT：ASSERT 不允许出现在有返回值的函数中）
	auto render_brightness = [&](engine::Mesh_Handle mesh)
	{
		EXPECT_TRUE(renderer.mesh_draw(mesh, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return center_brightness(pixels);
	};

	//受光帧：默认光照（方向 -Z、白、强度 1、环境光 0）→ 照度 1
	uint32_t lit = render_brightness(lit_mesh);
	//背光帧：同一光照，法线与光传播方向同向 → 照度 0
	uint32_t dark = render_brightness(dark_mesh);
	RecordProperty("light_lit", lit);
	RecordProperty("light_dark", dark);
	EXPECT_GE(lit, 200u) << "受光面未达到满照（中心亮度 " << lit << "）";
	EXPECT_LE(dark, 40u) << "背光面未被压暗（中心亮度 " << dark << "）";
	EXPECT_GT(static_cast<int>(lit), static_cast<int>(dark) + 100) << "光照未区分正反面";

	renderer.mesh_unload(lit_mesh);
	renderer.mesh_unload(dark_mesh);
	renderer.texture_unload(texture);
}

/*
未提供法线保持满照（第二十九轮回归口径）：只写位置 / 纹理坐标的旧网格在默认光照下画面不变
*/
TEST_F(Renderer_Scene_Test, 无法线网格保持满照)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data()));
	renderer.texture_bind_set(texture);
	ASSERT_TRUE(renderer.mesh_draw(mesh, full_screen_matrix().data()));
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
	//无法线 → 按满照处理，中心仍为纯白
	EXPECT_GE(center_brightness(pixels), 200u) << "无法线网格被光照压暗，破坏既有画面";
	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

/*
PBR 掠射高光能量修正（第六百七十二轮；D1 第一步）
口径：PBR 开启 ＋ 斜射主光。能量修正关 → 开：镜面乘入射余弦，画面出现差异；
关闭态为默认路径（逐字节不变），门面查询与实际开关一致。
*/
TEST_F(Renderer_Scene_Test, PBR掠射高光能量修正生效)
{
	//单次运行的产物（画面 ＋ 开关状态）
	struct Outcome
	{
		std::vector<uint8_t> pixels;
		bool energy = false;
		bool ok = false;
	};
	auto run = [](bool energy)
	{
		Outcome outcome;
		engine::Renderer renderer;
		if (!renderer.init(hidden_config()) || !renderer.readback_supported())
			return outcome;
		uint64_t texture = 0;
		std::vector<uint8_t> texels = solid_white();
		if (!renderer.texture_load(texture, texels.data(), 2, 2))
			return outcome;
		engine::Mesh_Handle mesh = 0;
		if (!renderer.mesh_load(mesh, quad_data_with_normal(0.0f, 0.0f, 1.0f)))
			return outcome;
		renderer.texture_bind_set(texture);

		//PBR 开启、主光斜射（入射余弦约 0.6）、纯金属放大镜面，便于观察能量修正
		engine::Light_Data light{};
		light.direction[0] = -0.8f;
		light.direction[2] = -0.6f;
		light.intensity = 1.0f;
		light.ambient = 0.0f;
		light.pbr_enabled = 1;
		light.pbr_energy = energy ? 1 : 0;
		light.pbr_metallic = 1.0f;
		//提升镜面强度（默认 0 时整段镜面为 0，无法体现能量修正）
		light.specular_intensity = 2.0f;
		engine::Camera_Data camera{};
		engine::Fog_Data fog{};
		renderer.scene_apply(camera, light, fog);

		EXPECT_TRUE(renderer.mesh_draw(mesh, full_screen_matrix().data()));
		outcome.pixels.resize(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(outcome.pixels.data(),
			static_cast<std::uint32_t>(outcome.pixels.size())));
		outcome.energy = renderer.pbr_energy();
		outcome.ok = true;
		return outcome;
	};

	const Outcome plain = run(false);
	const Outcome energy = run(true);
	ASSERT_TRUE(plain.ok) << "默认 PBR 配置初始化失败";
	ASSERT_TRUE(energy.ok) << "能量修正配置初始化失败";
	//门面状态与实际开关一致
	EXPECT_FALSE(plain.energy) << "默认态不应开启能量修正";
	EXPECT_TRUE(energy.energy) << "能量修正未生效";
	//统计画面差异像素
	uint32_t difference = 0;
	for (uint32_t y = 0; y < WINDOW_HEIGHT; ++y)
		for (uint32_t x = 0; x < WINDOW_WIDTH; ++x)
		{
			std::array<uint8_t, 4> first = pixel_get(plain.pixels, x, y);
			std::array<uint8_t, 4> second = pixel_get(energy.pixels, x, y);
			if (first != second)
				++difference;
		}
	RecordProperty("energy_difference_pixels", difference);
	EXPECT_GT(difference, 100u) << "能量修正未改变掠射镜面（差异像素 " << difference << "）";
}

/*
PBR 漫反射菲涅尔能量分摊（第六百七十四轮；D1 第二步）
口径：介电材质（金属度 0）＋ 正面视角。能量模式关 → 开：漫反射按 (1 − 0.04) 略降，
整面亮度小幅下降（差异像素覆盖整个受光面）。
*/
TEST_F(Renderer_Scene_Test, PBR漫反射能量分摊生效)
{
	struct Outcome
	{
		std::vector<uint8_t> pixels;
		uint32_t brightness = 0;
		bool ok = false;
	};
	auto run = [](bool energy)
	{
		Outcome outcome;
		engine::Renderer renderer;
		if (!renderer.init(hidden_config()) || !renderer.readback_supported())
			return outcome;
		uint64_t texture = 0;
		std::vector<uint8_t> texels = solid_white();
		if (!renderer.texture_load(texture, texels.data(), 2, 2))
			return outcome;
		engine::Mesh_Handle mesh = 0;
		if (!renderer.mesh_load(mesh, quad_data_with_normal(0.0f, 0.0f, 1.0f)))
			return outcome;
		renderer.texture_bind_set(texture);

		//PBR 介电材质，仅漫反射参与（镜面强度 0）
		engine::Light_Data light{};
		light.direction[0] = -0.4f;
		light.direction[2] = -0.9f;
		light.intensity = 1.0f;
		light.ambient = 0.0f;
		light.pbr_enabled = 1;
		light.pbr_energy = energy ? 1 : 0;
		light.pbr_metallic = 0.0f;
		light.specular_intensity = 0.0f;
		engine::Camera_Data camera{};
		engine::Fog_Data fog{};
		renderer.scene_apply(camera, light, fog);

		EXPECT_TRUE(renderer.mesh_draw(mesh, full_screen_matrix().data()));
		outcome.pixels.resize(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(outcome.pixels.data(),
			static_cast<std::uint32_t>(outcome.pixels.size())));
		outcome.brightness = center_brightness(outcome.pixels);
		outcome.ok = true;
		return outcome;
	};

	const Outcome plain = run(false);
	const Outcome energy = run(true);
	ASSERT_TRUE(plain.ok) << "默认 PBR 配置初始化失败";
	ASSERT_TRUE(energy.ok) << "能量分摊配置初始化失败";
	RecordProperty("diffuse_energy_plain", plain.brightness);
	RecordProperty("diffuse_energy_on", energy.brightness);
	//统计差异像素（逐通道差 ≥ 1）
	uint32_t difference = 0;
	for (uint32_t y = 0; y < WINDOW_HEIGHT; ++y)
		for (uint32_t x = 0; x < WINDOW_WIDTH; ++x)
			if (pixel_get(plain.pixels, x, y) != pixel_get(energy.pixels, x, y))
				++difference;
	RecordProperty("diffuse_energy_difference_pixels", difference);
	//正面视角：漫反射略降（中心亮度不升）
	EXPECT_LE(energy.brightness, plain.brightness) << "能量分摊反而提亮了漫反射";
	EXPECT_GT(difference, 1000u) << "能量分摊未覆盖受光面（差异像素 " << difference << "）";
}

/*
非法光照参数整次忽略（第二十九轮）：零向量方向 / 非数 / 越界颜色均不改变画面
*/
TEST_F(Renderer_Scene_Test, 非法光照参数整次忽略)
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

	//渲染一帧（默认光照）作为基线（lambda 内用 EXPECT：ASSERT 不允许出现在有返回值的函数中）
	auto render_frame = [&]()
	{
		EXPECT_TRUE(renderer.mesh_draw(mesh, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return pixels;
	};
	std::vector<uint8_t> baseline = render_frame();
	//零长度方向（无法归一化）→ 整次拒绝
	const float zero_direction[3] = { 0.0f, 0.0f, 0.0f };
	const float white[3] = { 1.0f, 1.0f, 1.0f };
	renderer.light_set(zero_direction, white, 1.0f, 0.0f);
	//非数方向 → 整次拒绝
	const float nan_direction[3] = { std::nanf(""), 0.0f, -1.0f };
	renderer.light_set(nan_direction, white, 1.0f, 0.0f);
	//越界颜色（1.5）→ 整次拒绝
	const float over_color[3] = { 1.5f, 1.0f, 1.0f };
	const float forward[3] = { 0.0f, 0.0f, -1.0f };
	renderer.light_set(forward, over_color, 1.0f, 0.0f);
	//三次非法调用后画面应与基线逐字节一致
	EXPECT_EQ(baseline, render_frame()) << "非法光照参数改变了画面（未整次拒绝）";

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

/*
正交投影（第二十轮）：同一四边形在相机远近不同时屏幕占比不变；透视下则明显变小
口径：纯红纹理 ＋ 覆盖视口中央的四边形（单位矩阵）；统计「红饱和（外通道 ≥ 200）」的像素数。
      透视：z = 5 的红色像素数应显著少于 z = 3；
      正交（ortho_height = 2）：z = 3 与 z = 5 的红色像素数应基本一致（差 ≤ 2%）。
*/

TEST_F(Renderer_Scene_Test, 立方体正面随光照方向明暗反转)
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

	//光沿 -Z（默认）：+Z 面正对光源 → 满照
	uint32_t lit = render_brightness();
	//光沿 +Z：+Z 面背光 → 照度 0
	const float toward_camera[3] = { 0.0f, 0.0f, 1.0f };
	const float white_light[3] = { 1.0f, 1.0f, 1.0f };
	renderer.light_set(toward_camera, white_light, 1.0f, 0.0f);
	uint32_t dark = render_brightness();
	RecordProperty("cube_lit", lit);
	RecordProperty("cube_dark", dark);
	EXPECT_GE(lit, 200u) << "立方体正面未被照亮（中心亮度 " << lit << "）";
	EXPECT_LE(dark, 40u) << "立方体正面未被压暗（中心亮度 " << dark << "）";

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

/*
旋转立方体的两个可见面亮度不同（第三十轮）
口径：立方体绕 Y 轴 -35°（+X 面转向右侧、+Z 面转向左侧），光沿 -X 传播（光源在 +X）：
      +X 面受光（约 0.82）、+Z 面背光（0）；比较左右两侧区域的最大亮度，避开像素级对齐。
*/
TEST_F(Renderer_Scene_Test, 旋转立方体两侧面明暗不同)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, cube_data(0.6f)));
	renderer.texture_bind_set(texture);
	//光沿 -X 传播（光源在 +X 一侧）
	const float from_positive_x[3] = { -1.0f, 0.0f, 0.0f };
	const float white_light[3] = { 1.0f, 1.0f, 1.0f };
	renderer.light_set(from_positive_x, white_light, 1.0f, 0.0f);
	//旋转 -35°（+X 面转向相机右侧）
	std::array<float, 16> model = rotation_y_matrix(-0.6109f);
	ASSERT_TRUE(renderer.mesh_draw(mesh, model.data()));
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));

	//左右两侧区域的最大亮度（立方体投影约占中心 40% 宽，故取中心两侧各 1/8 宽、纵向中部）
	uint32_t left_max = 0;
	uint32_t right_max = 0;
	for (uint32_t y = WINDOW_HEIGHT / 4; y < WINDOW_HEIGHT * 3 / 4; ++y)
	{
		for (uint32_t x = WINDOW_WIDTH * 3 / 8; x < WINDOW_WIDTH * 7 / 16; ++x)
		{
			std::array<uint8_t, 4> value = pixel_get(pixels, x, y);
			uint32_t brightness = (static_cast<uint32_t>(value[0]) + value[1] + value[2]) / 3;
			if (brightness > left_max)
				left_max = brightness;
		}
		for (uint32_t x = WINDOW_WIDTH * 9 / 16; x < WINDOW_WIDTH * 5 / 8; ++x)
		{
			std::array<uint8_t, 4> value = pixel_get(pixels, x, y);
			uint32_t brightness = (static_cast<uint32_t>(value[0]) + value[1] + value[2]) / 3;
			if (brightness > right_max)
				right_max = brightness;
		}
	}
	RecordProperty("cube_left_max", left_max);
	RecordProperty("cube_right_max", right_max);
	EXPECT_GE(right_max, 120u) << "受光面亮度不足（右侧 " << right_max << "）";
	//左侧取样区含背景（清屏色亮度约 71），故以「不高于背景」为压暗判据
	EXPECT_LE(left_max, 90u) << "背光面未被压暗（左侧 " << left_max << "）";
	EXPECT_GT(static_cast<int>(right_max), static_cast<int>(left_max) + 60) << "两侧面明暗未区分";

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

/*
环境光抬升背光面（第三十轮）
口径：法线 (0,0,-1) 的全屏四边形在默认光下照度 0；`light_set` 把环境光抬到 0.5 后中心亮度约 127。
*/
TEST_F(Renderer_Scene_Test, 环境光抬升背光面)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
	renderer.texture_bind_set(texture);

	//渲染一帧并返回中心亮度（lambda 内用 EXPECT：ASSERT 不允许出现在有返回值的函数中）
	auto render_brightness = [&](const float* model)
	{
		EXPECT_TRUE(renderer.mesh_draw(mesh, model));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return center_brightness(pixels);
	};
	//默认光照（环境光 0）：背光面全黑
	uint32_t without_ambient = render_brightness(nullptr);
	//环境光 0.5：背光面被抬亮
	const float forward[3] = { 0.0f, 0.0f, -1.0f };
	const float white_light[3] = { 1.0f, 1.0f, 1.0f };
	renderer.light_set(forward, white_light, 1.0f, 0.5f);
	uint32_t with_ambient = render_brightness(nullptr);
	RecordProperty("ambient_off", without_ambient);
	RecordProperty("ambient_half", with_ambient);
	EXPECT_LE(without_ambient, 40u) << "背光面未全黑（亮度 " << without_ambient << "）";
	EXPECT_GE(with_ambient, 90u) << "环境光未抬亮背光面（亮度 " << with_ambient << "）";
	EXPECT_LE(with_ambient, 210u) << "环境光亮度超出预期（亮度 " << with_ambient << "）";

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}
/*
点光源距离衰减（第三十一轮）
口径：法线 (0,0,1) 的全屏四边形，点光源放在 +Z 轴上不同距离、半径 3：
      距离 1 → 衰减 0.67（亮），距离 2.9 → 衰减 0.03（暗）。
*/
TEST_F(Renderer_Scene_Test, 点光源近远亮度不同)
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

	//渲染一帧并返回中心亮度（lambda 内用 EXPECT：ASSERT 不允许出现在有返回值的函数中）
	auto render_brightness = [&]()
	{
		EXPECT_TRUE(renderer.mesh_draw(mesh, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return center_brightness(pixels);
	};

	//近距点光源（距离 1，半径 3）
	const float near_position[3] = { 0.0f, 0.0f, 1.0f };
	renderer.light_point_set(near_position, white_light, 3.0f, 1.0f, 0.0f);
	uint32_t near_brightness = render_brightness();
	//远距点光源（距离 2.9，半径 3 → 衰减约 0.03）
	const float far_position[3] = { 0.0f, 0.0f, 2.9f };
	renderer.light_point_set(far_position, white_light, 3.0f, 1.0f, 0.0f);
	uint32_t far_brightness = render_brightness();
	RecordProperty("point_near", near_brightness);
	RecordProperty("point_far", far_brightness);
	EXPECT_GE(near_brightness, 170u) << "近距点光源未照亮（亮度 " << near_brightness << "）";
	EXPECT_LE(far_brightness, 80u) << "远距点光源未衰减（亮度 " << far_brightness << "）";
	EXPECT_GT(static_cast<int>(near_brightness), static_cast<int>(far_brightness) + 80) << "距离衰减未生效";

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

/*
镜面高光提升亮度，非法高光参数整次忽略（第三十一轮）
口径：中灰纹理 ＋ 法线 (0,0,1) 全屏四边形；光沿 -Z、相机在 +Z，故 H = N，高光强度直接叠加到照度上。
      高光 0 时亮度约 188（线性 0.5 的 sRGB 编码）；高光 0.6 时被钳制到 1.0（约 255）。
*/
TEST_F(Renderer_Scene_Test, 镜面高光提升亮度)
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

	//高光关闭（默认）
	uint32_t plain = render_brightness();
	//高光 0.6、光泽度 16（H = N → pow(1, 16) = 1）
	renderer.specular_set(0.6f, 16.0f);
	uint32_t highlighted = render_brightness();
	RecordProperty("spec_plain", plain);
	RecordProperty("spec_highlight", highlighted);
	EXPECT_GE(highlighted, 230u) << "高光未提升亮度（" << plain << " → " << highlighted << "）";
	EXPECT_GT(static_cast<int>(highlighted), static_cast<int>(plain) + 30) << "高光增量不足";
	//非法高光参数（负强度 / 光泽度小于 1）整次忽略 → 画面与上一帧逐字节一致
	renderer.specular_set(-1.0f, 0.0f);
	EXPECT_TRUE(renderer.mesh_draw(mesh, nullptr));
	std::vector<uint8_t> after(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(after.data(), static_cast<std::uint32_t>(after.size())));
	EXPECT_EQ(center_brightness(after), highlighted) << "非法高光参数改变了画面";

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}
/*
多个点光源累加（第三十二轮）
口径：法线 (0,0,1) 全屏四边形、半径 3；单灯（中心 0,0,1）与双灯（±0.5, 0, 1）：
      单灯照度约 0.67；双灯各约 0.56，累加后钳制到 1.0 → 画面更亮。
*/
TEST_F(Renderer_Scene_Test, 多个点光源累加更亮)
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
	const float white_light[3] = { 1.0f, 1.0f, 1.0f };

	//渲染一帧并返回中心亮度（lambda 内用 EXPECT：ASSERT 不允许出现在有返回值的函数中）
	auto render_brightness = [&]()
	{
		EXPECT_TRUE(renderer.mesh_draw(mesh, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return center_brightness(pixels);
	};

	//单灯（中心）
	const float single_position[3] = { 0.0f, 0.0f, 1.0f };
	ASSERT_TRUE(renderer.light_add_point(single_position, white_light, 3.0f, 1.0f, 0.0f));
	uint32_t single = render_brightness();
	//追加第二盏灯（左移 0.5）
	const float second_position[3] = { -0.5f, 0.0f, 1.0f };
	ASSERT_TRUE(renderer.light_add_point(second_position, white_light, 3.0f, 1.0f, 0.0f));
	uint32_t doubled = render_brightness();
	RecordProperty("single_light", single);
	RecordProperty("double_light", doubled);
	EXPECT_GE(doubled, single + 15u) << "双灯未比单灯更亮（" << single << " → " << doubled << "）";

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

/*
点光源数量上限与清空（第三十二轮；第六十二轮上限 4 → 8；第六百九十九轮 8 → 16）
口径：上限 16 个；第 17 个返回假且画面不变；`lights_clear` 后回到默认方向光。
*/
TEST_F(Renderer_Scene_Test, 点光源上限与清空)
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

	//渲染一帧并返回中心亮度（lambda 内用 EXPECT：ASSERT 不允许出现在有返回值的函数中）
	auto render_brightness = [&]()
	{
		EXPECT_TRUE(renderer.mesh_draw(mesh, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return center_brightness(pixels);
	};

	//登记 16 个点光源（高度避开中心像素，避免完全饱和而无法比较）
	const float positions[16][3] = {
		{ -1.5f, 0.0f, 1.0f }, { -0.5f, 0.0f, 1.0f }, { 0.5f, 0.0f, 1.0f }, { 1.5f, 0.0f, 1.0f },
		{ -1.5f, 0.5f, 1.0f }, { -0.5f, 0.5f, 1.0f }, { 0.5f, 0.5f, 1.0f }, { 1.5f, 0.5f, 1.0f },
		{ -1.5f, -0.5f, 1.0f }, { -0.5f, -0.5f, 1.0f }, { 0.5f, -0.5f, 1.0f }, { 1.5f, -0.5f, 1.0f },
		{ -1.5f, 1.0f, 1.0f }, { -0.5f, 1.0f, 1.0f }, { 0.5f, 1.0f, 1.0f }, { 1.5f, 1.0f, 1.0f },
	};
	for (uint32_t index = 0; index < 16; ++index)
		ASSERT_TRUE(renderer.light_add_point(positions[index], white_light, 6.0f, 0.4f, 0.0f));
	uint32_t full = render_brightness();
	//第 17 个被拒（上限 16），画面不变
	ASSERT_FALSE(renderer.light_add_point(positions[0], white_light, 6.0f, 0.4f, 0.0f));
	EXPECT_EQ(render_brightness(), full) << "超上限的追加改变了画面";
	//清空后回到默认方向光（满照，纯白中心）
	renderer.lights_clear();
	EXPECT_GE(render_brightness(), 200u) << "清空点光源后未回到方向光满照";

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

/*
第八盏点光源参与着色（第六十二轮）
口径：方向光强度 0、环境光 0；先登记 7 盏强度 0 的点光源 → 中心暗；
     第 8 盏（中心正前方、强度 1）登记后中心被照亮，证明循环上限确为 8。
*/
TEST_F(Renderer_Scene_Test, 第八盏点光源参与着色)
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

	//方向光关闭、环境光 0
	const float forward[3] = { 0.0f, 0.0f, -1.0f };
	const float white_light[3] = { 1.0f, 1.0f, 1.0f };
	renderer.light_set(forward, white_light, 0.0f, 0.0f);

	//渲染一帧并返回中心亮度
	auto render_brightness = [&]()
	{
		EXPECT_TRUE(renderer.mesh_draw(mesh, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return center_brightness(pixels);
	};

	//前 7 盏强度 0（占位）
	const float center_light[3] = { 0.0f, 0.0f, 1.0f };
	for (uint32_t index = 0; index < 7; ++index)
		ASSERT_TRUE(renderer.light_add_point(center_light, white_light, 6.0f, 0.0f, 0.0f));
	uint32_t before = render_brightness();
	//第 8 盏强度 1 → 中心被照亮
	ASSERT_TRUE(renderer.light_add_point(center_light, white_light, 6.0f, 1.0f, 0.0f));
	uint32_t after = render_brightness();
	RecordProperty("light8_before", before);
	RecordProperty("light8_after", after);
	EXPECT_LE(before, 40u) << "占位光源意外照亮了中心（" << before << "）";
	EXPECT_GE(after, 150u) << "第 8 盏点光源未参与着色（" << after << "）";

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}
/*
场景描述携带点光源（第三十三轮）
口径：中灰纹理 ＋ 法线 (0,0,1) 四边形；同一 Scene_Data 两次应用（0 盏 vs 2 盏点光源），
      其余参数（方向光强度 0、环境光 0）相同 → 仅点光源造成亮度差。
*/
TEST_F(Renderer_Scene_Test, 场景描述携带点光源更亮)
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

	//仅方向光（强度 0 → 全黑）
	engine::Scene_Data scene{};
	scene.light_intensity = 0.0f;
	renderer.scene_apply(scene);
	uint32_t without_points = render_brightness();
	//追加两盏点光源（左右各一，半径 3）
	scene.light_count = 2;
	scene.point_light_position[0][0] = -0.5f;
	scene.point_light_position[0][1] = 0.0f;
	scene.point_light_position[0][2] = 1.0f;
	scene.point_light_position[1][0] = 0.5f;
	scene.point_light_position[1][1] = 0.0f;
	scene.point_light_position[1][2] = 1.0f;
	scene.point_light_radius[0] = 3.0f;
	scene.point_light_radius[1] = 3.0f;
	renderer.scene_apply(scene);
	uint32_t with_points = render_brightness();
	RecordProperty("scene_no_points", without_points);
	RecordProperty("scene_two_points", with_points);
	EXPECT_LE(without_points, 40u) << "方向光关闭时画面未变暗（亮度 " << without_points << "）";
	EXPECT_GE(with_points, 120u) << "场景描述中的点光源未生效（亮度 " << with_points << "）";

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

/*
光照场景上的文字仍可见（第三十三轮）
口径：纯白纹理立方体 ＋ 方向光渲染；再叠一行文字；断言文字区域存在明显亮像素
      （文字走独立着色器，不受场景光照影响；字体缺失时跳过）。
*/
TEST_F(Renderer_Scene_Test, 光照场景上的文字仍可见)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//系统等宽字体缺失则跳过
	uint64_t font = 0;
	if (!renderer.font_load_file(font, "C:\\Windows\\Fonts\\consola.ttf", 16))
		GTEST_SKIP() << "系统中缺少 consola.ttf";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, cube_data(0.5f)));
	renderer.texture_bind_set(texture);

	//场景：方向光（默认）+ 白色文字叠在画面上
	engine::Scene_Data scene{};
	renderer.scene_apply(scene);
	const float white_text[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	renderer.text_color_set(white_text);
	ASSERT_TRUE(renderer.mesh_draw(mesh, nullptr));
	ASSERT_TRUE(renderer.text_draw(font, "LIGHT", 4.0f, 4.0f));
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));

	//文字所在区域（左上角）统计接近纯白的像素数
	uint32_t bright_count = 0;
	for (uint32_t y = 0; y < WINDOW_HEIGHT / 3; ++y)
		for (uint32_t x = 0; x < WINDOW_WIDTH / 3; ++x)
		{
			std::array<uint8_t, 4> value = pixel_get(pixels, x, y);
			if (value[0] >= 200 && value[1] >= 200 && value[2] >= 200)
				++bright_count;
		}
	RecordProperty("text_bright_pixels", bright_count);
	EXPECT_GT(bright_count, 10u) << "光照场景上的文字不可见（亮像素 " << bright_count << "）";

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

//逐像素差异计数（RGBA 任一通道不同即记一处；第六百九十九轮）
static uint32_t pixel_difference(const std::vector<uint8_t>& left, const std::vector<uint8_t>& right)
{
	//尺寸不一致按最大差异
	if (left.size() != right.size())
		return 0xFFFFFFFFu;
	uint32_t differences = 0;
	for (std::size_t index = 0; index + 3 < left.size(); index += 4)
	{
		if (left[index] != right[index] || left[index + 1] != right[index + 1]
			|| left[index + 2] != right[index + 2] || left[index + 3] != right[index + 3])
			++differences;
	}
	return differences;
}

/*
前向高 8 槽参与着色（第六百九十九轮；A1 剩余项 8 → 16）
口径：方向光强度 0、环境光 0；先登记 15 盏强度 0 的占位光源 → 中心暗；
     第 16 盏（中心正前方、强度 1）登记后中心被照亮，证明前向循环确为 16 槽。
*/
TEST_F(Renderer_Scene_Test, 第十六盏点光源参与着色)
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

	//方向光关闭、环境光 0
	const float forward[3] = { 0.0f, 0.0f, -1.0f };
	const float white_light[3] = { 1.0f, 1.0f, 1.0f };
	renderer.light_set(forward, white_light, 0.0f, 0.0f);

	//渲染一帧并返回中心亮度
	auto render_brightness = [&]()
	{
		EXPECT_TRUE(renderer.mesh_draw(mesh, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return center_brightness(pixels);
	};

	//前 15 盏强度 0（占位）
	const float center_light[3] = { 0.0f, 0.0f, 1.0f };
	for (uint32_t index = 0; index < 15; ++index)
		ASSERT_TRUE(renderer.light_add_point(center_light, white_light, 6.0f, 0.0f, 0.0f));
	uint32_t before = render_brightness();
	//第 16 盏强度 1 → 中心被照亮
	ASSERT_TRUE(renderer.light_add_point(center_light, white_light, 6.0f, 1.0f, 0.0f));
	uint32_t after = render_brightness();
	RecordProperty("light16_before", before);
	RecordProperty("light16_after", after);
	EXPECT_LE(before, 40u) << "占位光源意外照亮了中心（" << before << "）";
	EXPECT_GE(after, 150u) << "第 16 盏点光源未参与着色（" << after << "）";

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

/*
高 8 槽零强度不改变画面（第六百九十九轮）
口径：8 盏相机光 → 画面 A；追加 4 盏强度 0（落高 8 槽）→ 画面 B 与 A 逐字节一致；
     该 4 盏改为强度 0.8 → 画面 C 与 A 出现差异（证明高槽数据确实进入着色器）。
*/
TEST_F(Renderer_Scene_Test, 高八槽零强度逐字节一致)
{
	//指定高 8 槽额外光源数量与强度的一次运行
	auto run = [](uint32_t extra_count, float extra_intensity)
	{
		std::vector<uint8_t> pixels;
		engine::Renderer renderer;
		if (!renderer.init(hidden_config()) || !renderer.readback_supported())
			return pixels;
		uint64_t texture = 0;
		std::vector<uint8_t> texels = solid_white();
		if (!renderer.texture_load(texture, texels.data(), 2, 2))
			return pixels;
		engine::Mesh_Handle mesh = 0;
		if (!renderer.mesh_load(mesh, quad_data_with_normal(0.0f, 0.0f, 1.0f)))
			return pixels;
		renderer.texture_bind_set(texture);
		//方向光关闭、环境光 0，仅点光贡献
		const float forward[3] = { 0.0f, 0.0f, -1.0f };
		const float white_light[3] = { 1.0f, 1.0f, 1.0f };
		renderer.light_set(forward, white_light, 0.0f, 0.0f);
		//低 8 槽：四角偏上位置，强度 0.1（保持不饱和，便于观察高槽增量）
		const float low_positions[8][3] = {
			{ -1.5f, 0.0f, 1.0f }, { -0.5f, 0.0f, 1.0f }, { 0.5f, 0.0f, 1.0f }, { 1.5f, 0.0f, 1.0f },
			{ -1.5f, 0.5f, 1.0f }, { -0.5f, 0.5f, 1.0f }, { 0.5f, 0.5f, 1.0f }, { 1.5f, 0.5f, 1.0f },
		};
		for (uint32_t index = 0; index < 8; ++index)
			if (!renderer.light_add_point(low_positions[index], white_light, 6.0f, 0.1f, 0.0f))
				return pixels;
		//高 8 槽前 extra_count 盏：底部位置，强度由参数控制
		const float high_positions[4][3] = {
			{ -1.5f, -0.5f, 1.0f }, { -0.5f, -0.5f, 1.0f }, { 0.5f, -0.5f, 1.0f }, { 1.5f, -0.5f, 1.0f },
		};
		for (uint32_t index = 0; index < extra_count; ++index)
			if (!renderer.light_add_point(high_positions[index], white_light, 6.0f, extra_intensity, 0.0f))
				return pixels;

		if (!renderer.mesh_draw(mesh, nullptr))
			return pixels;
		pixels.resize(pixel_bytes());
		if (!renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())))
			pixels.clear();
		renderer.mesh_unload(mesh);
		renderer.texture_unload(texture);
		return pixels;
	};

	//画面 A：仅低 8 槽；画面 B：低 8 槽 ＋ 高 8 槽 4 盏零强度；画面 C：高槽 4 盏强度 0.8
	std::vector<uint8_t> baseline = run(0u, 0.0f);
	std::vector<uint8_t> high_zero = run(4u, 0.0f);
	std::vector<uint8_t> high_lit = run(4u, 0.8f);
	ASSERT_FALSE(baseline.empty()) << "基线画面为空";
	ASSERT_FALSE(high_zero.empty()) << "零强度画面为空";
	ASSERT_FALSE(high_lit.empty()) << "高槽点亮画面为空";
	//高槽零强度的 4 盏不改变画面
	EXPECT_EQ(pixel_difference(baseline, high_zero), 0u) << "高 8 槽零强度改变了画面";
	//高槽点亮后出现差异
	uint32_t differences = pixel_difference(baseline, high_lit);
	RecordProperty("high_slot_difference", differences);
	EXPECT_GT(differences, 0u) << "高 8 槽强度未进入着色器";
}
