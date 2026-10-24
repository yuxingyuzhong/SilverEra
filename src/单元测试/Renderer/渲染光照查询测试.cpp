/*
渲染光照查询测试（第九十二轮）
覆盖：点光源数量查询与逐盏状态快照（字段回读、越界拒绝、清空归零）。
*/
#include <gtest/gtest.h>

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
