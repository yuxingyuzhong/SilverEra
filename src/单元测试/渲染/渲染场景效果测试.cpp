/*
渲染场景效果测试（雾 / 环境光颜色 / 半球环境光 / 逐光与逐盏高光 / 文字在效果场景中的可见性）
（第五十七轮由「渲染场景数据测试.cpp」按功能域拆分；逻辑未改）
*/
#include "渲染场景测试辅助.h"

TEST_F(Renderer_Scene_Test, 距离雾压暗远处物体)
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

	//无雾基线
	uint32_t plain = render_brightness();
	//开启黑雾（密度 1、区间 [1, 4]）
	const float black[3] = { 0.0f, 0.0f, 0.0f };
	renderer.fog_set(black, 1.0f, 1.0f, 4.0f);
	uint32_t fogged = render_brightness();
	//关闭雾
	renderer.fog_clear();
	uint32_t restored = render_brightness();
	RecordProperty("fog_plain", plain);
	RecordProperty("fog_fogged", fogged);
	RecordProperty("fog_restored", restored);
	EXPECT_GE(plain, 240u) << "无雾基线异常（亮度 " << plain << "）";
	EXPECT_LE(fogged, plain - 30u) << "雾未压暗画面（" << plain << " → " << fogged << "）";
	EXPECT_GE(fogged, 150u) << "雾压暗过度（亮度 " << fogged << "）";
	uint32_t gap = restored > plain ? restored - plain : plain - restored;
	EXPECT_LE(gap, 2u) << "关闭雾后亮度未恢复（" << plain << " vs " << restored << "）";

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

/*
非法雾参数被忽略（第三十八轮）
口径：负密度、结束不大于起始均为非法；两种调用后画面与调用前逐字节一致。
*/
TEST_F(Renderer_Scene_Test, 非法雾参数被忽略)
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

	std::vector<uint8_t> baseline = render_frame();
	const float black[3] = { 0.0f, 0.0f, 0.0f };
	//负密度 → 忽略
	renderer.fog_set(black, -0.5f, 1.0f, 4.0f);
	EXPECT_EQ(render_frame(), baseline) << "非法密度改变了画面";
	//结束不大于起始 → 忽略
	renderer.fog_set(black, 1.0f, 4.0f, 4.0f);
	EXPECT_EQ(render_frame(), baseline) << "非法区间改变了画面";

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

/*
场景描述携带距离雾（第三十九轮）
口径：白纹理 ＋ 法线 (0,0,1) 四边形、默认方向光满照；同一 Scene_Data 的雾密度由 0 改为 1（黑雾、区间 [1,4]）：
      中心亮度显著下降；恢复密度 0 后回到基线。
*/
TEST_F(Renderer_Scene_Test, 场景描述携带距离雾)
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

	//基线：默认场景（雾关闭）
	engine::Scene_Data scene{};
	renderer.scene_apply(scene);
	uint32_t plain = render_brightness();
	//开启黑雾（密度 1、区间 [1, 4]）
	scene.fog_color[0] = 0.0f;
	scene.fog_color[1] = 0.0f;
	scene.fog_color[2] = 0.0f;
	scene.fog_density = 1.0f;
	scene.fog_start = 1.0f;
	scene.fog_end = 4.0f;
	renderer.scene_apply(scene);
	uint32_t fogged = render_brightness();
	//恢复密度 0
	scene.fog_density = 0.0f;
	renderer.scene_apply(scene);
	uint32_t restored = render_brightness();
	RecordProperty("scene_fog_plain", plain);
	RecordProperty("scene_fog_fogged", fogged);
	RecordProperty("scene_fog_restored", restored);
	EXPECT_GE(plain, 240u) << "无雾基线异常（亮度 " << plain << "）";
	EXPECT_LE(fogged, plain - 30u) << "场景描述中的雾未生效（" << plain << " → " << fogged << "）";
	uint32_t gap = restored > plain ? restored - plain : plain - restored;
	EXPECT_LE(gap, 2u) << "恢复雾关闭后亮度未回归（" << plain << " vs " << restored << "）";

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

/*
场景描述中的非法雾参数被忽略（第三十九轮）
口径：默认场景回读基线；把 fog_density 写为 -1 再应用：门面整次忽略，画面逐字节一致。
*/
TEST_F(Renderer_Scene_Test, 场景描述非法雾参数被忽略)
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
	//非法雾密度（-1）应被整次忽略
	engine::Scene_Data invalid = baseline;
	invalid.fog_density = -1.0f;
	renderer.scene_apply(invalid);
	EXPECT_EQ(render_frame(), plain) << "场景描述中的非法雾参数改变了画面";

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

/*
环境光颜色着色（第四十轮）
口径：中灰纹理 ＋ 法线 (0,0,-1)（背光面）＋ 环境光强度 0.5：
      默认白色环境光下三通道接近相等；设为纯红后红通道保留、绿 / 蓝压到接近 0。
*/
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

TEST_F(Renderer_Scene_Test, 雾场景上的文字仍可见)
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
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data_with_normal(0.0f, 0.0f, 1.0f)));
	renderer.texture_bind_set(texture);

	//开启黑雾并叠白色文字
	const float black[3] = { 0.0f, 0.0f, 0.0f };
	renderer.fog_set(black, 1.0f, 1.0f, 4.0f);
	const float white_text[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	renderer.text_color_set(white_text);
	ASSERT_TRUE(renderer.mesh_draw(mesh, nullptr));
	ASSERT_TRUE(renderer.text_draw(font, "FOG", 4.0f, 4.0f));
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
	RecordProperty("fog_text_bright_pixels", bright_count);
	EXPECT_GT(bright_count, 10u) << "雾场景上的文字不可见（亮像素 " << bright_count << "）";

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

/*
雾不影响背景清屏色（第四十四轮）
口径：相机背对四边形（中心为清屏色）＋ 绿色清屏色 ＋ 黑雾（密度 1）：
      雾只在几何片元上混合，中心仍应为清屏绿而非黑。
*/
TEST_F(Renderer_Scene_Test, 雾不影响背景清屏色)
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

	//场景：相机背对四边形 ＋ 绿色清屏色 ＋ 黑雾
	engine::Scene_Data scene{};
	scene.position[0] = 0.0f;
	scene.position[1] = 0.0f;
	scene.position[2] = 3.0f;
	scene.target[0] = 0.0f;
	scene.target[1] = 0.0f;
	scene.target[2] = 10.0f;
	scene.clear_color[0] = 0.0f;
	scene.clear_color[1] = 1.0f;
	scene.clear_color[2] = 0.0f;
	scene.clear_color[3] = 1.0f;
	scene.fog_color[0] = 0.0f;
	scene.fog_color[1] = 0.0f;
	scene.fog_color[2] = 0.0f;
	scene.fog_density = 1.0f;
	scene.fog_start = 1.0f;
	scene.fog_end = 4.0f;
	renderer.scene_apply(scene);

	ASSERT_TRUE(renderer.mesh_draw(mesh, nullptr));
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
	std::array<uint8_t, 4> center = pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	RecordProperty("fog_background_center", pixel_text(center));
	EXPECT_GE(center[1], 200u) << "雾把背景清屏色染黑（" << pixel_text(center) << "）";

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

/*
雾起始距离内无雾（第四十四轮）
口径：四边形距相机 3；雾区间 [5, 10]（起始在片元之外）→ 画面与关闭雾时逐字节一致。
*/
TEST_F(Renderer_Scene_Test, 雾起始距离内无雾)
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

	//基线：雾关闭
	const float black[3] = { 0.0f, 0.0f, 0.0f };
	renderer.fog_set(black, 0.0f, 5.0f, 10.0f);
	std::vector<uint8_t> plain = render_frame();
	//启用雾但区间在片元之外
	renderer.fog_set(black, 1.0f, 5.0f, 10.0f);
	EXPECT_EQ(render_frame(), plain) << "起始距离内出现了雾";

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

/*
雾结束距离处完全遮蔽（第四十五轮）
口径：四边形距相机 3；雾色红、密度 1、区间 [1, 3]（片元恰在结束处）→ 中心应完全变为雾色。
*/
TEST_F(Renderer_Scene_Test, 雾结束距离处完全遮蔽)
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

	//红色雾，片元恰在结束距离
	const float red[3] = { 1.0f, 0.0f, 0.0f };
	renderer.fog_set(red, 1.0f, 1.0f, 3.0f);
	ASSERT_TRUE(renderer.mesh_draw(mesh, nullptr));
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
	std::array<uint8_t, 4> center = pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	RecordProperty("fog_end_center", pixel_text(center));
	//外通道保留（红 / 蓝依格式其一）、另一外通道与绿通道接近 0
	uint8_t outer = outer_max(center);
	uint8_t inner = center[0] < center[2] ? center[0] : center[2];
	EXPECT_GE(outer, 200u) << "结束距离处未完全遮蔽（" << pixel_text(center) << "）";
	EXPECT_LE(inner, 30u) << "结束距离处残留场景色（" << pixel_text(center) << "）";
	EXPECT_LE(center[1], 30u) << "绿色通道未归零（" << pixel_text(center) << "）";

        renderer.mesh_unload(mesh);
        renderer.texture_unload(texture);
}

/*
逐光源距离衰减模式覆盖全局（第四十六轮）
口径：立方体 +Z 面中心距点光源 2.0、半径 2.0（d/r = 1）：
      线性衰减为 0、平方反比为 0.5，故「全局线性 ＋ 逐盏平方反比」应明显亮于「全局平方反比 ＋ 逐盏线性」；
      逐盏模式 0 应跟随全局；越界值按 2（平方反比）处理。
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
