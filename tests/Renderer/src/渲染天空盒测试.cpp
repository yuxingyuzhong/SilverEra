/*
渲染天空盒 / 简易 IBL 测试（第一百二十五轮）
口径：
  ① 关闭天空盒时画面与基线逐字节一致（启用后再关闭同样恢复原样）；
  ② 启用蓝色天空盒后，未被几何覆盖的背景像素变蓝，几何覆盖处不受影响；
  ③ 天空盒漫反射强度增大时物体被环境补光提亮，强度归零并关闭后恢复基线。
*/
#include "渲染场景测试辅助.h"

//构造六面同色的立方体贴图数据（RGBA8，顺序 +X / -X / +Y / -Y / +Z / -Z）
std::vector<uint8_t> solid_cube_faces(uint8_t red, uint8_t green, uint8_t blue)
{
	std::vector<uint8_t> faces(6 * 4);
	for (std::size_t index = 0; index < faces.size(); index += 4)
	{
		faces[index] = red;
		faces[index + 1] = green;
		faces[index + 2] = blue;
		faces[index + 3] = 255;
	}
	return faces;
}

//背景像素（左上角 1,1；被清屏色或天空盒覆盖，几何不触及）
std::array<uint8_t, 4> background_pixel(const std::vector<uint8_t>& pixels)
{
	return pixel_get(pixels, 1, 1);
}

//单通道亮度（RGB 三通道均值）
uint32_t brightness_of(const std::array<uint8_t, 4>& value)
{
	return (static_cast<uint32_t>(value[0]) + value[1] + value[2]) / 3;
}

/*
天空盒关闭时与基线逐字节一致（第一百二十五轮）
流程：基线帧 → 载入蓝色立方体并启用 → 背景变蓝（证明启用生效）→ 关闭并再次渲染，
     该帧须与基线逐字节完全一致（含几何与背景）。
*/
TEST_F(Renderer_Scene_Test, 天空盒关闭时画面不变)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	engine::Mesh_Handle quad = 0;
	ASSERT_TRUE(renderer.mesh_load(quad, quad_data_with_normal(0.0f, 0.0f, 1.0f)));

	//场景：单方向光满照（正交方向光默认朝 -Z），环境光 0
	engine::Camera_Data camera{};
	engine::Light_Data light{};
	light.intensity = 1.0f;
	light.ambient = 0.0f;
	engine::Fog_Data fog{};
	renderer.scene_apply(camera, light, fog);

	//渲染一帧并返回整帧像素
	auto render_frame = [&]()
	{
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return pixels;
	};

	//基线帧（天空盒默认关闭）
	ASSERT_TRUE(renderer.mesh_draw(quad, nullptr));
	std::vector<uint8_t> baseline = render_frame();

	//载入蓝色立方体并启用天空盒
	uint64_t cube = 0;
	std::vector<uint8_t> faces = solid_cube_faces(30, 120, 255);
	ASSERT_TRUE(renderer.texture_cube_load(cube, faces.data(), 1, 1));
	ASSERT_NE(cube, 0u);
	ASSERT_EQ(renderer.sky_texture(), 0u) << "天空盒贴图默认应为内置立方体";
	renderer.sky_texture_set(cube);
	EXPECT_EQ(renderer.sky_texture(), cube);
	renderer.sky_enable_set(true);
	ASSERT_TRUE(renderer.sky_enabled());
	ASSERT_TRUE(renderer.mesh_draw(quad, nullptr));
	std::vector<uint8_t> enabled = render_frame();

	//关闭天空盒
	renderer.sky_enable_set(false);
	ASSERT_FALSE(renderer.sky_enabled());
	ASSERT_TRUE(renderer.mesh_draw(quad, nullptr));
	std::vector<uint8_t> restored = render_frame();

	const uint32_t baseline_background = brightness_of(background_pixel(baseline));
	const uint32_t enabled_background = brightness_of(background_pixel(enabled));
	RecordProperty("sky_baseline_background", static_cast<int>(baseline_background));
	RecordProperty("sky_enabled_background", static_cast<int>(enabled_background));

	EXPECT_GT(enabled_background, baseline_background + 10u) << "启用天空盒后背景未变化（启用未生效）";
	EXPECT_EQ(restored, baseline) << "关闭天空盒后画面未与基线逐字节一致";

	renderer.mesh_unload(quad);
	renderer.texture_unload(cube);
}

/*
启用蓝色天空盒后背景变蓝（第一百二十五轮）
背景像素须出现明显蓝倾向（绿通道抬高以区分纯红），几何覆盖的中心像素保持不变。
*/
TEST_F(Renderer_Scene_Test, 天空盒绘制背景)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	engine::Mesh_Handle quad = 0;
	ASSERT_TRUE(renderer.mesh_load(quad, quad_data_with_normal(0.0f, 0.0f, 1.0f)));

	engine::Camera_Data camera{};
	engine::Light_Data light{};
	light.intensity = 1.0f;
	light.ambient = 0.0f;
	engine::Fog_Data fog{};
	renderer.scene_apply(camera, light, fog);

	auto render_frame = [&]()
	{
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return pixels;
	};

	//基线帧
	ASSERT_TRUE(renderer.mesh_draw(quad, nullptr));
	std::vector<uint8_t> baseline = render_frame();

	//启用蓝色天空盒（面为 0/200/255：绿通道可区分于纯红背景）
	uint64_t cube = 0;
	std::vector<uint8_t> faces = solid_cube_faces(0, 200, 255);
	ASSERT_TRUE(renderer.texture_cube_load(cube, faces.data(), 1, 1));
	renderer.sky_texture_set(cube);
	renderer.sky_enable_set(true);
	ASSERT_TRUE(renderer.mesh_draw(quad, nullptr));
	std::vector<uint8_t> sky_pixels = render_frame();

	const std::array<uint8_t, 4> baseline_background = background_pixel(baseline);
	const std::array<uint8_t, 4> sky_background = background_pixel(sky_pixels);
	const std::array<uint8_t, 4> baseline_center = pixel_get(baseline, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	const std::array<uint8_t, 4> sky_center = pixel_get(sky_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	RecordProperty("sky_background_text", pixel_text(sky_background));
	RecordProperty("sky_baseline_text", pixel_text(baseline_background));
	RecordProperty("sky_center_text", pixel_text(sky_center));

	//背景：绿色通道较基线明显抬高（蓝色天空盒的面为 0/200/255；纯红面不会抬高绿通道）
	EXPECT_GE(static_cast<int>(sky_background[1]), static_cast<int>(baseline_background[1]) + 80)
		<< "背景未呈现蓝色天空：" << pixel_text(sky_background);
	EXPECT_LE(static_cast<int>(sky_background[1]), 250) << "背景绿通道异常饱和：" << pixel_text(sky_background);
	//几何覆盖处：天空项被深度测试挡住，中心像素不变
	EXPECT_EQ(sky_center, baseline_center) << "天空盒覆盖了场景几何：" << pixel_text(sky_center);

	renderer.mesh_unload(quad);
	renderer.texture_unload(cube);
}

/*
天空盒漫反射补光使物体提亮（第一百二十五轮）
场景无方向光、环境光 0 时物体本应为黑；启用白色天空盒并按强度 0.25 → 0.5 递增，
中心亮度须逐档上升；关闭后恢复基线（同时验证 sky_params_get 的读回与负值钳制）。
*/
TEST_F(Renderer_Scene_Test, 天空盒环境补光)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	engine::Mesh_Handle quad = 0;
	ASSERT_TRUE(renderer.mesh_load(quad, quad_data_with_normal(0.0f, 0.0f, 1.0f)));

	//场景：无方向光、环境光 0
	engine::Camera_Data camera{};
	engine::Light_Data light{};
	light.intensity = 0.0f;
	light.ambient = 0.0f;
	engine::Fog_Data fog{};
	renderer.scene_apply(camera, light, fog);

	auto render_frame = [&]()
	{
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return pixels;
	};

	//基线帧（应为黑）
	ASSERT_TRUE(renderer.mesh_draw(quad, nullptr));
	std::vector<uint8_t> baseline = render_frame();
	const uint32_t baseline_center = center_brightness(baseline);

	//白色天空盒（六面 255）
	uint64_t cube = 0;
	std::vector<uint8_t> faces = solid_cube_faces(255, 255, 255);
	ASSERT_TRUE(renderer.texture_cube_load(cube, faces.data(), 1, 1));
	renderer.sky_texture_set(cube);
	renderer.sky_enable_set(true);

	//强度 0.25
	renderer.sky_params_set(0.25f, 0.0f);
	ASSERT_TRUE(renderer.mesh_draw(quad, nullptr));
	const uint32_t dim_center = center_brightness(render_frame());
	//强度 0.5
	renderer.sky_params_set(0.5f, 0.0f);
	ASSERT_TRUE(renderer.mesh_draw(quad, nullptr));
	const uint32_t bright_center = center_brightness(render_frame());

	//参数读回与负值钳制
	float ambient = -1.0f;
	float specular = -1.0f;
	renderer.sky_params_get(&ambient, &specular);
	EXPECT_FLOAT_EQ(ambient, 0.5f) << "漫反射强度读回不一致";
	EXPECT_FLOAT_EQ(specular, 0.0f) << "镜面强度读回不一致";
	renderer.sky_params_set(-1.0f, -2.0f);
	renderer.sky_params_get(&ambient, &specular);
	EXPECT_FLOAT_EQ(ambient, 0.0f) << "负漫反射强度未被钳制";
	EXPECT_FLOAT_EQ(specular, 0.0f) << "负镜面强度未被钳制";

	//关闭天空盒后恢复基线
	renderer.sky_enable_set(false);
	ASSERT_TRUE(renderer.mesh_draw(quad, nullptr));
	const uint32_t restored_center = center_brightness(render_frame());

	RecordProperty("sky_ibl_baseline", static_cast<int>(baseline_center));
	RecordProperty("sky_ibl_dim", static_cast<int>(dim_center));
	RecordProperty("sky_ibl_bright", static_cast<int>(bright_center));
	RecordProperty("sky_ibl_restored", static_cast<int>(restored_center));

	EXPECT_LE(baseline_center, 4u) << "基线未处于无光状态（用例前提不成立）";
	EXPECT_GT(dim_center, baseline_center + 10u) << "天空盒漫反射补光未提亮物体";
	EXPECT_GT(bright_center, dim_center + 10u) << "提高漫反射强度未继续提亮";
	EXPECT_NEAR(restored_center, baseline_center, 3u) << "关闭天空盒后中心亮度未恢复";

	renderer.mesh_unload(quad);
	renderer.texture_unload(cube);
}
