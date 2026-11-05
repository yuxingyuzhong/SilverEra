/*
渲染阴影诊断测试（第六百四十七轮；深研方向八 A4 补齐）
覆盖：方向光级联阴影 pass 数（单级 1 / 双级 2）、静止帧复用（pass 数归 0）、
     聚光瓦片 pass 数与阴影图边长（shadow_resolution）。
口径：默认阴影分辨率 1024；阴影 pass 计数为「最近一帧」语义，复用帧为 0。
*/
#include "渲染场景测试辅助.h"

namespace
{
	//构造沿 X、Z 平移的模型矩阵（列主序）
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

	//应用相机与光照（雾保持默认；scene_apply 会按 Light_Data 重置阴影，故随后补开）
	void scene_apply(engine::Renderer& renderer, const engine::Camera_Data& camera)
	{
		engine::Fog_Data fog{};
		renderer.scene_apply(camera, positive_z_light(), fog);
		renderer.shadow_enable_set(true);
	}

	//绘制固定场景并推进一帧（遮挡面 z = -10、接收面 z = -9.5）
	void draw_scene(engine::Renderer& renderer, engine::Mesh_Handle occluder, engine::Mesh_Handle receiver)
	{
		EXPECT_TRUE(renderer.mesh_draw(occluder, occluder_model(0.0f, 0.0f, -10.0f).data()));
		EXPECT_TRUE(renderer.mesh_draw(receiver, translate_model(0.0f, 0.0f, -9.5f).data()));
		EXPECT_TRUE(renderer.frame_render());
	}
}

/*
方向光级联阴影 pass 数（第六百四十七轮）
口径：单级一帧执行 1 次阴影 pass；切双级后一帧执行 2 次（每片一次）；
     阴影图边长为默认 1024；聚光 pass 数为 0。
*/
TEST_F(Renderer_Scene_Test, 阴影pass数随级联数变化)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle receiver = 0;
	engine::Mesh_Handle occluder = 0;
	ASSERT_TRUE(renderer.mesh_load(receiver, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
	ASSERT_TRUE(renderer.mesh_load(occluder, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
	renderer.texture_bind_set(texture);

	//单级
	engine::Camera_Data camera{};
	scene_apply(renderer, camera);
	draw_scene(renderer, occluder, receiver);
	engine::Frame_Stats stats = renderer.frame_stats_get();
	RecordProperty("single_passes", stats.shadow_passes);
	RecordProperty("single_resolution", stats.shadow_resolution);
	EXPECT_EQ(stats.shadow_passes, 1u) << "单级阴影 pass 数应为 1";
	EXPECT_EQ(stats.spot_shadow_passes, 0u) << "未开启聚光阴影时聚光 pass 数应为 0";
	EXPECT_EQ(stats.shadow_resolution, 1024u) << "默认阴影图边长应为 1024";

	//切双级（矩阵变化触发重绘）
	renderer.shadow_cascade_set(2, 3.0f, 12.0f);
	draw_scene(renderer, occluder, receiver);
	stats = renderer.frame_stats_get();
	RecordProperty("dual_passes", stats.shadow_passes);
	EXPECT_EQ(stats.shadow_passes, 2u) << "双级阴影 pass 数应为 2";

	renderer.mesh_unload(occluder);
	renderer.mesh_unload(receiver);
	renderer.texture_unload(texture);
}

/*
静止帧阴影复用（第六百四十七轮）
口径：同一相机与场景再推进一帧，签名未变 → 不执行阴影 pass（shadow_passes = 0），复用计数递增。
*/
TEST_F(Renderer_Scene_Test, 静止帧阴影pass归零)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle receiver = 0;
	engine::Mesh_Handle occluder = 0;
	ASSERT_TRUE(renderer.mesh_load(receiver, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
	ASSERT_TRUE(renderer.mesh_load(occluder, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
	renderer.texture_bind_set(texture);

	engine::Camera_Data camera{};
	scene_apply(renderer, camera);
	renderer.shadow_cascade_set(2, 3.0f, 12.0f);

	//首帧重绘
	draw_scene(renderer, occluder, receiver);
	const uint32_t first_passes = renderer.frame_stats_get().shadow_passes;
	const uint64_t reuses_before = renderer.frame_stats_get().shadow_reuses;
	EXPECT_EQ(first_passes, 2u);

	//第二帧复用
	draw_scene(renderer, occluder, receiver);
	const engine::Frame_Stats stats = renderer.frame_stats_get();
	RecordProperty("reuse_passes", stats.shadow_passes);
	RecordProperty("reuse_count", stats.shadow_reuses);
	EXPECT_EQ(stats.shadow_passes, 0u) << "静止帧不应再次执行阴影 pass";
	EXPECT_GE(stats.shadow_reuses, reuses_before + 1u) << "静止帧未计入阴影复用";

	renderer.mesh_unload(occluder);
	renderer.mesh_unload(receiver);
	renderer.texture_unload(texture);
}

/*
聚光瓦片 pass 数（第六百四十七轮）
口径：开启多盏聚光阴影并只登记 1 盏聚光灯 → 一帧执行 1 次聚光瓦片 pass；
     方向光阴影关闭 → 方向光级联 pass 为 0，总 pass 数 = 1。
*/
TEST_F(Renderer_Scene_Test, 聚光瓦片pass计数)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle occluder = 0;
	ASSERT_TRUE(renderer.mesh_load(occluder, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
	renderer.texture_bind_set(texture);

	//1 盏聚光灯（位于相机上方、朝下照射场景）＋ 多盏聚光阴影 ＋ 方向光阴影关闭
	engine::Light_Data light{};
	light.spot_count = 1;
	light.spot_position[0][1] = 2.0f;
	light.spot_position[0][2] = 3.0f;
	light.spot_direction[0][1] = -1.0f;
	light.spot_direction[0][2] = 0.0f;
	light.spot_radius[0] = 10.0f;
	light.spot_intensity[0] = 1.0f;
	light.spot_inner_deg[0] = 15.0f;
	light.spot_outer_deg[0] = 30.0f;
	light.spot_shadow_enabled = 1;
	light.spot_shadow_all = 1;
	light.spot_shadow_index = 0;
	engine::Camera_Data camera{};
	engine::Fog_Data fog{};
	renderer.scene_apply(camera, light, fog);

	//方向光阴影保持关闭
	EXPECT_TRUE(renderer.mesh_draw(occluder, occluder_model(0.0f, 0.0f, -1.0f).data()));
	EXPECT_TRUE(renderer.frame_render());
	const engine::Frame_Stats stats = renderer.frame_stats_get();
	RecordProperty("spot_passes", stats.spot_shadow_passes);
	RecordProperty("spot_total_passes", stats.shadow_passes);
	EXPECT_GE(stats.spot_shadow_passes, 1u) << "聚光瓦片 pass 未执行";
	EXPECT_EQ(stats.shadow_passes, stats.spot_shadow_passes) << "方向光阴影关闭时总 pass 数应等于聚光 pass 数";

	renderer.mesh_unload(occluder);
	renderer.texture_unload(texture);
}

/*
阴影提交次数等于 pass 数（第六百六十五轮；C2 基线）
口径：当前「一片一次提交」，单级 1 次、双级 2 次，且与 shadow_passes 相等；
     C2 单次提交合并落地后本用例改为「提交恒 1、pass 数不变」。
*/
TEST_F(Renderer_Scene_Test, 阴影提交数等于pass数)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle occluder = 0;
	ASSERT_TRUE(renderer.mesh_load(occluder, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
	renderer.texture_bind_set(texture);

	//方向光沿 +Z、阴影开启（单级）
	engine::Camera_Data camera{};
	engine::Light_Data light{};
	light.direction[2] = 1.0f;
	light.intensity = 1.0f;
	light.ambient = 0.0f;
	light.shadow_enabled = 1;
	light.shadow_cascade_count = 1;
	engine::Fog_Data fog{};
	renderer.scene_apply(camera, light, fog);
	renderer.shadow_enable_set(true);
	EXPECT_TRUE(renderer.mesh_draw(occluder, occluder_model(0.0f, 0.0f, -1.0f).data()));
	EXPECT_TRUE(renderer.frame_render());
	const engine::Frame_Stats single = renderer.frame_stats_get();
	RecordProperty("single_submits", single.shadow_submits);
	RecordProperty("single_passes", single.shadow_passes);
	EXPECT_EQ(single.shadow_submits, 1u) << "单级阴影提交次数不为 1";
	EXPECT_EQ(single.shadow_submits, single.shadow_passes) << "提交次数与 pass 数不一致";

	//双级级联
	light.shadow_cascade_count = 2;
	renderer.scene_apply(camera, light, fog);
	renderer.shadow_enable_set(true);
	EXPECT_TRUE(renderer.mesh_draw(occluder, occluder_model(0.0f, 0.0f, -1.0f).data()));
	EXPECT_TRUE(renderer.frame_render());
	const engine::Frame_Stats dual = renderer.frame_stats_get();
	RecordProperty("dual_submits", dual.shadow_submits);
	RecordProperty("dual_passes", dual.shadow_passes);
	EXPECT_EQ(dual.shadow_submits, 2u) << "双级阴影提交次数不为 2";
	EXPECT_EQ(dual.shadow_submits, dual.shadow_passes) << "双级提交次数与 pass 数不一致";

	renderer.mesh_unload(occluder);
	renderer.texture_unload(texture);
}

/*
阴影批量提交（第六百六十九轮；C2）
口径：双级级联 ＋ 2 盏聚光瓦片（共 4 段）。关闭：提交数 = 段数；开启：提交数 = 1；
     开关两态画面逐字节一致、方向光阴影图逐字节一致。
*/
TEST_F(Renderer_Scene_Test, 阴影批量提交单次且画面不变)
{
	//单次运行的产物（画面 ＋ 方向光阴影图 ＋ 统计）
	struct Outcome
	{
		std::vector<uint8_t> pixels;
		std::vector<uint8_t> shadow;
		uint32_t shadow_width = 0;
		uint32_t shadow_height = 0;
		engine::Frame_Stats stats{};
		bool ok = false;
	};
	auto run = [](bool batch)
	{
		Outcome outcome;
		engine::Renderer renderer;
		if (!renderer.init(hidden_config()) || !renderer.readback_supported())
			return outcome;
		uint64_t texture = 0;
		std::vector<uint8_t> texels = solid_white();
		if (!renderer.texture_load(texture, texels.data(), 2, 2))
			return outcome;
		engine::Mesh_Handle receiver = 0;
		engine::Mesh_Handle occluder = 0;
		if (!renderer.mesh_load(receiver, quad_data_with_normal(0.0f, 0.0f, -1.0f))
			|| !renderer.mesh_load(occluder, quad_data_with_normal(0.0f, 0.0f, -1.0f)))
			return outcome;
		renderer.texture_bind_set(texture);

		//双级级联 ＋ 2 盏聚光（两盏都参与阴影，形成 2 个瓦片段）
		engine::Light_Data light{};
		light.direction[2] = 1.0f;
		light.intensity = 1.0f;
		light.ambient = 0.0f;
		light.shadow_enabled = 1;
		light.shadow_cascade_count = 2;
		light.shadow_batch_submit = batch ? 1 : 0;
		light.spot_count = 2;
		for (int index = 0; index < 2; ++index)
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
		engine::Camera_Data camera{};
		engine::Fog_Data fog{};
		renderer.scene_apply(camera, light, fog);
		renderer.shadow_enable_set(true);

		//接收面在原点、遮挡面在其前方
		EXPECT_TRUE(renderer.mesh_draw(occluder, occluder_model(0.0f, 0.0f, -1.0f).data()));
		EXPECT_TRUE(renderer.mesh_draw(receiver, translate_model(0.0f, 0.0f, 0.0f).data()));
		outcome.pixels.resize(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(outcome.pixels.data(),
			static_cast<std::uint32_t>(outcome.pixels.size())));
		outcome.stats = renderer.frame_stats_get();

		//方向光阴影图整图回读（批量 / 逐段应逐字节一致）
		std::vector<uint8_t> shadow_map(2048u * 2048u * 4u);
		if (renderer.shadow_readback(shadow_map.data(), static_cast<uint32_t>(shadow_map.size()),
			&outcome.shadow_width, &outcome.shadow_height))
		{
			shadow_map.resize(static_cast<std::size_t>(outcome.shadow_width) * outcome.shadow_height * 4u);
			outcome.shadow = std::move(shadow_map);
		}
		outcome.ok = true;
		return outcome;
	};

	const Outcome per_pass = run(false);
	const Outcome batched = run(true);
	ASSERT_TRUE(per_pass.ok) << "逐段配置初始化失败";
	ASSERT_TRUE(batched.ok) << "批量配置初始化失败";
	RecordProperty("per_pass_submits", per_pass.stats.shadow_submits);
	RecordProperty("batched_submits", batched.stats.shadow_submits);
	RecordProperty("shadow_passes", per_pass.stats.shadow_passes);
	//逐段：提交数等于段数（2 级联 ＋ 2 瓦片）
	EXPECT_EQ(per_pass.stats.shadow_submits, per_pass.stats.shadow_passes) << "逐段口径提交数应等于段数";
	EXPECT_GE(per_pass.stats.shadow_passes, 4u) << "阴影段数不足（应含 2 级联 ＋ 2 瓦片）";
	//批量：一次提交、段数不变
	EXPECT_EQ(batched.stats.shadow_submits, 1u) << "批量提交次数不为 1";
	EXPECT_EQ(batched.stats.shadow_passes, per_pass.stats.shadow_passes) << "批量改变了阴影段数";
	//画面与阴影图一致
	EXPECT_TRUE(batched.pixels == per_pass.pixels) << "批量提交改变了画面";
	if (!per_pass.shadow.empty() || !batched.shadow.empty())
		EXPECT_TRUE(batched.shadow == per_pass.shadow) << "批量提交改变了方向光阴影图";
}

/*
聚光阴影图集（第六百七十轮；C1）
口径：2 盏聚光（瓦片 0 / 1）。关闭图集 → 开启图集：
画面逐字节一致、阴影 pass 数不变、图集实际生效；关闭态查询返回假。
*/
TEST_F(Renderer_Scene_Test, 聚光阴影图集两态画面一致)
{
	//单次运行的产物（画面 ＋ 统计 ＋ 图集状态）
	struct Outcome
	{
		std::vector<uint8_t> pixels;
		engine::Frame_Stats stats{};
		bool atlas_active = false;
		bool ok = false;
	};
	auto run = [](bool atlas)
	{
		Outcome outcome;
		engine::Renderer renderer;
		if (!renderer.init(hidden_config()) || !renderer.readback_supported())
			return outcome;
		uint64_t texture = 0;
		std::vector<uint8_t> texels = solid_white();
		if (!renderer.texture_load(texture, texels.data(), 2, 2))
			return outcome;
		engine::Mesh_Handle receiver = 0;
		engine::Mesh_Handle occluder = 0;
		if (!renderer.mesh_load(receiver, quad_data_with_normal(0.0f, 0.0f, -1.0f))
			|| !renderer.mesh_load(occluder, quad_data_with_normal(0.0f, 0.0f, -1.0f)))
			return outcome;
		renderer.texture_bind_set(texture);

		//2 盏聚光各自投影（瓦片 0 / 1）
		engine::Light_Data light{};
		light.direction[2] = 1.0f;
		light.intensity = 1.0f;
		light.ambient = 0.0f;
		light.spot_count = 2;
		for (int index = 0; index < 2; ++index)
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
		light.spot_shadow_atlas = atlas ? 1 : 0;
		engine::Camera_Data camera{};
		engine::Fog_Data fog{};
		renderer.scene_apply(camera, light, fog);
		renderer.shadow_enable_set(true);

		//接收面在原点、遮挡面在其前方
		EXPECT_TRUE(renderer.mesh_draw(occluder, occluder_model(0.0f, 0.0f, -1.0f).data()));
		EXPECT_TRUE(renderer.mesh_draw(receiver, translate_model(0.0f, 0.0f, 0.0f).data()));
		outcome.pixels.resize(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(outcome.pixels.data(),
			static_cast<std::uint32_t>(outcome.pixels.size())));
		outcome.stats = renderer.frame_stats_get();
		outcome.atlas_active = renderer.spot_shadow_atlas();
		outcome.ok = true;
		return outcome;
	};

	const Outcome tiled = run(false);
	const Outcome atlas = run(true);
	ASSERT_TRUE(tiled.ok) << "独立瓦片配置初始化失败";
	ASSERT_TRUE(atlas.ok) << "图集配置初始化失败";
	//图集实际生效（未生效时用例失去意义）
	EXPECT_FALSE(tiled.atlas_active) << "关闭态不应启用图集";
	EXPECT_TRUE(atlas.atlas_active) << "图集未生效";
	RecordProperty("tiled_passes", tiled.stats.shadow_passes);
	RecordProperty("atlas_passes", atlas.stats.shadow_passes);
	//阴影段数不变（图集只改目标布局）
	EXPECT_EQ(atlas.stats.shadow_passes, tiled.stats.shadow_passes) << "图集改变了阴影段数";
	//画面逐字节一致
	EXPECT_TRUE(atlas.pixels == tiled.pixels) << "图集改变了画面";
}

/*
聚光阴影图集开销对比（第六百七十六轮；E1 性能基线）
口径：2 盏聚光 ＋ 单级方向光，同一场景各建一个渲染器；图集关 / 开各取首帧阴影统计。
断言：两种模式阴影段数一致；GPU 阴影耗时为非负（设备不支持时间戳时为 0，仅记录）。
*/
TEST_F(Renderer_Scene_Test, 聚光图集阴影耗时对比)
{
	struct Outcome
	{
		engine::Frame_Stats stats{};
		engine::Mesh_Handle receiver = 0;
		engine::Mesh_Handle occluder = 0;
		uint64_t texture = 0;
		bool ok = false;
	};
	auto run = [](bool atlas)
	{
		Outcome outcome;
		engine::Renderer renderer;
		if (!renderer.init(hidden_config()) || !renderer.readback_supported())
			return outcome;
		std::vector<uint8_t> texels = solid_white();
		if (!renderer.texture_load(outcome.texture, texels.data(), 2, 2))
			return outcome;
		if (!renderer.mesh_load(outcome.receiver, quad_data_with_normal(0.0f, 0.0f, -1.0f))
			|| !renderer.mesh_load(outcome.occluder, quad_data_with_normal(0.0f, 0.0f, -1.0f)))
			return outcome;
		renderer.texture_bind_set(outcome.texture);

		//单级方向光 ＋ 2 盏聚光（两盏都参与阴影）
		engine::Light_Data light{};
		light.direction[2] = 1.0f;
		light.intensity = 1.0f;
		light.ambient = 0.0f;
		light.shadow_enabled = 1;
		light.shadow_cascade_count = 1;
		light.spot_count = 2;
		for (int index = 0; index < 2; ++index)
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
		light.spot_shadow_atlas = atlas ? 1 : 0;
		engine::Camera_Data camera{};
		engine::Fog_Data fog{};
		renderer.scene_apply(camera, light, fog);
		renderer.shadow_enable_set(true);

		EXPECT_TRUE(renderer.mesh_draw(outcome.occluder, occluder_model(0.0f, 0.0f, -1.0f).data()));
		EXPECT_TRUE(renderer.mesh_draw(outcome.receiver, translate_model(0.0f, 0.0f, 0.0f).data()));
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(),
			static_cast<std::uint32_t>(pixels.size())));
		outcome.stats = renderer.frame_stats_get();
		/*
		第二帧（第六百七十六轮）：轻移聚光触发重绘，避开首帧「图集创建 / 绑定」一次性开销，
		取稳定态阴影耗时。
		*/
		light.spot_position[0][0] += 0.01f;
		renderer.scene_apply(camera, light, fog);
		renderer.shadow_enable_set(true);
		EXPECT_TRUE(renderer.mesh_draw(outcome.occluder, occluder_model(0.0f, 0.0f, -1.0f).data()));
		EXPECT_TRUE(renderer.mesh_draw(outcome.receiver, translate_model(0.0f, 0.0f, 0.0f).data()));
		EXPECT_TRUE(renderer.frame_render(pixels.data(),
			static_cast<std::uint32_t>(pixels.size())));
		outcome.stats = renderer.frame_stats_get();
		renderer.mesh_unload(outcome.occluder);
		renderer.mesh_unload(outcome.receiver);
		renderer.texture_unload(outcome.texture);
		outcome.ok = true;
		return outcome;
	};

	const Outcome tiled = run(false);
	const Outcome atlas = run(true);
	ASSERT_TRUE(tiled.ok) << "独立瓦片配置初始化失败";
	ASSERT_TRUE(atlas.ok) << "图集配置初始化失败";
	RecordProperty("shadow_gpu_ms_tiled", tiled.stats.gpu_shadow_ms);
	RecordProperty("shadow_gpu_ms_atlas", atlas.stats.gpu_shadow_ms);
	RecordProperty("shadow_passes_tiled", tiled.stats.shadow_passes);
	RecordProperty("shadow_passes_atlas", atlas.stats.shadow_passes);
	//段数与聚光 pass 数不因图集改变
	EXPECT_EQ(atlas.stats.shadow_passes, tiled.stats.shadow_passes) << "图集改变了阴影段数";
	EXPECT_EQ(atlas.stats.spot_shadow_passes, tiled.stats.spot_shadow_passes) << "图集改变了聚光段数";
	//耗时为非负（无时间戳能力时为 0）
	EXPECT_GE(tiled.stats.gpu_shadow_ms, 0.0);
	EXPECT_GE(atlas.stats.gpu_shadow_ms, 0.0);
}

/*
8 瓦片图集映射与 MSAA 组合（第六百七十七轮；C1 收尾）
口径：8 盏聚光（瓦片 0..7，覆盖图集两行）＋ 单级方向光；图集关 / 开对比，MSAA 1× / 4× 各一组。
断言：图集实际生效、开 / 关画面逐字节一致、阴影段数不变。
*/
TEST_F(Renderer_Scene_Test, 图集八瓦片映射与MSAA一致)
{
	struct Outcome
	{
		std::vector<uint8_t> pixels;
		engine::Frame_Stats stats{};
		bool atlas_active = false;
		bool ok = false;
	};
	auto run = [](bool atlas, uint32_t samples)
	{
		Outcome outcome;
		engine::Renderer renderer;
		engine::Renderer_Config config = hidden_config();
		config.msaa_samples = samples;
		if (!renderer.init(config) || !renderer.readback_supported())
			return outcome;
		uint64_t texture = 0;
		std::vector<uint8_t> texels = solid_white();
		if (!renderer.texture_load(texture, texels.data(), 2, 2))
			return outcome;
		engine::Mesh_Handle receiver = 0;
		engine::Mesh_Handle occluder = 0;
		if (!renderer.mesh_load(receiver, quad_data_with_normal(0.0f, 0.0f, -1.0f))
			|| !renderer.mesh_load(occluder, quad_data_with_normal(0.0f, 0.0f, -1.0f)))
			return outcome;
		renderer.texture_bind_set(texture);

		//8 盏聚光沿 X 均匀分布（覆盖图集 4 列 × 2 行）
		engine::Light_Data light{};
		light.direction[2] = 1.0f;
		light.intensity = 1.0f;
		light.ambient = 0.0f;
		light.shadow_enabled = 1;
		light.shadow_cascade_count = 1;
		light.spot_count = 8;
		for (int index = 0; index < 8; ++index)
		{
			light.spot_position[index][0] = -3.5f + static_cast<float>(index);
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
		light.spot_shadow_atlas = atlas ? 1 : 0;
		engine::Camera_Data camera{};
		engine::Fog_Data fog{};
		renderer.scene_apply(camera, light, fog);
		renderer.shadow_enable_set(true);

		EXPECT_TRUE(renderer.mesh_draw(occluder, occluder_model(0.0f, 0.0f, -1.0f).data()));
		EXPECT_TRUE(renderer.mesh_draw(receiver, translate_model(0.0f, 0.0f, 0.0f).data()));
		outcome.pixels.resize(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(outcome.pixels.data(),
			static_cast<std::uint32_t>(outcome.pixels.size())));
		outcome.stats = renderer.frame_stats_get();
		outcome.atlas_active = renderer.spot_shadow_atlas();
		renderer.mesh_unload(occluder);
		renderer.mesh_unload(receiver);
		renderer.texture_unload(texture);
		outcome.ok = true;
		return outcome;
	};

	//MSAA 1× 与 4× 两组：图集关 / 开各一次
	for (uint32_t samples : { 1u, 4u })
	{
		const Outcome tiled = run(false, samples);
		const Outcome atlas = run(true, samples);
		ASSERT_TRUE(tiled.ok) << "独立瓦片初始化失败（MSAA " << samples << "）";
		ASSERT_TRUE(atlas.ok) << "图集初始化失败（MSAA " << samples << "）";
		EXPECT_FALSE(tiled.atlas_active);
		EXPECT_TRUE(atlas.atlas_active) << "图集未生效（MSAA " << samples << "）";
		EXPECT_EQ(atlas.stats.shadow_passes, tiled.stats.shadow_passes) << "段数变化（MSAA " << samples << "）";
		EXPECT_TRUE(atlas.pixels == tiled.pixels) << "画面不一致（MSAA " << samples << "）";
	}
}
