/*
渲染场景数据测试（L2 隐藏窗口 + 像素回读）：场景描述与清屏色、三组入参、正交投影
（第五十七轮按功能域拆分：光照用例移至「渲染光照测试.cpp」、雾 / 环境光 / 高光与文字可见性移至
「渲染场景效果测试.cpp」、阴影用例移至「渲染阴影测试.cpp」；本文件逻辑未改）
*/
#include "渲染场景测试辅助.h"

#include <numeric>

TEST_F(Renderer_Scene_Test, 未初始化应用场景无副作用)
{
	engine::Renderer renderer;
	engine::Scene_Data scene{};
	renderer.scene_apply(scene);
	const float color[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
	renderer.clear_color_set(color);
	//设置不改变状态、不崩溃
	EXPECT_FALSE(renderer.valid());
}

//scene_apply 的相机与清屏色同时生效：相机背对四边形后中心应显示清屏色
TEST_F(Renderer_Scene_Test, 应用场景描述相机与清屏色同时生效)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//若本机不支持像素回读则跳过
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//纯红纹理 ＋ 铺满全屏四边形
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_red();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data()));
	renderer.texture_bind_set(texture);
	//第一帧：默认相机（机位 +Z 3、注视原点）→ 四边形可见，中心为纹理红（绿通道低）
	ASSERT_TRUE(renderer.mesh_draw(mesh, full_screen_matrix().data()));
	std::vector<uint8_t> before(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(before.data(), static_cast<uint32_t>(before.size())));
	std::array<uint8_t, 4> center_before = pixel_get(before, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	EXPECT_LT(center_before[1], 60) << "默认相机下中心非纹理红：" << pixel_text(center_before);
	EXPECT_GE(outer_max(center_before), 200) << "默认相机下中心外通道未饱和：" << pixel_text(center_before);

	//第二帧：scene_apply 令相机背对四边形（注视点在机位前方 +Z）并把清屏色设为纯绿
	engine::Scene_Data scene{};
	scene.position[0] = 0.0f;
	scene.position[1] = 0.0f;
	scene.position[2] = 3.0f;
	scene.target[0] = 0.0f;
	scene.target[1] = 0.0f;
	scene.target[2] = 6.0f;
	scene.clear_color[0] = 0.0f;
	scene.clear_color[1] = 1.0f;
	scene.clear_color[2] = 0.0f;
	scene.clear_color[3] = 1.0f;
	renderer.scene_apply(scene);
	ASSERT_TRUE(renderer.mesh_draw(mesh, full_screen_matrix().data()));
	std::vector<uint8_t> after(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(after.data(), static_cast<uint32_t>(after.size())));
	std::array<uint8_t, 4> center_after = pixel_get(after, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	//四边形已被相机移出视野 → 中心应为清屏纯绿
	EXPECT_GE(center_after[1], 200) << "相机未被应用（中心仍见四边形）：" << pixel_text(center_after);
	EXPECT_LT(center_after[0], 60) << "清屏色未被应用（绿通道旁分量偏高）：" << pixel_text(center_after);
	EXPECT_LT(center_after[2], 60) << "清屏色未被应用（绿通道旁分量偏高）：" << pixel_text(center_after);

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

//clear_color_set 切换清屏色生效：由绿改为红后中心变为饱和外色且非绿
TEST_F(Renderer_Scene_Test, 切换清屏色生效)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//若本机不支持像素回读则跳过
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//先设纯绿清屏色并回读
	const float green[4] = { 0.0f, 1.0f, 0.0f, 1.0f };
	renderer.clear_color_set(green);
	std::vector<uint8_t> green_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(green_pixels.data(), static_cast<uint32_t>(green_pixels.size())));
	std::array<uint8_t, 4> green_center = pixel_get(green_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	EXPECT_GE(green_center[1], 200) << "绿色清屏色未生效：" << pixel_text(green_center);
	//再设纯红清屏色并回读（绿通道应降低、外通道应饱和）
	const float red[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
	renderer.clear_color_set(red);
	std::vector<uint8_t> red_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(red_pixels.data(), static_cast<uint32_t>(red_pixels.size())));
	std::array<uint8_t, 4> red_center = pixel_get(red_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	EXPECT_LT(red_center[1], 60) << "红色清屏色未生效（绿通道仍高）：" << pixel_text(red_center);
	EXPECT_GE(outer_max(red_center), 200) << "红色清屏色未生效（外通道未饱和）：" << pixel_text(red_center);
	//两次清屏色不同 → 画面应可区分
	EXPECT_NE(green_pixels, red_pixels);
}

//非法场景描述整次拒绝：不改变既有相机与清屏色
TEST_F(Renderer_Scene_Test, 非法场景描述整次拒绝)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//若本机不支持像素回读则跳过
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//第一帧：应用合法场景（清屏色绿、相机背对四边形）→ 中心为纯绿
	engine::Scene_Data valid{};
	valid.clear_color[0] = 0.0f;
	valid.clear_color[1] = 1.0f;
	valid.clear_color[2] = 0.0f;
	valid.clear_color[3] = 1.0f;
	renderer.scene_apply(valid);
	std::vector<uint8_t> green_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(green_pixels.data(), static_cast<uint32_t>(green_pixels.size())));
	std::array<uint8_t, 4> green_center = pixel_get(green_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	EXPECT_GE(green_center[1], 200) << "合法场景未生效：" << pixel_text(green_center);

	//第二帧：应用非法场景（视场角为零、清屏色红）→ 应被整次拒绝
	engine::Scene_Data invalid{};
	invalid.fov = 0.0f;
	invalid.clear_color[0] = 1.0f;
	invalid.clear_color[1] = 0.0f;
	invalid.clear_color[2] = 0.0f;
	invalid.clear_color[3] = 1.0f;
	renderer.scene_apply(invalid);
	std::vector<uint8_t> rejected_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(rejected_pixels.data(), static_cast<uint32_t>(rejected_pixels.size())));
	std::array<uint8_t, 4> rejected_center = pixel_get(rejected_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	//清屏色仍为绿（非法描述里的红未被采用）
	EXPECT_GE(rejected_center[1], 200) << "非法场景描述被部分采用（清屏色已变）：" << pixel_text(rejected_center);
	EXPECT_LT(rejected_center[0], 60) << "非法场景描述被部分采用（外通道偏高）：" << pixel_text(rejected_center);
	//画面与拒绝前完全一致
	EXPECT_EQ(green_pixels, rejected_pixels) << "非法场景描述改变了画面，整次拒绝未生效";
}

//清屏色分量越界被钳制到 [0,1]
TEST_F(Renderer_Scene_Test, 清屏色越界分量被钳制)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//若本机不支持像素回读则跳过
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//越界分量：外通道 2.0（应钳到 1）、绿 -1.0（应钳到 0）
	const float out_of_range[4] = { 2.0f, -1.0f, 0.5f, 1.0f };
	renderer.clear_color_set(out_of_range);
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	std::array<uint8_t, 4> center = pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	//外通道钳到 1 → 饱和；绿钳到 0 → 低
	EXPECT_GE(outer_max(center), 200) << "越界外通道未被钳到 1：" << pixel_text(center);
	EXPECT_LT(center[1], 60) << "负绿通道未被钳到 0：" << pixel_text(center);
}

//应用默认场景描述与原默认状态画面完全一致（默认值单一来源）
TEST_F(Renderer_Scene_Test, 默认场景描述与原默认画面一致)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//若本机不支持像素回读则跳过
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//纯红纹理 ＋ 铺满全屏四边形（画面随相机与清屏色变化，故对默认值敏感）
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_red();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data()));
	renderer.texture_bind_set(texture);
	//第一帧：不调用 scene_apply（使用本层既有默认）
	ASSERT_TRUE(renderer.mesh_draw(mesh, full_screen_matrix().data()));
	std::vector<uint8_t> baseline(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(baseline.data(), static_cast<std::uint32_t>(baseline.size())));
	//第二帧：应用默认构造的 Scene_Data（其默认值应与本层既有默认一致）
	engine::Scene_Data defaults{};
	renderer.scene_apply(defaults);
	ASSERT_TRUE(renderer.mesh_draw(mesh, full_screen_matrix().data()));
	std::vector<uint8_t> applied(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(applied.data(), static_cast<std::uint32_t>(applied.size())));
	//两帧画面应完全一致（默认值已收敛为单一来源）
	EXPECT_EQ(baseline, applied) << "应用默认 Scene_Data 后画面改变，默认值未收敛为单一来源";
	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

/*
方向光（第二十九轮）：面法线朝向光源的四边形应明显亮于背光四边形
口径：纯白纹理 ＋ 覆盖视口中央的四边形（单位矩阵）；同一光照参数下改用法线 (0,0,-1) 的网格，
      默认光沿 -Z 传播，故 (0,0,1) 满照、(0,0,-1) 照度为 0；只比较中心像素 RGB 均值。
*/

TEST_F(Renderer_Scene_Test, 正交投影下远近尺寸一致)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_red();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data()));
	renderer.texture_bind_set(texture);

	//红色像素计数（外通道饱和即视为四边形覆盖）
	auto red_count = [](const std::vector<uint8_t>& pixels)
	{
		uint32_t count = 0;
		for (uint32_t y = 0; y < WINDOW_HEIGHT; ++y)
			for (uint32_t x = 0; x < WINDOW_WIDTH; ++x)
				if (outer_max(pixel_get(pixels, x, y)) >= 200)
					++count;
		return count;
	};
	//按给定相机位置 / 正文模式渲染一帧并计数
	auto render_count = [&](float camera_z, bool ortho)
	{
		engine::Scene_Data scene{};
		scene.position[2] = camera_z;
		scene.ortho = ortho;
		scene.ortho_height = 2.0f;
		renderer.scene_apply(scene);
		renderer.mesh_draw(mesh, nullptr);
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return red_count(pixels);
	};

	//透视：近大远小
	uint32_t perspective_near = render_count(3.0f, false);
	uint32_t perspective_far = render_count(5.0f, false);
	RecordProperty("ortho_perspective_near", perspective_near);
	RecordProperty("ortho_perspective_far", perspective_far);
	EXPECT_GT(perspective_near, 0u) << "透视近距帧无四边形（用例前提不成立）";
	EXPECT_LT(perspective_far, perspective_near * 8 / 10) << "透视下远距未明显变小";

	//正交：远近一致
	uint32_t ortho_near = render_count(3.0f, true);
	uint32_t ortho_far = render_count(5.0f, true);
	RecordProperty("ortho_near", ortho_near);
	RecordProperty("ortho_far", ortho_far);
	EXPECT_GT(ortho_near, 0u) << "正交帧无四边形（用例前提不成立）";
	//允许 2% 的像素级误差
	uint32_t tolerance = ortho_near / 50 + 1;
	EXPECT_LE(std::abs(static_cast<int>(ortho_far) - static_cast<int>(ortho_near)), static_cast<int>(tolerance))
		<< "正交投影下远距画面占比改变（正交" << ortho_near << " / " << ortho_far << "）";

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

/*
立方体正面随光照方向明暗反转（第三十轮）
口径：带逐面法线的立方体 ＋ 默认相机（+Z 3）→ 中心像素落在 +Z 面；
      光沿 -Z 传播时该面满照，光沿 +Z 传播时该面照度为 0。
*/

TEST_F(Renderer_Scene_Test, 三组入参与扁平场景等价)
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

	//渲染一帧并返回像素（lambda 内用 EXPECT：ASSERT 不允许出现在有返回值的函数中）
	auto render_frame = [&]()
	{
		EXPECT_TRUE(renderer.mesh_draw(mesh, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return pixels;
	};

	//同一份场景的扁平描述
	engine::Scene_Data scene{};
	scene.clear_color[0] = 0.1f;
	scene.clear_color[1] = 0.2f;
	scene.clear_color[2] = 0.3f;
	scene.clear_color[3] = 1.0f;
	scene.light_ambient = 0.3f;
	scene.light_count = 1;
	scene.point_light_position[0][0] = 0.0f;
	scene.point_light_position[0][1] = 0.0f;
	scene.point_light_position[0][2] = 2.5f;
	scene.point_light_radius[0] = 2.0f;
	scene.point_light_intensity[0] = 1.0f;
	scene.point_light_attenuation[0] = 2;
	scene.specular_intensity = 0.4f;
	scene.specular_shininess = 32.0f;
	scene.attenuation_mode = 1;
	scene.fog_color[0] = 0.6f;
	scene.fog_color[1] = 0.1f;
	scene.fog_color[2] = 0.1f;
	scene.fog_density = 0.5f;
	scene.fog_start = 1.0f;
	scene.fog_end = 8.0f;
	scene.ambient_color[0] = 1.0f;
	scene.ambient_color[1] = 0.8f;
	scene.ambient_color[2] = 0.6f;
	//半球环境光（第四十七轮：地面色与混合强度须随三组等价传递）
	scene.ambient_ground[0] = 0.2f;
	scene.ambient_ground[1] = 0.1f;
	scene.ambient_ground[2] = 0.1f;
	scene.ambient_hemisphere = 1.0f;
	//补光（第四十七轮：方向 / 颜色 / 强度须随三组等价传递）
	scene.fill_direction[0] = 0.3f;
	scene.fill_direction[1] = 0.4f;
	scene.fill_direction[2] = -0.5f;
	scene.fill_color[0] = 0.5f;
	scene.fill_color[1] = 0.6f;
	scene.fill_color[2] = 0.7f;
	scene.fill_intensity = 0.4f;
	renderer.scene_apply(scene);
	std::vector<uint8_t> flat_pixels = render_frame();

	//等价的三组描述
	engine::Camera_Data camera{};
	for (std::size_t index = 0; index < 4; ++index)
		camera.clear_color[index] = scene.clear_color[index];
	engine::Light_Data light{};
	light.intensity = scene.light_intensity;
	light.ambient = scene.light_ambient;
	light.point_count = scene.light_count;
	for (std::size_t index = 0; index < 3; ++index)
	{
		light.point_position[0][index] = scene.point_light_position[0][index];
		light.point_color[0][index] = scene.point_light_color[0][index];
		light.ambient_color[index] = scene.ambient_color[index];
		light.ambient_ground[index] = scene.ambient_ground[index];
		light.fill_direction[index] = scene.fill_direction[index];
		light.fill_color[index] = scene.fill_color[index];
	}
	light.ambient_hemisphere = scene.ambient_hemisphere;
	light.fill_intensity = scene.fill_intensity;
	light.point_radius[0] = scene.point_light_radius[0];
	light.point_intensity[0] = scene.point_light_intensity[0];
	light.point_attenuation[0] = scene.point_light_attenuation[0];
	light.specular_intensity = scene.specular_intensity;
	light.specular_shininess = scene.specular_shininess;
	light.attenuation_mode = scene.attenuation_mode;
	engine::Fog_Data fog{};
	for (std::size_t index = 0; index < 3; ++index)
		fog.color[index] = scene.fog_color[index];
	fog.density = scene.fog_density;
	fog.start = scene.fog_start;
	fog.end = scene.fog_end;
	renderer.scene_apply(camera, light, fog);
	std::vector<uint8_t> group_pixels = render_frame();

	RecordProperty("flat_center", pixel_text(pixel_get(flat_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2)));
	EXPECT_EQ(group_pixels, flat_pixels) << "三组入参与扁平场景的画面不一致";
	//场景确有可见几何（排除「两帧都是纯清屏色」的空等价）
	EXPECT_GT(center_brightness(flat_pixels), 0u) << "等价性用例的两帧均为空白，未覆盖光照与雾";

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

/*
三组入参相机非法整次拒绝（第四十七轮）
口径：先提交「相机背对四边形 ＋ 绿色清屏色」的三组描述并渲染；
      再提交「视场角为 0 ＋ 红色清屏色」的三组描述（相机非法）→ 整次拒绝，
      画面应与上一帧逐字节一致（清屏色未被改红、相机未被改回）。
*/
TEST_F(Renderer_Scene_Test, 三组入参相机非法整次拒绝)
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

	//渲染一帧并返回像素（lambda 内用 EXPECT：ASSERT 不允许出现在有返回值的函数中）
	auto render_frame = [&]()
	{
		EXPECT_TRUE(renderer.mesh_draw(mesh, nullptr));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return pixels;
	};

	//合法三组：相机背对四边形 ＋ 绿色清屏色
	engine::Camera_Data camera{};
	camera.target[2] = 10.0f;
	camera.clear_color[0] = 0.0f;
	camera.clear_color[1] = 1.0f;
	camera.clear_color[2] = 0.0f;
	camera.clear_color[3] = 1.0f;
	engine::Light_Data light{};
	engine::Fog_Data fog{};
	renderer.scene_apply(camera, light, fog);
	std::vector<uint8_t> baseline = render_frame();

	//非法三组：视场角为 0 ＋ 红色清屏色
	engine::Camera_Data invalid = camera;
	invalid.fov = 0.0f;
	invalid.clear_color[0] = 1.0f;
	invalid.clear_color[1] = 0.0f;
	invalid.clear_color[2] = 0.0f;
	renderer.scene_apply(invalid, light, fog);
	std::vector<uint8_t> after = render_frame();

	RecordProperty("invalid_camera_center", pixel_text(pixel_get(after, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2)));
	EXPECT_EQ(after, baseline) << "非法相机未整次拒绝（清屏色或相机被部分应用）";
	EXPECT_GE(pixel_get(after, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2)[1], 200u) << "绿色清屏色丢失";

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

/*
半球环境光上下异色（第四十七轮）
口径：两块正对相机的四边形，法线分别朝上（+Y）与朝下（-Y）；关闭方向光、环境光强度 0.5、天顶色白、地面色黑。
      半球强度 1 时「朝上」应明显亮于「朝下」；强度 0 时两者一致（退化为单一环境光颜色）。
*/

/*
场景 UBO 按需写入（第六十轮；第一百二十一轮整块由 976 扩为 1056 字节）
口径：frame_overlap = 2；各帧槽首次使用各整块上传 1056 字节，之后输入不变则上传 0 字节；
     修改一个光照参数后单帧上传 >0 且 ≤1056（仅变化区间），随后一帧又回到 0。
*/
TEST_F(Renderer_Scene_Test, 场景UBO按需写入)
{
	engine::Renderer renderer;
	engine::Renderer_Config config = hidden_config();
	config.frame_overlap = 2;
	ASSERT_TRUE(renderer.init(config));

	//逐帧累计上传字节的增量
	std::vector<uint64_t> deltas;
	uint64_t previous = renderer.scene_upload_bytes();
	for (int frame = 0; frame < 4; ++frame)
	{
		ASSERT_TRUE(renderer.frame_render());
		uint64_t current = renderer.scene_upload_bytes();
		uint64_t delta = current - previous;
		//单帧上传不得超过整块大小（第一百二十一轮起 1056 字节）
		EXPECT_LE(delta, 1056u) << "第 " << frame + 1 << " 帧上传超出整块";
		deltas.push_back(delta);
		previous = current;
	}
	RecordProperty("scene_upload_frame1", static_cast<int>(deltas[0]));
	RecordProperty("scene_upload_frame2", static_cast<int>(deltas[1]));
	RecordProperty("scene_upload_frame3", static_cast<int>(deltas[2]));
	RecordProperty("scene_upload_frame4", static_cast<int>(deltas[3]));
	//首帧必整块写入
	EXPECT_EQ(deltas[0], 1056u) << "首帧未整块写入场景 UBO";
	//静止场景下应出现 0 字节上传（各帧槽预热完成后）
	EXPECT_EQ(*std::min_element(deltas.begin(), deltas.end()), 0u) << "静止场景仍整块上传";

	//修改方向光环境光 → 只有光照块变化，单帧上传应大于 0 且不超过整块
	const float forward[3] = { 0.0f, 0.0f, -1.0f };
	const float white[3] = { 1.0f, 1.0f, 1.0f };
	renderer.light_set(forward, white, 1.0f, 0.25f);
	//变化需按帧槽逐圈传播：连续若干帧后必须回到 0 字节
	uint64_t changed_total = 0;
	uint64_t last_delta = 0;
	for (int frame = 0; frame < 4; ++frame)
	{
		ASSERT_TRUE(renderer.frame_render());
		uint64_t current = renderer.scene_upload_bytes();
		last_delta = current - previous;
		previous = current;
		EXPECT_LE(last_delta, 1056u) << "变化后第 " << frame + 1 << " 帧上传超出整块";
		changed_total += last_delta;
	}
	RecordProperty("scene_upload_changed", static_cast<int>(changed_total));
	EXPECT_GT(changed_total, 0u) << "光照参数变化后未上传";
	//各帧槽均同步后应回到 0（按需写入生效）
	EXPECT_EQ(last_delta, 0u) << "静止场景仍上传（按需写入未生效）";
}

/*
实例缓冲按需写入（第六十一轮）
口径：frame_overlap = 2；每帧绘制同一个四边形（1 个实例 = 64 字节）；
     各帧槽预热后静止帧上传 0 字节；平移模型矩阵后逐帧上传 >0 且 ≤64，随后回到 0。
*/
TEST_F(Renderer_Scene_Test, 实例缓冲按需写入)
{
	engine::Renderer renderer;
	engine::Renderer_Config config = hidden_config();
	config.frame_overlap = 2;
	ASSERT_TRUE(renderer.init(config));
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data()));
	renderer.texture_bind_set(texture);

	//逐帧累计上传字节的增量（transform 为空表示单位矩阵）
	auto upload_delta = [&](std::vector<uint64_t>& deltas, uint64_t& previous, int frames, const float* transform)
	{
		for (int frame = 0; frame < frames; ++frame)
		{
			EXPECT_TRUE(renderer.mesh_draw(mesh, transform));
			EXPECT_TRUE(renderer.frame_render());
			uint64_t current = renderer.instance_upload_bytes();
			deltas.push_back(current - previous);
			previous = current;
		}
	};

	//静止场景：两个帧槽依次预热（各 80 字节：模型矩阵 16 ＋ 材质 4 浮点），之后为 0
	std::vector<uint64_t> deltas;
	uint64_t previous = renderer.instance_upload_bytes();
	upload_delta(deltas, previous, 4, nullptr);
	RecordProperty("instance_upload_frame1", static_cast<int>(deltas[0]));
	RecordProperty("instance_upload_frame3", static_cast<int>(deltas[2]));
	EXPECT_EQ(deltas[0], 80u) << "首帧未写入单个实例";
	EXPECT_EQ(*std::min_element(deltas.begin(), deltas.end()), 0u) << "静止场景仍整块上传";
	for (uint64_t delta : deltas)
		EXPECT_LE(delta, 80u) << "单帧上传超出本帧实例字节数";

	//平移模型矩阵 → 模型矩阵变化，应重新上传该实例
	std::array<float, 16> model = {};
	model[0] = 1.0f;
	model[5] = 1.0f;
	model[10] = 1.0f;
	model[12] = 0.3f;
	model[15] = 1.0f;
	std::vector<uint64_t> moved;
	upload_delta(moved, previous, 4, model.data());
	RecordProperty("instance_upload_moved_total",
		static_cast<int>(std::accumulate(moved.begin(), moved.end(), static_cast<uint64_t>(0))));
	EXPECT_GT(*std::max_element(moved.begin(), moved.end()), 0u) << "模型矩阵变化后未上传";
	EXPECT_EQ(moved.back(), 0u) << "静止后仍上传（按需写入未生效）";

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}