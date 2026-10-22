/*
渲染光照测试（方向光 / 点光源 / 聚光灯 / 衰减与光照参数校验）
（第五十七轮由「渲染场景数据测试.cpp」按功能域拆分；逻辑未改）
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
点光源数量上限与清空（第三十二轮）
口径：上限 4 个；第 5 个返回假且画面不变；`lights_clear` 后回到默认方向光。
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

	//登记 4 个点光源（高度避开中心像素，避免完全饱和而无法比较）
	const float positions[4][3] = {
		{ -1.5f, 0.0f, 1.0f }, { -0.5f, 0.0f, 1.0f }, { 0.5f, 0.0f, 1.0f }, { 1.5f, 0.0f, 1.0f },
	};
	for (uint32_t index = 0; index < 4; ++index)
		ASSERT_TRUE(renderer.light_add_point(positions[index], white_light, 6.0f, 0.4f, 0.0f));
	uint32_t full = render_brightness();
	//第 5 个被拒（上限 4），画面不变
	ASSERT_FALSE(renderer.light_add_point(positions[0], white_light, 6.0f, 0.4f, 0.0f));
	EXPECT_EQ(render_brightness(), full) << "超上限的追加改变了画面";
	//清空后回到默认方向光（满照，纯白中心）
	renderer.lights_clear();
	EXPECT_GE(render_brightness(), 200u) << "清空点光源后未回到方向光满照";

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
聚光灯参数校验与数量上限（第三十四轮）
口径：内角 ≥ 外角、外角越界（> 90°）、方向退化为零向量均被拒绝；
      合法登记 4 盏后第 5 盏被拒；清空后可重新登记。
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
	//合法登记 4 盏（共用点光源上限）
	for (uint32_t index = 0; index < 4; ++index)
		EXPECT_TRUE(renderer.light_add_spot(position, forward, white_light, 10.0f, 1.0f, 10.0f, 20.0f, 0.0f));
	//第 5 盏被拒
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
