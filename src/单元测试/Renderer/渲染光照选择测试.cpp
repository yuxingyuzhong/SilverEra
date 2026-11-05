/*
渲染光照选择测试（第六百四十五轮；深研方向八 · A1 / A4）
覆盖：声明顺序口径（默认，与引入前一致）、重要性选择（择优 ＋ 同分同序）、预算截断、诊断统计。
*/
#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>

#include "Engine/EngineSystem/src/core/Renderer/src/render/Renderer/渲染器.h"

namespace
{
	//构造隐藏窗口配置
	engine::Renderer_Config hidden_config(void)
	{
		engine::Renderer_Config config;
		config.width = 64;
		config.height = 36;
		config.hidden = true;
		return config;
	}

	//构造默认相机（位于 +Z 方向看向原点，作为重要性评分的距离基准）
	engine::Camera_Data default_camera(void)
	{
		engine::Camera_Data camera{};
		camera.position[0] = 0.0f;
		camera.position[1] = 0.0f;
		camera.position[2] = 3.0f;
		camera.target[0] = 0.0f;
		camera.target[1] = 0.0f;
		camera.target[2] = 0.0f;
		camera.fov = 1.0472f;
		camera.near_plane = 0.1f;
		camera.far_plane = 100.0f;
		return camera;
	}

	//构造关闭雾
	engine::Fog_Data empty_fog(void)
	{
		engine::Fog_Data fog{};
		fog.density = 0.0f;
		return fog;
	}

	//逐盏状态与声明参数逐字段比较（位置 / 强度 / 是否聚光）
	void expect_state(const engine::Renderer& renderer, uint32_t slot, const float position[3],
		float intensity, bool is_spot)
	{
		engine::Point_Light_State state{};
		ASSERT_TRUE(renderer.point_light_state_get(slot, state));
		for (int axis = 0; axis < 3; ++axis)
			EXPECT_FLOAT_EQ(state.position[axis], position[axis]);
		EXPECT_FLOAT_EQ(state.intensity, intensity);
		EXPECT_EQ(state.is_spot, is_spot);
	}
}

//声明顺序口径与光照诊断（默认路径与引入前逐条一致）
TEST(Renderer_Light_Select_Test, 声明顺序口径与诊断)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Light_Data light{};
	//8 盏点光 ＋ 4 盏聚光（候选 12 条 > 前向上限 8）
	light.point_count = 8;
	light.spot_count = 4;
	for (uint32_t index = 0; index < 8; ++index)
	{
		light.point_position[index][0] = static_cast<float>(index) * 0.5f - 1.75f;
		light.point_position[index][2] = -2.0f;
		light.point_radius[index] = 4.0f;
		light.point_intensity[index] = 0.3f;
	}
	for (uint32_t index = 0; index < 4; ++index)
	{
		light.spot_position[index][0] = static_cast<float>(index) * 0.5f - 0.75f;
		light.spot_position[index][2] = -2.0f;
		light.spot_radius[index] = 4.0f;
		light.spot_intensity[index] = 0.3f;
	}
	//默认：声明顺序（0）＋ 预算 0 → 前 8 个候选（全部点光）生效，聚光被淘汰
	renderer.scene_apply(default_camera(), light, empty_fog());
	EXPECT_EQ(renderer.point_light_count(), 8u);
	engine::Frame_Stats stats = renderer.frame_stats_get();
	EXPECT_EQ(stats.lights_active, 8u);
	EXPECT_EQ(stats.lights_dropped, 4u);
	EXPECT_EQ(stats.light_select_mode, 0u);
	EXPECT_EQ(stats.light_truncated, 1u);
	for (uint32_t slot = 0; slot < 8; ++slot)
		expect_state(renderer, slot, light.point_position[slot], 0.3f, false);

	//预算内的声明（6 盏）不产生淘汰、不置截断标志
	light.spot_count = 0;
	light.point_count = 6;
	renderer.scene_apply(default_camera(), light, empty_fog());
	EXPECT_EQ(renderer.point_light_count(), 6u);
	stats = renderer.frame_stats_get();
	EXPECT_EQ(stats.lights_dropped, 0u);
	EXPECT_EQ(stats.light_truncated, 0u);
}

//重要性选择：近距离强光优先入选，且同分同序（同输入重复应用结果一致）
TEST(Renderer_Light_Select_Test, 重要性选择与稳定性)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Light_Data light{};
	light.light_select_mode = 1;
	light.point_count = 8;
	light.spot_count = 8;
	//点光：远且弱（评分 = 0.1 × 5 /(5 ＋ 55) ≈ 0.0083）
	for (uint32_t index = 0; index < 8; ++index)
	{
		light.point_position[index][0] = 0.0f;
		light.point_position[index][1] = 0.0f;
		light.point_position[index][2] = -52.0f;
		light.point_radius[index] = 5.0f;
		light.point_intensity[index] = 0.1f;
	}
	//聚光 0..3：近且强、相机位于锥内（评分 ≈ 0.83）；聚光 4..7：远且背离相机（评分 0）
	for (uint32_t index = 0; index < 8; ++index)
	{
		light.spot_position[index][0] = 0.0f;
		light.spot_direction[index][1] = -1.0f;
		light.spot_direction[index][2] = 0.0f;
		light.spot_radius[index] = 10.0f;
		light.spot_intensity[index] = index < 4 ? 1.0f : 0.1f;
		light.spot_position[index][1] = index < 4 ? 2.0f : 2.0f;
		light.spot_position[index][2] = index < 4 ? 3.0f : -50.0f;
	}
	renderer.scene_apply(default_camera(), light, empty_fog());
	//命中：4 盏近强聚光 ＋ 4 盏点光（其余 8 条淘汰）
	EXPECT_EQ(renderer.point_light_count(), 8u);
	engine::Frame_Stats stats = renderer.frame_stats_get();
	EXPECT_EQ(stats.lights_active, 8u);
	EXPECT_EQ(stats.lights_dropped, 8u);
	EXPECT_EQ(stats.light_select_mode, 1u);
	//命中后仍按声明顺序登记：点光段在前（序号 0..3），聚光段在后（序号 0..3）
	for (uint32_t slot = 0; slot < 4; ++slot)
		expect_state(renderer, slot, light.point_position[slot], 0.1f, false);
	for (uint32_t slot = 4; slot < 8; ++slot)
		expect_state(renderer, slot, light.spot_position[slot - 4], 1.0f, true);

	//同输入重复应用：选中集合与顺序逐字段一致（不随帧抖动）
	engine::Point_Light_State first[8]{};
	for (uint32_t slot = 0; slot < 8; ++slot)
		ASSERT_TRUE(renderer.point_light_state_get(slot, first[slot]));
	renderer.scene_apply(default_camera(), light, empty_fog());
	for (uint32_t slot = 0; slot < 8; ++slot)
	{
		engine::Point_Light_State again{};
		ASSERT_TRUE(renderer.point_light_state_get(slot, again));
		for (int axis = 0; axis < 3; ++axis)
			EXPECT_FLOAT_EQ(again.position[axis], first[slot].position[axis]);
		EXPECT_FLOAT_EQ(again.intensity, first[slot].intensity);
		EXPECT_EQ(again.is_spot, first[slot].is_spot);
	}
	EXPECT_EQ(renderer.frame_stats_get().lights_dropped, 8u);
}

//预算截断：声明顺序模式下的预算取较小值；重要性模式预算同样生效
TEST(Renderer_Light_Select_Test, 预算截断与门面入口)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Light_Data light{};
	light.light_budget = 4;
	light.point_count = 5;
	for (uint32_t index = 0; index < 5; ++index)
	{
		light.point_position[index][0] = static_cast<float>(index) * 0.5f - 1.0f;
		light.point_position[index][2] = -2.0f;
		light.point_radius[index] = 4.0f;
		light.point_intensity[index] = 0.3f;
	}
	//声明顺序模式：预算 4 → 前 4 盏生效、淘汰 1 盏
	renderer.scene_apply(default_camera(), light, empty_fog());
	EXPECT_EQ(renderer.point_light_count(), 4u);
	engine::Frame_Stats stats = renderer.frame_stats_get();
	EXPECT_EQ(stats.lights_active, 4u);
	EXPECT_EQ(stats.lights_dropped, 1u);
	EXPECT_EQ(stats.light_truncated, 1u);

	//重要性模式 ＋ 预算 3 → 生效 3 盏、淘汰 2 盏
	light.light_select_mode = 1;
	light.light_budget = 3;
	renderer.scene_apply(default_camera(), light, empty_fog());
	EXPECT_EQ(renderer.point_light_count(), 3u);
	stats = renderer.frame_stats_get();
	EXPECT_EQ(stats.lights_active, 3u);
	EXPECT_EQ(stats.lights_dropped, 2u);
	EXPECT_EQ(stats.light_select_mode, 1u);

	//门面入口：模式仅接受 0 / 1（其它按 0）、预算钳到容量
	renderer.light_select_set(1, 2);
	EXPECT_EQ(renderer.frame_stats_get().light_select_mode, 1u);
	renderer.light_select_set(5, 5000);
	EXPECT_EQ(renderer.frame_stats_get().light_select_mode, 0u);
}
