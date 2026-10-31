/*
渲染光照查询测试（第九十二轮）
覆盖：点光源数量查询与逐盏状态快照（字段回读、越界拒绝、清空归零）。
*/
#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>

#include "Application/Renderer/src/render/Renderer/渲染器.h"

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
}

TEST(Renderer_Light_Query_Test, 点光源状态可查询)
{
	engine::Renderer renderer;
	//未初始化：数量为 0、读取失败
	EXPECT_EQ(renderer.point_light_count(), 0u);
	engine::Point_Light_State state{};
	EXPECT_FALSE(renderer.point_light_state_get(0, state));

	ASSERT_TRUE(renderer.init(hidden_config()));
	EXPECT_EQ(renderer.point_light_count(), 0u);

	//登记一盏普通点光源
	const float position[3] = { 1.0f, 2.0f, 3.0f };
	const float color[3] = { 0.2f, 0.4f, 0.6f };
	ASSERT_TRUE(renderer.light_add_point(position, color, 7.0f, 0.5f, 0.1f, 1, 0.8f, 32.0f));
	EXPECT_EQ(renderer.point_light_count(), 1u);

	ASSERT_TRUE(renderer.point_light_state_get(0, state));
	for (int axis = 0; axis < 3; ++axis)
	{
		EXPECT_FLOAT_EQ(state.position[axis], position[axis]);
		EXPECT_FLOAT_EQ(state.color[axis], color[axis]);
	}
	EXPECT_FLOAT_EQ(state.radius, 7.0f);
	EXPECT_FLOAT_EQ(state.intensity, 0.5f);
	EXPECT_EQ(state.attenuation, 1u);
	EXPECT_FLOAT_EQ(state.specular_scale, 0.8f);
	EXPECT_FLOAT_EQ(state.shininess, 32.0f);
	EXPECT_FALSE(state.is_spot);

	//越界读取被拒
	EXPECT_FALSE(renderer.point_light_state_get(1, state));

	//清空后归零
	renderer.lights_clear();
	EXPECT_EQ(renderer.point_light_count(), 0u);
	EXPECT_FALSE(renderer.point_light_state_get(0, state));
}

//光源记录快照与有效数截断（第二百六十五轮；阶段 D 数据底座）
TEST(Renderer_Light_Query_Test, 光源记录快照)
{
	engine::Renderer renderer;
	//未初始化：总数为 0、读取返回 0
	EXPECT_EQ(renderer.light_record_count(), 0u);
	engine::Light_Record records[12];
	EXPECT_EQ(renderer.light_records_get(records, 12), 0u);
	ASSERT_TRUE(renderer.init(hidden_config()));
	//初始两条：主方向光 ＋ 补光
	EXPECT_EQ(renderer.light_record_count(), 2u);
	EXPECT_EQ(renderer.light_records_get(records, 12), 2u);
	EXPECT_EQ(records[0].type, static_cast<uint32_t>(engine::Light_Type::Directional));
	EXPECT_FLOAT_EQ(records[0].color_intensity[3], 1.0f);
	EXPECT_FLOAT_EQ(records[0].direction[2], -1.0f);
	EXPECT_EQ(records[1].type, static_cast<uint32_t>(engine::Light_Type::Directional));
	EXPECT_EQ(records[1].flags, 0u);
	//追加一盏点光与一盏聚光
	const float point_position[3] = { 1.0f, 2.0f, 3.0f };
	const float point_color[3] = { 0.2f, 0.4f, 0.6f };
	const float spot_direction[3] = { 0.0f, -1.0f, 0.0f };
	ASSERT_TRUE(renderer.light_add_point(point_position, point_color, 7.0f, 0.5f, 0.1f, 1, 0.8f, 32.0f));
	ASSERT_TRUE(renderer.light_add_spot(point_position, spot_direction, point_color, 5.0f, 0.6f, 10.0f, 20.0f, 0.0f));
	EXPECT_EQ(renderer.light_record_count(), 4u);
	EXPECT_EQ(renderer.light_records_get(records, 12), 4u);
	//点光记录
	for (int axis = 0; axis < 3; ++axis)
	{
		EXPECT_FLOAT_EQ(records[2].pos_radius[axis], point_position[axis]);
		EXPECT_FLOAT_EQ(records[2].color_intensity[axis], point_color[axis]);
	}
	EXPECT_FLOAT_EQ(records[2].pos_radius[3], 7.0f);
	EXPECT_FLOAT_EQ(records[2].color_intensity[3], 0.5f);
	EXPECT_FLOAT_EQ(records[2].cone[2], 1.0f);
	EXPECT_FLOAT_EQ(records[2].cone[3], 0.8f);
	EXPECT_FLOAT_EQ(records[2].shininess, 32.0f);
	EXPECT_EQ(records[2].type, static_cast<uint32_t>(engine::Light_Type::Point));
	//聚光记录（方向已归一化；锥角余弦）
	EXPECT_EQ(records[3].type, static_cast<uint32_t>(engine::Light_Type::Spot));
	EXPECT_FLOAT_EQ(records[3].direction[1], -1.0f);
	EXPECT_NEAR(records[3].cone[0], std::cos(10.0f * 0.017453292f), 1e-4f);
	EXPECT_NEAR(records[3].cone[1], std::cos(20.0f * 0.017453292f), 1e-4f);
	//容量截断：容量 3 只写 3 条
	EXPECT_EQ(renderer.light_records_get(records, 3), 3u);
	//空指针返回 0
	EXPECT_EQ(renderer.light_records_get(nullptr, 3), 0u);
	//有效数截断（Cluster 关闭 → 8；开启 → 全量）
	EXPECT_EQ(engine::light_effective_count(12u, false), 8u);
	EXPECT_EQ(engine::light_effective_count(12u, true), 12u);
	EXPECT_EQ(engine::light_effective_count(3u, false), 3u);
}

