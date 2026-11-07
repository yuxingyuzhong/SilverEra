/*
渲染场景效果测试（环境光颜色 / 半球环境光 / 逐光与逐盏高光）
（第五十九轮按功能域拆分：雾相关见「渲染雾测试.cpp」；逻辑未改）
*/
#include "渲染场景测试辅助.h"

TEST_F(Renderer_Scene_Test, 环境光颜色着色)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_gray();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
	renderer.texture_bind_set(texture);
	//方向光朝 -Z + 环境光 0.5：背光面只吃环境光
	const float forward[3] = { 0.0f, 0.0f, -1.0f };
	const float white_light[3] = { 1.0f, 1.0f, 1.0f };
	renderer.light_set(forward, white_light, 1.0f, 0.5f);

	//渲染一帧并返回中心像素（lambda 内用 EXPECT：ASSERT 不允许出现在有返回值的函数中）
	auto render_center = [&]()
	{
		EXPECT_TRUE(renderer.mesh_draw(mesh, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	};

	std::array<uint8_t, 4> plain = render_center();
	//纯红环境光
	const float red[3] = { 1.0f, 0.0f, 0.0f };
	renderer.ambient_color_set(red);
	std::array<uint8_t, 4> tinted = render_center();
	RecordProperty("ambient_plain", pixel_text(plain));
	RecordProperty("ambient_red", pixel_text(tinted));
	//默认白：三通道接近相等
	EXPECT_LE(std::abs(static_cast<int>(plain[0]) - static_cast<int>(plain[1])), 3) << "默认环境光非中性";
	//纯红：外通道（R 与 B 的较大者）保留、另一外通道与绿通道接近 0（通道序无关）
	uint8_t outer = outer_max(tinted);
	uint8_t inner = tinted[0] < tinted[2] ? tinted[0] : tinted[2];
	EXPECT_GE(outer, 50u) << "红色环境光未保留外通道（" << pixel_text(tinted) << "）";
	EXPECT_LE(inner, 30u) << "另一外通道未被压暗（" << pixel_text(tinted) << "）";
	EXPECT_LE(tinted[1], 30u) << "绿色通道未被压暗（" << pixel_text(tinted) << "）";

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

/*
非法环境光颜色被忽略（第四十轮）
口径：分量越界（1.5）整次忽略；画面与调用前逐字节一致。
*/
TEST_F(Renderer_Scene_Test, 非法环境光颜色被忽略)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_gray();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
	renderer.texture_bind_set(texture);
	const float forward[3] = { 0.0f, 0.0f, -1.0f };
	const float white_light[3] = { 1.0f, 1.0f, 1.0f };
	renderer.light_set(forward, white_light, 1.0f, 0.5f);

	//渲染一帧并返回像素（lambda 内用 EXPECT：ASSERT 不允许出现在有返回值的函数中）
	auto render_frame = [&]()
	{
		EXPECT_TRUE(renderer.mesh_draw(mesh, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return pixels;
	};

	std::vector<uint8_t> baseline = render_frame();
	const float invalid[3] = { 1.5f, 0.0f, 0.0f };
	renderer.ambient_color_set(invalid);
	EXPECT_EQ(render_frame(), baseline) << "非法环境光颜色改变了画面";

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

/*
场景描述携带环境光颜色（第四十一轮）
口径：中灰纹理 ＋ 背光面 ＋ 环境光 0.5；同一 Scene_Data 把 ambient_color 由白改红：
      外通道保留、另一外通道与绿通道接近 0（通道序无关）。
*/
TEST_F(Renderer_Scene_Test, 场景描述携带环境光颜色)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_gray();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
	renderer.texture_bind_set(texture);

	//渲染一帧并返回中心像素（lambda 内用 EXPECT：ASSERT 不允许出现在有返回值的函数中）
	auto render_center = [&]()
	{
		EXPECT_TRUE(renderer.mesh_draw(mesh, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	};

	//场景：方向光朝 -Z ＋ 环境光 0.5（背光面只吃环境光）
	engine::Scene_Data scene{};
	scene.light_direction[0] = 0.0f;
	scene.light_direction[1] = 0.0f;
	scene.light_direction[2] = -1.0f;
	scene.light_intensity = 1.0f;
	scene.light_ambient = 0.5f;
	renderer.scene_apply(scene);
	std::array<uint8_t, 4> plain = render_center();
	//同一场景改红色环境光
	scene.ambient_color[0] = 1.0f;
	scene.ambient_color[1] = 0.0f;
	scene.ambient_color[2] = 0.0f;
	renderer.scene_apply(scene);
	std::array<uint8_t, 4> tinted = render_center();
	RecordProperty("scene_ambient_plain", pixel_text(plain));
	RecordProperty("scene_ambient_red", pixel_text(tinted));
	//默认白：三通道接近相等
	EXPECT_LE(std::abs(static_cast<int>(plain[0]) - static_cast<int>(plain[1])), 3) << "默认环境光非中性";
	//红色：外通道保留、另一外通道与绿通道接近 0
	uint8_t outer = outer_max(tinted);
	uint8_t inner = tinted[0] < tinted[2] ? tinted[0] : tinted[2];
	EXPECT_GE(outer, 50u) << "场景环境光颜色未保留外通道（" << pixel_text(tinted) << "）";
	EXPECT_LE(inner, 30u) << "另一外通道未被压暗（" << pixel_text(tinted) << "）";
	EXPECT_LE(tinted[1], 30u) << "绿色通道未被压暗（" << pixel_text(tinted) << "）";

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

/*
场景描述中的非法环境光颜色被忽略（第四十一轮）
口径：分量越界（2.0）整次忽略；画面与调用前逐字节一致。
*/
TEST_F(Renderer_Scene_Test, 场景描述非法环境光颜色被忽略)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_gray();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
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
	baseline.light_ambient = 0.5f;
	renderer.scene_apply(baseline);
	std::vector<uint8_t> plain = render_frame();
	//非法环境光颜色（2.0）应被整次忽略
	engine::Scene_Data invalid = baseline;
	invalid.ambient_color[0] = 2.0f;
	renderer.scene_apply(invalid);
	EXPECT_EQ(render_frame(), plain) << "场景描述中的非法环境光颜色改变了画面";

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

/*
平方反比下聚光灯仍只照锥内（第四十二轮）
口径：白纹理 ＋ 法线 (0,0,1) 四边形、方向光关闭；聚光灯 (0,0,1) 朝 -Z、窄锥（10°/20°）、半径 10、
      衰减模式切平方反比：中心（锥内）亮、角部 (41,9)（锥外）仍被压暗。
*/


TEST_F(Renderer_Scene_Test, 半球环境光上下异色)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	//朝上法线与朝下法线的两块四边形
	engine::Mesh_Handle up_mesh = 0;
	engine::Mesh_Handle down_mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(up_mesh, quad_data_with_normal(0.0f, 1.0f, 0.0f)));
	ASSERT_TRUE(renderer.mesh_load(down_mesh, quad_data_with_normal(0.0f, -1.0f, 0.0f)));
	renderer.texture_bind_set(texture);

	//渲染一帧并返回中心亮度（lambda 内用 EXPECT：ASSERT 不允许出现在有返回值的函数中）
	auto render_brightness = [&](engine::Mesh_Handle mesh)
	{
		EXPECT_TRUE(renderer.mesh_draw(mesh, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return center_brightness(pixels);
	};

	//关闭方向光，仅保留环境光（强度 0.5），天顶白、地面黑
	const float light_direction[3] = { 0.0f, 0.0f, -1.0f };
	const float white[3] = { 1.0f, 1.0f, 1.0f };
	const float black[3] = { 0.0f, 0.0f, 0.0f };
	renderer.light_set(light_direction, white, 0.0f, 0.5f);
	renderer.ambient_color_set(white);
	//启用满强度半球
	renderer.ambient_ground_set(black, 1.0f);
	uint32_t up = render_brightness(up_mesh);
	uint32_t down = render_brightness(down_mesh);
	//关闭半球（退化为单一环境光颜色）
	renderer.ambient_ground_set(black, 0.0f);
	uint32_t up_plain = render_brightness(up_mesh);
	uint32_t down_plain = render_brightness(down_mesh);

	RecordProperty("hemisphere_up", up);
	RecordProperty("hemisphere_down", down);
	RecordProperty("hemisphere_up_plain", up_plain);
	RecordProperty("hemisphere_down_plain", down_plain);

	EXPECT_GT(up, down + 80u) << "朝上法线未取天顶色（" << up << " vs " << down << "）";
	EXPECT_LT(down, 40u) << "朝下法线未取地面色（" << down << "）";
	EXPECT_GE(up, 100u) << "朝上法线亮度偏低（" << up << "）";
	EXPECT_NEAR(up_plain, down_plain, 5u) << "半球关闭后上下法线不应有差异（"
		<< up_plain << " vs " << down_plain << "）";

	renderer.mesh_unload(up_mesh);
	renderer.mesh_unload(down_mesh);
	renderer.texture_unload(texture);
}

/*
逐盏高光缩放（第四十九轮）
口径：四边形法线朝 +Z、关闭方向光与环境光、单点光源强度 0.4、半径 5：
      缩放 0（无高光）时中心仅漫反射；缩放 1 时叠加高光明显更亮；缩放 -1 整次拒绝。
*/
TEST_F(Renderer_Scene_Test, 逐盏高光缩放生效)
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

	//渲染一帧并返回中心亮度（lambda 内用 EXPECT：ASSERT 不允许出现在有返回值的函数中）
	auto render_brightness = [&]()
	{
		EXPECT_TRUE(renderer.mesh_draw(mesh, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return center_brightness(pixels);
	};

	const float white[3] = { 1.0f, 1.0f, 1.0f };
	const float light_direction[3] = { 0.0f, 0.0f, -1.0f };
	const float light_position[3] = { 0.0f, 0.0f, 2.5f };
	//关闭方向光与环境光，开启全局高光
	renderer.light_set(light_direction, white, 0.0f, 0.0f);
	renderer.specular_set(1.0f, 16.0f);
	//缩放 0：该灯不产生高光
	renderer.lights_clear();
	renderer.light_set(light_direction, white, 0.0f, 0.0f);
	ASSERT_TRUE(renderer.light_add_point(light_position, white, 5.0f, 0.4f, 0.0f, 1, 0.0f));
	uint32_t no_specular = render_brightness();
	//缩放 1：叠加高光
	renderer.lights_clear();
	renderer.light_set(light_direction, white, 0.0f, 0.0f);
	ASSERT_TRUE(renderer.light_add_point(light_position, white, 5.0f, 0.4f, 0.0f, 1, 1.0f));
	uint32_t with_specular = render_brightness();
	//非法缩放整次拒绝（数量保持 1）
	EXPECT_FALSE(renderer.light_add_point(light_position, white, 5.0f, 0.4f, 0.0f, 1, -1.0f));
	uint32_t after_invalid = render_brightness();

	RecordProperty("per_light_no_specular", no_specular);
	RecordProperty("per_light_with_specular", with_specular);
	RecordProperty("per_light_after_invalid", after_invalid);

	EXPECT_GT(with_specular, no_specular + 40u) << "逐盏高光缩放未生效（"
		<< no_specular << " vs " << with_specular << "）";
	EXPECT_NEAR(after_invalid, with_specular, 4u) << "非法高光缩放改变了画面（"
		<< after_invalid << " vs " << with_specular << "）";

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

/*
方向光阴影生效（第五十二轮）
口径：接收面法线朝 -Z（法线受光面朝光源）、方向光沿 +Z（光源在 -Z 侧）、强度 1、环境光 0；
     遮挡面位于接收面之后（z = -0.5）并缩到 0.6，故相机看不到它、光源却先看到它。
     关闭阴影时中心亮度与无遮挡基线一致；开启后中心被遮蔽变暗、而阴影外的接收面仍受光。
*/

TEST_F(Renderer_Scene_Test, 方向光与补光高光缩放生效)
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

	//渲染一帧并返回中心亮度（lambda 内用 EXPECT：ASSERT 不允许出现在有返回值的函数中）
	auto render_brightness = [&]()
	{
		EXPECT_TRUE(renderer.mesh_draw(mesh, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return center_brightness(pixels);
	};

	const float white[3] = { 1.0f, 1.0f, 1.0f };
	const float light_direction[3] = { 0.0f, 0.0f, -1.0f };
	//方向光 0.4、环境光 0、全局高光开启
	renderer.light_set(light_direction, white, 0.4f, 0.0f);
	renderer.specular_set(1.0f, 16.0f);
	//方向光高光缩放 0 / 1
	renderer.directional_specular_set(0.0f);
	uint32_t dir_no_spec = render_brightness();
	renderer.directional_specular_set(1.0f);
	uint32_t dir_spec = render_brightness();
	//非法缩放整次忽略（保持 1）
	renderer.directional_specular_set(-1.0f);
	uint32_t dir_after_invalid = render_brightness();

	//补光 0.4（主光关闭）：高光缩放 0 / 1
	renderer.light_set(light_direction, white, 0.0f, 0.0f);
	renderer.light_fill_set(light_direction, white, 0.4f, 0.0f);
	uint32_t fill_no_spec = render_brightness();
	renderer.light_fill_set(light_direction, white, 0.4f, 1.0f);
	uint32_t fill_spec = render_brightness();

	RecordProperty("dir_no_spec", dir_no_spec);
	RecordProperty("dir_spec", dir_spec);
	RecordProperty("dir_after_invalid", dir_after_invalid);
	RecordProperty("fill_no_spec", fill_no_spec);
	RecordProperty("fill_spec", fill_spec);

	EXPECT_GT(dir_spec, dir_no_spec + 40u) << "方向光高光缩放未生效（"
		<< dir_no_spec << " vs " << dir_spec << "）";
	EXPECT_NEAR(dir_after_invalid, dir_spec, 4u) << "非法方向光缩放改变了画面（"
		<< dir_after_invalid << " vs " << dir_spec << "）";
	EXPECT_GT(fill_spec, fill_no_spec + 40u) << "补光高光缩放未生效（"
		<< fill_no_spec << " vs " << fill_spec << "）";

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

/*
逐盏光泽度生效（第五十一轮）
口径：斜视相机（中心处 半程向量 不重合法线）、关闭方向光与环境光、单点光源强度 0.4 / 半径 5、全局光泽度 16：
      低光泽度（2）的高光更宽、中心更亮；高光泽度（64）更窄、中心更暗；非法光泽度（0.5）整次拒绝。
*/
TEST_F(Renderer_Scene_Test, 逐盏光泽度生效)
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

	//渲染一帧并返回中心亮度（lambda 内用 EXPECT：ASSERT 不允许出现在有返回值的函数中）
	auto render_brightness = [&]()
	{
		EXPECT_TRUE(renderer.mesh_draw(mesh, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return center_brightness(pixels);
	};

	//斜视相机：机位 (1, 0, 3)、注视原点，使中心处半程向量不重合法线
	engine::Camera_Data camera{};
	camera.position[0] = 1.0f;
	engine::Light_Data light{};
	light.intensity = 0.0f;
	light.ambient = 0.0f;
	light.specular_intensity = 1.0f;
	light.specular_shininess = 16.0f;
	light.point_count = 1;
	light.point_position[0][2] = 2.5f;
	light.point_radius[0] = 5.0f;
	light.point_intensity[0] = 0.4f;
	light.point_attenuation[0] = 1;
	light.point_specular[0] = 1.0f;
	//低光泽度
	light.point_shininess[0] = 2.0f;
	engine::Fog_Data fog{};
	renderer.scene_apply(camera, light, fog);
	uint32_t low_shininess = render_brightness();
	//高光泽度
	light.point_shininess[0] = 64.0f;
	renderer.scene_apply(camera, light, fog);
	uint32_t high_shininess = render_brightness();
	//非法光泽度（0.5）整次拒绝，既有光源保持不变
	const float white[3] = { 1.0f, 1.0f, 1.0f };
	const float light_position[3] = { 0.0f, 0.0f, 2.5f };
	EXPECT_FALSE(renderer.light_add_point(light_position, white, 5.0f, 0.4f, 0.0f, 1, 1.0f, 0.5f));
	uint32_t after_invalid = render_brightness();

	RecordProperty("low_shininess", low_shininess);
	RecordProperty("high_shininess", high_shininess);
	RecordProperty("after_invalid_shininess", after_invalid);

	EXPECT_GT(low_shininess, high_shininess + 20u) << "逐盏光泽度未生效（"
		<< low_shininess << " vs " << high_shininess << "）";
	EXPECT_NEAR(after_invalid, high_shininess, 4u) << "非法光泽度改变了画面（"
		<< after_invalid << " vs " << high_shininess << "）";

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

/*
补光可独立生效（第四十七轮）
口径：四边形法线朝 +Z；关闭主方向光与环境光，补光方向 (0,0,-1)、强度 0.5：
      补光关闭（强度 0）时中心近黑；开启后中心亮度约 0.5；非法强度（负值）整次拒绝、画面不变。
*/
TEST_F(Renderer_Scene_Test, 补光可独立生效)
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

	//渲染一帧并返回中心亮度（lambda 内用 EXPECT：ASSERT 不允许出现在有返回值的函数中）
	auto render_brightness = [&]()
	{
		EXPECT_TRUE(renderer.mesh_draw(mesh, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return center_brightness(pixels);
	};

	//关闭主方向光与环境光
	const float light_direction[3] = { 0.0f, 0.0f, -1.0f };
	const float white[3] = { 1.0f, 1.0f, 1.0f };
	renderer.light_set(light_direction, white, 0.0f, 0.0f);
	//补光关闭
	renderer.light_fill_set(light_direction, white, 0.0f);
	uint32_t off = render_brightness();
	//补光开启
	renderer.light_fill_set(light_direction, white, 0.5f);
	uint32_t on = render_brightness();
	//非法强度整次拒绝（保持既有 0.5）
	renderer.light_fill_set(light_direction, white, -1.0f);
	uint32_t invalid = render_brightness();

	RecordProperty("fill_off", off);
	RecordProperty("fill_on", on);
	RecordProperty("fill_invalid", invalid);

	EXPECT_LT(off, 20u) << "补光关闭时仍有照明（" << off << "）";
	EXPECT_GE(on, 100u) << "补光未照亮正面（" << on << "）";
	EXPECT_NEAR(invalid, on, 4u) << "非法补光强度改变了画面（" << invalid << " vs " << on << "）";

        renderer.mesh_unload(mesh);
        renderer.texture_unload(texture);
}

/*
PBR-lite 镜面开关与粗糙度（第六十四轮）
口径：中灰纹理、法线 +Z、方向光沿 -Z（与视线同向）、环境光 0、全局高光 1 / 光泽度 16：
      关闭 → Blinn-Phong 中心饱和（≥240）；开启（缩放 1）→ 中心下降到 100–230（GGX 能量重分配）；
      粗糙度缩放 4 → 中心再降（≥10）；非法缩放 9 整次忽略（保持 4）；关闭后画面与初始逐字节一致。
*/
TEST_F(Renderer_Scene_Test, PBR镜面开关与粗糙度)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_gray();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	renderer.texture_bind_set(texture);
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data_with_normal(0.0f, 0.0f, 1.0f)));

	//方向光沿 -Z（to_light 与视线 +Z 对齐）、环境光 0；全局高光 1、光泽度 16
	const float forward[3] = { 0.0f, 0.0f, -1.0f };
	const float white_light[3] = { 1.0f, 1.0f, 1.0f };
	renderer.light_set(forward, white_light, 1.0f, 0.0f);
	renderer.specular_set(1.0f, 16.0f);

	//渲染一帧并返回像素
	auto render_frame = [&]()
	{
		EXPECT_TRUE(renderer.mesh_draw(mesh, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return pixels;
	};

	//关闭：Blinn-Phong 中心饱和
	std::vector<uint8_t> legacy_pixels = render_frame();
	uint32_t legacy = center_brightness(legacy_pixels);
	//开启：GGX
	renderer.pbr_enable_set(true);
	ASSERT_TRUE(renderer.pbr_enabled());
	uint32_t smooth = center_brightness(render_frame());
	//粗糙度缩放 4：高光更散
	renderer.pbr_roughness_set(4.0f);
	uint32_t rough = center_brightness(render_frame());
	//非法缩放 9 整次忽略（保持 4）
	renderer.pbr_roughness_set(9.0f);
	EXPECT_FLOAT_EQ(renderer.pbr_roughness(), 4.0f);
	//关闭：恢复逐字节一致
	renderer.pbr_enable_set(false);
	ASSERT_FALSE(renderer.pbr_enabled());
	std::vector<uint8_t> restored_pixels = render_frame();

	RecordProperty("pbr_legacy", legacy);
	RecordProperty("pbr_smooth", smooth);
	RecordProperty("pbr_rough", rough);
	EXPECT_GE(legacy, 240u) << "关闭 PBR 时中心未饱和（" << legacy << "）";
	EXPECT_GE(smooth, 100u) << "开启 PBR 后中心过暗（" << smooth << "）";
	EXPECT_LE(smooth, 230u) << "开启 PBR 后中心仍饱和（" << smooth << "）";
	EXPECT_LT(rough + 10u, smooth) << "粗糙度放大未削弱中心高光（" << rough << " vs " << smooth << "）";
	EXPECT_EQ(restored_pixels, legacy_pixels) << "关闭 PBR 后画面未恢复";

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

/*
PBR 金属度压暗漫反射（第六十五轮）
口径：中灰纹理、法线 +Z、方向光沿 -Z、环境光 0、**高光强度 0**（只看漫反射）：
      PBR 关闭 → 中心约 128（sRGB 采样）；开启且金属度 0 → 不变；0.5 → 约 92；1 → 近 0；
      非法金属度（-0.1 / 1.5）整次忽略；PBR 关闭时金属度不生效（仍约 128）。
*/
TEST_F(Renderer_Scene_Test, PBR金属度压暗漫反射)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_gray();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	renderer.texture_bind_set(texture);
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data_with_normal(0.0f, 0.0f, 1.0f)));

	//方向光沿 -Z、环境光 0；关闭高光
	const float forward[3] = { 0.0f, 0.0f, -1.0f };
	const float white_light[3] = { 1.0f, 1.0f, 1.0f };
	renderer.light_set(forward, white_light, 1.0f, 0.0f);
	renderer.specular_set(0.0f, 64.0f);

	//渲染一帧并返回中心亮度
	auto render_brightness = [&]()
	{
		EXPECT_TRUE(renderer.mesh_draw(mesh, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return center_brightness(pixels);
	};

	//PBR 关闭（基线）
	uint32_t legacy = render_brightness();
	//PBR 开启、金属度 0：应保持基线
	renderer.pbr_enable_set(true);
	uint32_t metallic_zero = render_brightness();
	//金属度 0.5 / 1：漫反射按 (1 − metallic) 压暗
	renderer.pbr_metallic_set(0.5f);
	uint32_t metallic_half = render_brightness();
	renderer.pbr_metallic_set(1.0f);
	uint32_t metallic_full = render_brightness();
	//非法金属度整次忽略
	renderer.pbr_metallic_set(-0.1f);
	EXPECT_FLOAT_EQ(renderer.pbr_metallic(), 1.0f);
	renderer.pbr_metallic_set(1.5f);
	EXPECT_FLOAT_EQ(renderer.pbr_metallic(), 1.0f);
	//PBR 关闭：金属度不生效
	renderer.pbr_enable_set(false);
	uint32_t restored = render_brightness();

	RecordProperty("metallic_legacy", legacy);
	RecordProperty("metallic_zero", metallic_zero);
	RecordProperty("metallic_half", metallic_half);
	RecordProperty("metallic_full", metallic_full);
	EXPECT_NEAR(metallic_zero, legacy, 3u) << "金属度 0 改变了漫反射";
	EXPECT_LT(metallic_half + 20u, metallic_zero) << "金属度 0.5 未压暗漫反射（" << metallic_half << "）";
	EXPECT_LE(metallic_full, 15u) << "金属度 1 未消除漫反射（" << metallic_full << "）";
	EXPECT_NEAR(restored, legacy, 3u) << "PBR 关闭时金属度不应生效";

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

/*
逐物体材质覆盖全局 PBR（第六十八轮）
口径：中灰纹理、法线 +Z、方向光沿 -Z、环境光 0、全局 PBR 开启（粗糙度 1、金属度 0）：
      ① 全局高光 1 / 光泽度 16 → 基线中心 100–230（GGX）；
      ② material_pbr_set(4, 0) → 中心再降（≥10），证逐物体粗糙度生效；
      ③ 非法参数（粗糙度 9 / 金属度 1.5）整次拒绝且状态不变；
      ④ material_pbr_clear → 画面与基线逐字节一致；
      ⑤ 关高光后 material_pbr_set(1, 1) → 中心近 0（逐物体金属度压暗漫反射）。
*/
TEST_F(Renderer_Scene_Test, 逐物体材质覆盖全局PBR)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_gray();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	renderer.texture_bind_set(texture);
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data_with_normal(0.0f, 0.0f, 1.0f)));

	//方向光沿 -Z、环境光 0；全局高光 1、光泽度 16；PBR 开启
	const float forward[3] = { 0.0f, 0.0f, -1.0f };
	const float white_light[3] = { 1.0f, 1.0f, 1.0f };
	renderer.light_set(forward, white_light, 1.0f, 0.0f);
	renderer.specular_set(1.0f, 16.0f);
	renderer.pbr_enable_set(true);

	//渲染一帧并返回像素
	auto render_frame = [&]()
	{
		EXPECT_TRUE(renderer.mesh_draw(mesh, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return pixels;
	};

	//基线（未启用逐物体材质）
	std::vector<uint8_t> baseline_pixels = render_frame();
	uint32_t baseline = center_brightness(baseline_pixels);
	//逐物体粗糙度 4：高光更散
	ASSERT_TRUE(renderer.material_pbr_set(4.0f, 0.0f));
	EXPECT_TRUE(renderer.material_pbr_active());
	uint32_t rough = center_brightness(render_frame());
	//非法参数整次拒绝（保持 4 / 0）
	EXPECT_FALSE(renderer.material_pbr_set(9.0f, 0.0f));
	EXPECT_FLOAT_EQ(renderer.material_pbr_roughness(), 4.0f);
	EXPECT_FALSE(renderer.material_pbr_set(1.0f, 1.5f));
	EXPECT_FLOAT_EQ(renderer.material_pbr_metallic(), 0.0f);
	//清除：回落全局参数，画面逐字节恢复
	renderer.material_pbr_clear();
	EXPECT_FALSE(renderer.material_pbr_active());
	std::vector<uint8_t> restored_pixels = render_frame();

	//关闭高光后只看漫反射：逐物体金属度 1 应消除漫反射
	renderer.specular_set(0.0f, 16.0f);
	uint32_t diffuse_base = center_brightness(render_frame());
	ASSERT_TRUE(renderer.material_pbr_set(1.0f, 1.0f));
	uint32_t metallic = center_brightness(render_frame());

	RecordProperty("object_baseline", baseline);
	RecordProperty("object_rough", rough);
	RecordProperty("object_metallic", metallic);
	EXPECT_GE(baseline, 100u) << "PBR 基线中心过暗（" << baseline << "）";
	EXPECT_LE(baseline, 230u) << "PBR 基线中心仍饱和（" << baseline << "）";
	EXPECT_LT(rough + 10u, baseline) << "逐物体粗糙度未削弱中心高光（" << rough << " vs " << baseline << "）";
	EXPECT_EQ(restored_pixels, baseline_pixels) << "清除逐物体材质后画面未恢复";
	EXPECT_LT(metallic + 20u, diffuse_base) << "逐物体金属度未压暗漫反射（" << metallic << " vs " << diffuse_base << "）";
	EXPECT_LE(metallic, 15u) << "逐物体金属度 1 未消除漫反射（" << metallic << "）";

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

/*
逐物体反照率色调（第六十九轮）
口径：中灰纹理、法线 +Z、方向光沿 -Z、环境光 0、高光关闭：
      ① material_tint_set(1, 0, 0, 1) → 中心绿通道显著下降，外通道（R / B 随格式）仍较高；
      ② 显式 mesh_draw 颜色（白色）优先于吸附色调 → 与基线逐字节一致；
      ③ 非法分量（-0.1 / 1.5）整次拒绝且状态不变；
      ④ material_tint_clear → 画面与基线逐字节一致。
*/
TEST_F(Renderer_Scene_Test, 逐物体色调着色)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_gray();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	renderer.texture_bind_set(texture);
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data_with_normal(0.0f, 0.0f, 1.0f)));

	//方向光沿 -Z、环境光 0；关闭高光只看漫反射
	const float forward[3] = { 0.0f, 0.0f, -1.0f };
	const float white_light[3] = { 1.0f, 1.0f, 1.0f };
	renderer.light_set(forward, white_light, 1.0f, 0.0f);
	renderer.specular_set(0.0f, 16.0f);

	//渲染一帧并返回像素（可选显式颜色）
	auto render_frame = [&](const float* color)
	{
		EXPECT_TRUE(renderer.mesh_draw(mesh, nullptr, color));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return pixels;
	};

	//基线（未设置色调）
	std::vector<uint8_t> baseline_pixels = render_frame(nullptr);
	std::array<uint8_t, 4> baseline = pixel_get(baseline_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	//红色色调
	ASSERT_TRUE(renderer.material_tint_set(1.0f, 0.0f, 0.0f, 1.0f));
	EXPECT_TRUE(renderer.material_tint_active());
	std::vector<uint8_t> tinted_pixels = render_frame(nullptr);
	std::array<uint8_t, 4> tinted = pixel_get(tinted_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	//显式白色颜色应覆盖吸附色调
	const float white_color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	std::vector<uint8_t> explicit_pixels = render_frame(white_color);
	//非法分量整次拒绝（状态保持红）
	EXPECT_FALSE(renderer.material_tint_set(-0.1f, 0.0f, 0.0f, 1.0f));
	EXPECT_FALSE(renderer.material_tint_set(1.0f, 1.5f, 0.0f, 1.0f));
	float state[4] = {};
	renderer.material_tint_get(state);
	EXPECT_FLOAT_EQ(state[0], 1.0f);
	EXPECT_FLOAT_EQ(state[1], 0.0f);
	//清除后逐字节恢复
	renderer.material_tint_clear();
	EXPECT_FALSE(renderer.material_tint_active());
	std::vector<uint8_t> restored_pixels = render_frame(nullptr);

	//外通道（R / B 随表面格式互换，取较大者）
	uint8_t tinted_outer = tinted[0] > tinted[2] ? tinted[0] : tinted[2];
	RecordProperty("tint_baseline_green", static_cast<int>(baseline[1]));
	RecordProperty("tint_tinted_green", static_cast<int>(tinted[1]));
	EXPECT_GT(baseline[1], 60u) << "基线中心过暗：" << pixel_text(baseline);
	EXPECT_LT(tinted[1] + 40u, baseline[1]) << "色调未压暗绿通道：" << pixel_text(tinted);
	EXPECT_GT(tinted_outer, 80u) << "色调未保留红色通道：" << pixel_text(tinted);
	EXPECT_EQ(explicit_pixels, baseline_pixels) << "显式颜色未覆盖吸附色调";
	EXPECT_EQ(restored_pixels, baseline_pixels) << "清除色调后画面未恢复";

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

/*
逐物体自发光（第七十轮）
口径：中灰纹理、法线 +Z、**方向光强度 0、环境光 0、高光 0**（全黑基线）：
      ① 基线中心近 0；② material_emissive_set(0.5) 后中心明显提亮（≥60）；
      ③ 非法值（-1 / 9）整次拒绝且状态不变；④ material_emissive_clear 后逐字节还原。
*/
TEST_F(Renderer_Scene_Test, 逐物体自发光提亮)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_gray();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	renderer.texture_bind_set(texture);
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data_with_normal(0.0f, 0.0f, 1.0f)));

	//全黑场景：方向光强度 0、环境光 0、高光 0
	const float forward[3] = { 0.0f, 0.0f, -1.0f };
	const float white_light[3] = { 1.0f, 1.0f, 1.0f };
	renderer.light_set(forward, white_light, 0.0f, 0.0f);
	renderer.specular_set(0.0f, 16.0f);

	//渲染一帧并返回像素
	auto render_frame = [&]()
	{
		EXPECT_TRUE(renderer.mesh_draw(mesh, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return pixels;
	};

	//基线（无自发光）
	std::vector<uint8_t> baseline_pixels = render_frame();
	uint32_t baseline = center_brightness(baseline_pixels);
	//自发光 0.5
	ASSERT_TRUE(renderer.material_emissive_set(0.5f));
	EXPECT_FLOAT_EQ(renderer.material_emissive(), 0.5f);
	uint32_t emissive = center_brightness(render_frame());
	//非法值整次拒绝（保持 0.5）
	EXPECT_FALSE(renderer.material_emissive_set(-1.0f));
	EXPECT_FALSE(renderer.material_emissive_set(9.0f));
	EXPECT_FLOAT_EQ(renderer.material_emissive(), 0.5f);
	//清除后逐字节恢复
	renderer.material_emissive_clear();
	EXPECT_FLOAT_EQ(renderer.material_emissive(), 0.0f);
	std::vector<uint8_t> restored_pixels = render_frame();

	RecordProperty("emissive_baseline", baseline);
	RecordProperty("emissive_lit", emissive);
	EXPECT_LE(baseline, 20u) << "全黑基线未被压暗（" << baseline << "）";
	EXPECT_GE(emissive, 60u) << "自发光未提亮中心（" << emissive << "）";
	EXPECT_EQ(restored_pixels, baseline_pixels) << "清除自发光后画面未恢复";

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}