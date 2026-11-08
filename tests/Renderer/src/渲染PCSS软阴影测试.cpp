/*
渲染 PCSS 软阴影测试（第六百五十七轮；深研方向八 B2）
覆盖：关闭口径与旧版逐字节一致、开启后阴影边界出现差异、诊断字段与门面钳制。
口径：单级级联、方向光沿 +Z、遮挡面在 z = -10、接收面在 z = -9.5；64×36 隐藏窗口。
*/
#include "渲染场景测试辅助.h"


namespace
{
	//构造沿 X、Y、Z 平移的模型矩阵（列主序）
	std::array<float, 16> translate_model(float x, float y, float z)
	{
		return {
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			x, y, z, 1.0f,
		};
	}

	//构造缩放并平移到遮挡位置的模型矩阵（列主序）
	std::array<float, 16> occluder_model(float x, float y, float z)
	{
		return {
			0.6f, 0.0f, 0.0f, 0.0f,
			0.0f, 0.6f, 0.0f, 0.0f,
			0.0f, 0.0f, 0.6f, 0.0f,
			x, y, z, 1.0f,
		};
	}

	//构造「方向光沿 +Z、环境光 0」的光照组
	engine::Light_Data positive_z_light(void)
	{
		engine::Light_Data light{};
		light.direction[2] = 1.0f;
		light.intensity = 1.0f;
		light.ambient = 0.0f;
		return light;
	}

	//应用相机与光照并开启阴影
	void scene_apply(engine::Renderer& renderer, const engine::Camera_Data& camera, const engine::Light_Data& light)
	{
		engine::Fog_Data fog{};
		renderer.scene_apply(camera, light, fog);
		renderer.shadow_enable_set(true);
	}

	//绘制固定场景并推进一帧，返回画面像素（接收面在原点、遮挡面 z = -3，半影宽度可达数纹素）
	std::vector<uint8_t> draw_scene(engine::Renderer& renderer, engine::Mesh_Handle occluder, engine::Mesh_Handle receiver)
	{
		EXPECT_TRUE(renderer.mesh_draw(occluder, occluder_model(0.0f, 0.0f, -3.0f).data()));
		EXPECT_TRUE(renderer.mesh_draw(receiver, translate_model(0.0f, 0.0f, 0.0f).data()));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return pixels;
	}

	//统计两帧画面中不同的字节数
	std::size_t pixel_difference(const std::vector<uint8_t>& first, const std::vector<uint8_t>& second)
	{
		//长度不一致视为全部不同（防御性）
		if (first.size() != second.size())
			return first.size() > second.size() ? first.size() : second.size();
		std::size_t different = 0;
		for (std::size_t index = 0; index < first.size(); ++index)
			if (first[index] != second[index])
				++different;
		return different;
	}
}

/*
关闭口径逐字节一致（第六百五十七轮）
口径：默认（shadow_pcss = 0）与显式关闭渲染两次，画面逐字节一致（PCSS 分支不得被走到）。
*/
TEST_F(Renderer_Scene_Test, PCSS关闭口径逐字节一致)
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

	engine::Camera_Data camera{};
	engine::Light_Data light = positive_z_light();
	//默认（关闭）渲染
	scene_apply(renderer, camera, light);
	const std::vector<uint8_t> reference = draw_scene(renderer, occluder, receiver);
	ASSERT_FALSE(reference.empty()) << "画面回读为空";
	EXPECT_EQ(renderer.frame_stats_get().shadow_pcss, 0u);

	//显式关闭再渲染一次
	light.shadow_pcss = 0;
	scene_apply(renderer, camera, light);
	const std::vector<uint8_t> again = draw_scene(renderer, occluder, receiver);
	EXPECT_TRUE(reference == again) << "显式关闭 PCSS 后画面与默认不一致";
	EXPECT_EQ(renderer.frame_stats_get().shadow_pcss, 0u);

	renderer.mesh_unload(occluder);
	renderer.mesh_unload(receiver);
	renderer.texture_unload(texture);
}

/*
开启后阴影边界出现差异（第六百五十七轮）
口径：同一相机 / 场景，开启 PCSS（搜索 24、半径上限 8）后与关闭帧对比：
     至少一处像素变化（半影软化），且诊断字段为 1。
*/
TEST_F(Renderer_Scene_Test, PCSS开启改变阴影边界)
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

	engine::Camera_Data camera{};
	engine::Light_Data light = positive_z_light();
	//关闭：基准帧（先确认中心确实被遮蔽，否则差异断言无意义）
	scene_apply(renderer, camera, light);
	const std::vector<uint8_t> hard = draw_scene(renderer, occluder, receiver);
	ASSERT_FALSE(hard.empty()) << "画面回读为空";
	EXPECT_LT(center_brightness(hard), 60u) << "基准帧中心未被遮蔽（PCSS 对比场景无效）";

	//开启：同场景再渲染
	light.shadow_pcss = 1;
	light.shadow_pcss_search = 24.0f;
	light.shadow_pcss_max = 8.0f;
	scene_apply(renderer, camera, light);
	const std::vector<uint8_t> soft = draw_scene(renderer, occluder, receiver);
	const std::size_t difference = pixel_difference(hard, soft);
	RecordProperty("pcss_difference_bytes", difference);
	EXPECT_EQ(renderer.frame_stats_get().shadow_pcss, 1u);
	EXPECT_GT(difference, 0u) << "PCSS 开启后画面与硬边完全一致（分支未生效）";

	renderer.mesh_unload(occluder);
	renderer.mesh_unload(receiver);
	renderer.texture_unload(texture);
}

/*
门面钳制与关闭（第六百五十七轮）
口径：shadow_pcss_set 越界参数被钳制且不报错；关闭后诊断字段归 0。
*/
TEST_F(Renderer_Scene_Test, PCSS门面钳制与关闭)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//越界参数（搜索 100 / 半径 0.5）→ 钳制后仍可开启
	renderer.shadow_pcss_set(true, 100.0f, 0.5f);
	EXPECT_EQ(renderer.frame_stats_get().shadow_pcss, 1u);
	//关闭
	renderer.shadow_pcss_set(false, 24.0f, 8.0f);
	EXPECT_EQ(renderer.frame_stats_get().shadow_pcss, 0u);
}
