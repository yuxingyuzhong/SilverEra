/*
渲染Cluster光照测试（第二百六十六轮）
覆盖：光源记录 SSBO 上传（未初始化拒绝 / 默认关闭不上传 / 开启后按帧上传 / 光源增减即时反映 / 关闭归零）。
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

TEST(Renderer_Cluster_Test, 光源记录上传与开关)
{
	engine::Renderer renderer;
	//未初始化：能力与开关均为假、计数为 0、启用被拒
	EXPECT_FALSE(renderer.cluster_supported());
	EXPECT_FALSE(renderer.cluster_enable_get());
	EXPECT_EQ(renderer.cluster_light_count(), 0u);
	EXPECT_FALSE(renderer.cluster_enable_set(true));

	ASSERT_TRUE(renderer.init(hidden_config()));
	//默认关闭：不创建资源、不上传
	EXPECT_FALSE(renderer.cluster_enable_get());
	EXPECT_EQ(renderer.cluster_light_count(), 0u);
	//本机不支持计算路径时 Cluster 保持关闭（与 GPU 路径降级口径一致）
	if (!renderer.cluster_supported())
	{
		GTEST_SKIP() << "本机不支持计算路径，Cluster 光照保持关闭";
	}

	//开启后首帧上传主方向光 ＋ 补光两条记录
	ASSERT_TRUE(renderer.cluster_enable_set(true));
	EXPECT_TRUE(renderer.cluster_enable_get());
	ASSERT_TRUE(renderer.frame_render());
	EXPECT_EQ(renderer.cluster_light_count(), 2u);

	//追加一点光与一聚光后，下一帧上传 4 条
	const float point_position[3] = { 1.0f, 2.0f, 3.0f };
	const float point_color[3] = { 0.2f, 0.4f, 0.6f };
	const float spot_direction[3] = { 0.0f, -1.0f, 0.0f };
	ASSERT_TRUE(renderer.light_add_point(point_position, point_color, 7.0f, 0.5f, 0.1f, 1, 0.8f, 32.0f));
	ASSERT_TRUE(renderer.light_add_spot(point_position, spot_direction, point_color, 7.0f, 0.5f, 30.0f, 45.0f, 0.1f));
	ASSERT_TRUE(renderer.frame_render());
	EXPECT_EQ(renderer.cluster_light_count(), 4u);

	//清空光源后回到两条基础记录
	renderer.lights_clear();
	ASSERT_TRUE(renderer.frame_render());
	EXPECT_EQ(renderer.cluster_light_count(), 2u);

	//关闭后计数归零、开关为假
	ASSERT_TRUE(renderer.cluster_enable_set(false));
	EXPECT_FALSE(renderer.cluster_enable_get());
	EXPECT_EQ(renderer.cluster_light_count(), 0u);
}
