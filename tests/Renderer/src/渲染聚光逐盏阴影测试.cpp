/*
渲染聚光逐盏阴影测试（第六百四十八轮；深研方向八 A2 数据化）
覆盖：逐盏聚光阴影开关（Light_Data.spot_shadow_on 与门面 spot_shadow_light_set）、
     关闭某盏后瓦片 / pass 数下降、重新开启恢复、越界拒绝、默认全开逐字节不变。
口径：2 盏聚光灯 ＋ 多盏聚光阴影 ＋ 方向光阴影关闭；瓦片数取 Frame_Stats.spot_shadow_tiles。
*/
#include "渲染场景测试辅助.h"

namespace
{
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

	//构造「2 盏聚光灯 ＋ 多盏聚光阴影 ＋ 方向光阴影关闭」的光照组
	engine::Light_Data two_spot_light(void)
	{
		engine::Light_Data light{};
		light.spot_count = 2;
		for (uint32_t index = 0; index < 2; ++index)
		{
			light.spot_position[index][0] = index == 0 ? -1.0f : 1.0f;
			light.spot_position[index][1] = 2.0f;
			light.spot_position[index][2] = 3.0f;
			light.spot_direction[index][1] = -1.0f;
			light.spot_direction[index][2] = 0.0f;
			light.spot_radius[index] = 10.0f;
			light.spot_intensity[index] = 1.0f;
			light.spot_inner_deg[index] = 15.0f;
			light.spot_outer_deg[index] = 30.0f;
		}
		light.spot_shadow_enabled = 1;
		light.spot_shadow_all = 1;
		light.spot_shadow_index = 0;
		return light;
	}

	//推进一帧并返回本帧参与投影的聚光瓦片数
	uint32_t render_tiles(engine::Renderer& renderer, engine::Mesh_Handle occluder)
	{
		EXPECT_TRUE(renderer.mesh_draw(occluder, occluder_model(0.0f, 0.0f, -1.0f).data()));
		EXPECT_TRUE(renderer.frame_render());
		return renderer.frame_stats_get().spot_shadow_tiles;
	}

	//两帧之间逐通道差 ≥ 6 的像素数
	uint32_t changed_pixels(const std::vector<uint8_t>& left, const std::vector<uint8_t>& right)
	{
		uint32_t count = 0;
		for (std::size_t index = 0; index + 3 < left.size(); index += 4)
		{
			int maximum = 0;
			for (std::size_t channel = 0; channel < 3; ++channel)
			{
				int delta = static_cast<int>(left[index + channel]) - static_cast<int>(right[index + channel]);
				maximum = (std::max)(maximum, delta < 0 ? -delta : delta);
			}
			if (maximum >= 6)
				++count;
		}
		return count;
	}

	//整帧 RGB 亮度均值
	double mean_luma(const std::vector<uint8_t>& pixels)
	{
		double total = 0.0;
		for (std::size_t index = 0; index + 3 < pixels.size(); index += 4)
			total += 0.2126 * pixels[index] + 0.7152 * pixels[index + 1] + 0.0722 * pixels[index + 2];
		return pixels.empty() ? 0.0 : total / static_cast<double>(pixels.size() / 4);
	}

	//构造「XY 缩放 ＋ 平移」model 矩阵（列主序）
	std::array<float, 16> scale_model(float sx, float sy, float x, float y, float z)
	{
		return {
			sx, 0.0f, 0.0f, 0.0f,
			0.0f, sy, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			x, y, z, 1.0f,
		};
	}
}

/*
默认全开：2 盏聚光灯各占瓦片（第六百四十八轮）
口径：默认 spot_shadow_on 全为 1 → 2 张瓦片、2 次聚光 pass（与引入该字段前一致）。
*/
TEST_F(Renderer_Scene_Test, 聚光逐盏阴影默认全开)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle occluder = 0;
	ASSERT_TRUE(renderer.mesh_load(occluder, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
	renderer.texture_bind_set(texture);

	engine::Camera_Data camera{};
	engine::Fog_Data fog{};
	renderer.scene_apply(camera, two_spot_light(), fog);
	const uint32_t tiles = render_tiles(renderer, occluder);
	const engine::Frame_Stats stats = renderer.frame_stats_get();
	RecordProperty("default_tiles", tiles);
	RecordProperty("default_spot_passes", stats.spot_shadow_passes);
	EXPECT_EQ(tiles, 2u) << "默认全开时应有 2 张聚光瓦片";
	EXPECT_EQ(stats.spot_shadow_passes, 2u) << "默认全开时应有 2 次聚光 pass";

	renderer.mesh_unload(occluder);
	renderer.texture_unload(texture);
}

/*
数据侧逐盏关闭与恢复（第六百四十八轮）
口径：spot_shadow_on[0] = 0 → 只剩 1 张瓦片；恢复为 1 后回到 2 张；其余盏不受影响。
*/
TEST_F(Renderer_Scene_Test, 聚光逐盏阴影数据侧开关)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle occluder = 0;
	ASSERT_TRUE(renderer.mesh_load(occluder, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
	renderer.texture_bind_set(texture);
	engine::Camera_Data camera{};
	engine::Fog_Data fog{};

	//关闭第 1 盏的阴影
	engine::Light_Data light = two_spot_light();
	light.spot_shadow_on[0] = 0;
	renderer.scene_apply(camera, light, fog);
	uint32_t tiles = render_tiles(renderer, occluder);
	RecordProperty("disabled_tiles", tiles);
	EXPECT_EQ(tiles, 1u) << "关闭 1 盏后应只剩 1 张瓦片";
	EXPECT_EQ(renderer.frame_stats_get().spot_shadow_passes, 1u);

	//恢复全开
	light.spot_shadow_on[0] = 1;
	renderer.scene_apply(camera, light, fog);
	tiles = render_tiles(renderer, occluder);
	RecordProperty("restored_tiles", tiles);
	EXPECT_EQ(tiles, 2u) << "恢复后应回到 2 张瓦片";

	renderer.mesh_unload(occluder);
	renderer.texture_unload(texture);
}

/*
门面逐盏开关与越界拒绝（第六百四十八轮）
口径：spot_shadow_light_set(0, false) → 1 张瓦片；再置真 → 2 张；index ≥ 8 返回假且配置不变。
*/
TEST_F(Renderer_Scene_Test, 聚光逐盏阴影门面开关)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle occluder = 0;
	ASSERT_TRUE(renderer.mesh_load(occluder, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
	renderer.texture_bind_set(texture);
	engine::Camera_Data camera{};
	engine::Fog_Data fog{};
	renderer.scene_apply(camera, two_spot_light(), fog);

	//经门面关闭第 1 盏
	ASSERT_TRUE(renderer.spot_shadow_light_set(0, false));
	EXPECT_EQ(render_tiles(renderer, occluder), 1u);

	//重新开启
	ASSERT_TRUE(renderer.spot_shadow_light_set(0, true));
	EXPECT_EQ(render_tiles(renderer, occluder), 2u);

	//越界槽位拒绝且配置不变
	EXPECT_FALSE(renderer.spot_shadow_light_set(9, false));
	EXPECT_EQ(render_tiles(renderer, occluder), 2u);

	renderer.mesh_unload(occluder);
	renderer.texture_unload(texture);
}

/*
逐盏聚光阴影质量缩放（第七百零八轮；A2 收尾）
口径：1 盏聚光灯（z = 3、朝向 −Z）＋ 遮挡板（z = 1、0.4 缩放）＋ 接收面（z = 0）；全局软度 0.5、强度 1。
断言：显式 1 / 1 与不设置逐字节一致；软度 ×4 改变半影；强度 ×0.5 使画面整体变亮。
*/
TEST_F(Renderer_Scene_Test, 聚光逐盏质量缩放)
{
	engine::Renderer probe;
	ASSERT_TRUE(probe.init(hidden_config()));
	if (!probe.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	//渲染一帧（apply = 假时使用默认逐盏缩放 1 / 1）
	auto render = [](bool apply, float softness_scale, float strength_scale)
	{
		std::vector<uint8_t> pixels(pixel_bytes());
		engine::Renderer renderer;
		if (!renderer.init(hidden_config()))
			return pixels;
		uint64_t texture = 0;
		std::vector<uint8_t> texels = solid_white();
		if (!renderer.texture_load(texture, texels.data(), 2, 2))
			return pixels;
		engine::Mesh_Handle receiver = 0;
		engine::Mesh_Handle occluder = 0;
		if (!renderer.mesh_load(receiver, quad_data_with_normal(0.0f, 0.0f, 1.0f))
			|| !renderer.mesh_load(occluder, quad_data_with_normal(0.0f, 0.0f, 1.0f)))
			return pixels;
		renderer.texture_bind_set(texture);

		engine::Light_Data light{};
		//方向光关闭（只用聚光照明），环境光 0.2
		light.intensity = 0.0f;
		light.ambient = 0.2f;
		light.shadow_enabled = 1;
		light.shadow_cascade_count = 1;
		light.spot_count = 1;
		/*
		聚光灯偏轴布置（第七百零八轮）
		灯位 (2.5, 1.5, 3.0)、朝原点；遮挡板偏右上，使阴影落在遮挡板屏幕投影之外（否则被遮挡板自身盖住而不可见）。
		*/
		light.spot_position[0][0] = 2.5f;
		light.spot_position[0][1] = 1.5f;
		light.spot_position[0][2] = 3.0f;
		light.spot_direction[0][0] = -2.5f;
		light.spot_direction[0][1] = -1.5f;
		light.spot_direction[0][2] = -3.0f;
		light.spot_radius[0] = 10.0f;
		light.spot_intensity[0] = 1.0f;
		light.spot_inner_deg[0] = 5.0f;
		light.spot_outer_deg[0] = 30.0f;
		light.spot_shadow_enabled = 1;
		light.spot_shadow_all = 1;
		light.spot_shadow_softness = 0.5f;
		//逐盏质量缩放（仅槽位 0）
		if (apply)
		{
			light.spot_shadow_softness_scale[0] = softness_scale;
			light.spot_shadow_strength_scale[0] = strength_scale;
		}
		engine::Camera_Data camera{};
		engine::Fog_Data fog{};
		renderer.scene_apply(camera, light, fog);
		renderer.shadow_enable_set(true);

		//接收面（z = 0）与遮挡板（z = 1、缩小到 0.4 并偏右上）
		EXPECT_TRUE(renderer.mesh_draw(receiver, scale_model(1.0f, 1.0f, 0.0f, 0.0f, 0.0f).data()));
		EXPECT_TRUE(renderer.mesh_draw(occluder, scale_model(0.4f, 0.4f, 0.6f, 0.4f, 1.0f).data()));
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		renderer.mesh_unload(receiver);
		renderer.mesh_unload(occluder);
		renderer.texture_unload(texture);
		return pixels;
	};

	const std::vector<uint8_t> baseline = render(false, 1.0f, 1.0f);
	const std::vector<uint8_t> explicit_one = render(true, 1.0f, 1.0f);
	const std::vector<uint8_t> soft = render(true, 4.0f, 1.0f);
	const std::vector<uint8_t> weak = render(true, 1.0f, 0.5f);
	//默认与显式 1 / 1 逐字节一致
	EXPECT_TRUE(baseline == explicit_one) << "显式 1 / 1 改变了默认画面";
	//软度缩放改变半影
	const uint32_t soft_changed = changed_pixels(baseline, soft);
	RecordProperty("quality_softness_changed", soft_changed);
	EXPECT_GT(soft_changed, 0u) << "软度缩放未改变半影";
	//强度缩放使阴影变浅（整体变亮）
	const double base_luma = mean_luma(baseline);
	const double weak_luma = mean_luma(weak);
	RecordProperty("quality_baseline_luma", base_luma);
	RecordProperty("quality_weak_luma", weak_luma);
	EXPECT_GT(weak_luma, base_luma) << "强度缩放未使阴影变浅";
	//门面入口：越界槽位与非法值拒绝
	EXPECT_FALSE(probe.spot_shadow_quality_set(9, 2.0f, 1.0f));
	EXPECT_FALSE(probe.spot_shadow_quality_set(0, std::nanf(""), 1.0f));
	EXPECT_TRUE(probe.spot_shadow_quality_set(0, 2.0f, 0.5f));
}

/*
逐盏聚光阴影优先级（第七百零九轮；A2 收尾）
口径：10 盏聚光灯（槽位 0..9，z = 3 朝向 −Z）＋ 遮挡板 ＋ 接收面；全局软度 0、强度 1。
断言：默认优先级全 0 → 取槽位 0..7（8 瓦片）；把槽位 8 / 9 提到 10 → 仍 8 瓦片但选中集合改变，
      画面出现差异（说明高槽位聚光灯确实参与投影，而不只是改变选择）。
*/
TEST_F(Renderer_Scene_Test, 聚光逐盏阴影优先级)
{
	engine::Renderer probe;
	ASSERT_TRUE(probe.init(hidden_config()));
	if (!probe.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	//渲染一帧（boost = 真时把槽位 8 / 9 的优先级提到 10）
	auto render = [](bool boost)
	{
		std::vector<uint8_t> pixels(pixel_bytes());
		engine::Renderer renderer;
		if (!renderer.init(hidden_config()))
			return pixels;
		uint64_t texture = 0;
		std::vector<uint8_t> texels = solid_white();
		if (!renderer.texture_load(texture, texels.data(), 2, 2))
			return pixels;
		engine::Mesh_Handle receiver = 0;
		engine::Mesh_Handle occluder = 0;
		if (!renderer.mesh_load(receiver, quad_data_with_normal(0.0f, 0.0f, 1.0f))
			|| !renderer.mesh_load(occluder, quad_data_with_normal(0.0f, 0.0f, 1.0f)))
			return pixels;
		renderer.texture_bind_set(texture);

		engine::Light_Data light{};
		light.intensity = 0.0f;
		light.ambient = 0.15f;
		light.shadow_enabled = 1;
		light.shadow_cascade_count = 1;
		light.spot_count = 10;
		for (uint32_t index = 0; index < 10; ++index)
		{
			//沿 X 排开，均朝 −Z 照射接收面
			light.spot_position[index][0] = -1.1f + 0.22f * static_cast<float>(index);
			light.spot_position[index][1] = 0.0f;
			light.spot_position[index][2] = 3.0f;
			light.spot_direction[index][2] = -1.0f;
			light.spot_radius[index] = 10.0f;
			light.spot_intensity[index] = 0.12f;
			light.spot_inner_deg[index] = 10.0f;
			light.spot_outer_deg[index] = 30.0f;
		}
		light.spot_shadow_enabled = 1;
		light.spot_shadow_all = 1;
		//高槽位优先级（其余保持默认 0）
		if (boost)
		{
			light.spot_shadow_priority[8] = 10.0f;
			light.spot_shadow_priority[9] = 10.0f;
		}
		engine::Camera_Data camera{};
		engine::Fog_Data fog{};
		renderer.scene_apply(camera, light, fog);
		renderer.shadow_enable_set(true);

		EXPECT_TRUE(renderer.mesh_draw(receiver, scale_model(1.0f, 1.0f, 0.0f, 0.0f, 0.0f).data()));
		EXPECT_TRUE(renderer.mesh_draw(occluder, scale_model(0.3f, 0.3f, 0.0f, 0.0f, 1.0f).data()));
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		renderer.mesh_unload(receiver);
		renderer.mesh_unload(occluder);
		renderer.texture_unload(texture);
		return pixels;
	};

	const std::vector<uint8_t> baseline = render(false);
	const std::vector<uint8_t> boosted = render(true);
	const uint32_t difference = changed_pixels(baseline, boosted);
	RecordProperty("priority_changed_pixels", difference);
	EXPECT_GT(difference, 0u) << "优先级未改变参与投影的聚光灯集合";
	//门面入口：越界与非法值
	EXPECT_FALSE(probe.spot_shadow_priority_set(32, 1.0f));
	EXPECT_FALSE(probe.spot_shadow_priority_set(0, std::nanf("")));
	EXPECT_TRUE(probe.spot_shadow_priority_set(9, 5.0f));
}
