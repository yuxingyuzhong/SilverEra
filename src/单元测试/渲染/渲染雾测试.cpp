/*
渲染雾测试（距离雾：压暗远处 / 参数校验 / 场景描述携带 / 起始与结束边界 / 文字与清屏色不受雾影响）
（第五十九轮由「渲染场景效果测试.cpp」按功能域拆分；逻辑未改）
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
