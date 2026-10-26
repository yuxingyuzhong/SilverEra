/*
渲染相机测试（L2 隐藏窗口）
覆盖：相机位置 / 注视点 / 视场角 / 近远平面设置后推进帧不报错；极端视场角不崩溃；
      未初始化时设置相机为安全无操作。
*/
#include <gtest/gtest.h>

//获取渲染器门面与配置
#include "Application/Renderer/src/render/Renderer/渲染器.h"

namespace
{
	//构造隐藏窗口配置
	engine::Renderer_Config hidden_config(void)
	{
		engine::Renderer_Config config;
		config.width = 320;
		config.height = 180;
		config.hidden = true;
		return config;
	}
}

//相机测试夹具
class Renderer_Camera_Test : public ::testing::Test
{
};

//设置位置与注视点后推进一帧
TEST_F(Renderer_Camera_Test, 设置位置与注视点)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//把相机拉到前方
	renderer.camera_position_set(0.0f, 0.0f, 3.0f);
	//注视原点
	renderer.camera_target_set(0.0f, 0.0f, 0.0f);
	EXPECT_TRUE(renderer.frame_render());
}

//设置视场角与近远平面后推进一帧
TEST_F(Renderer_Camera_Test, 设置视场角与近远平面)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//约 60 度视场角
	renderer.camera_fov_set(1.0472f);
	//近远平面
	renderer.camera_near_far_set(0.1f, 100.0f);
	EXPECT_TRUE(renderer.frame_render());
}

//拉远拉近组合设置后连续多帧推进
TEST_F(Renderer_Camera_Test, 组合设置连续跑帧)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//连续五帧逐步拉近
	for (uint32_t index = 0; index < 5; ++index)
	{
		//逐帧把相机推近原点
		float distance = 5.0f - static_cast<float>(index);
		renderer.camera_position_set(0.0f, 0.0f, distance);
		renderer.camera_target_set(0.0f, 0.0f, 0.0f);
		EXPECT_TRUE(renderer.frame_render());
	}
}

//极端视场角设置后推进一帧不崩溃
TEST_F(Renderer_Camera_Test, 极端视场角不崩溃)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//很小的视场角
	renderer.camera_fov_set(0.1f);
	EXPECT_TRUE(renderer.frame_render());
	//很大的视场角
	renderer.camera_fov_set(3.0f);
	EXPECT_TRUE(renderer.frame_render());
}

//未初始化时各相机设置接口为安全无操作
TEST_F(Renderer_Camera_Test, 未初始化设置相机无副作用)
{
        engine::Renderer renderer;
        //未初始化时逐个调用各设置接口
        renderer.camera_position_set(1.0f, 2.0f, 3.0f);
        renderer.camera_target_set(0.0f, 0.0f, 0.0f);
        renderer.camera_fov_set(1.0f);
        renderer.camera_near_far_set(0.1f, 100.0f);
        //仍处于未初始化
        EXPECT_FALSE(renderer.valid());
}

/*
相机屏幕投影与反投影（第一百四十九轮）
口径：默认相机（位于 +Z 3、注视原点）：原点落在屏幕中心（0.5 / 0.5）且深度在 (0,1)；
      x = 1 的点落在右半（u > 0.5）；相机背后（z = 10）返回假且不改出参；
      unproject / project 往返误差 ≤ 1e-3；未初始化返回假。
*/
TEST_F(Renderer_Camera_Test, 相机屏幕投影与反投影)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//原点 → 屏幕中心
	const float origin[3] = { 0.0f, 0.0f, 0.0f };
	float u = -1.0f;
	float v = -1.0f;
	float depth = -1.0f;
	ASSERT_TRUE(renderer.camera_project(origin, &u, &v, &depth));
	EXPECT_NEAR(u, 0.5f, 1e-3f) << "原点未落在屏幕中心";
	EXPECT_NEAR(v, 0.5f, 1e-3f) << "原点未落在屏幕中心";
	EXPECT_GT(depth, 0.0f);
	EXPECT_LT(depth, 1.0f);
	//右侧点 → u > 0.5 且 v 仍居中
	const float right[3] = { 1.0f, 0.0f, 0.0f };
	ASSERT_TRUE(renderer.camera_project(right, &u, &v, nullptr));
	EXPECT_GT(u, 0.5f) << "右侧点未落在右半屏";
	EXPECT_NEAR(v, 0.5f, 1e-3f);
	//相机背后 → 假且出参不变
	const float behind[3] = { 0.0f, 0.0f, 10.0f };
	float untouched = 0.25f;
	EXPECT_FALSE(renderer.camera_project(behind, &untouched, nullptr, nullptr));
	EXPECT_FLOAT_EQ(untouched, 0.25f) << "失败时不应改写出参";
	//往返（屏幕 → 世界 → 屏幕）
	const float screen_u = 0.25f;
	const float screen_v = 0.75f;
	const float screen_depth = 0.6f;
	float world[3] = {};
	ASSERT_TRUE(renderer.camera_unproject(screen_u, screen_v, screen_depth, world));
	float round_u = 0.0f;
	float round_v = 0.0f;
	float round_depth = 0.0f;
	ASSERT_TRUE(renderer.camera_project(world, &round_u, &round_v, &round_depth));
	EXPECT_NEAR(round_u, screen_u, 1e-3f);
	EXPECT_NEAR(round_v, screen_v, 1e-3f);
	EXPECT_NEAR(round_depth, screen_depth, 1e-3f);
	//未初始化：两入口均返回假
	engine::Renderer uninitialized;
	EXPECT_FALSE(uninitialized.camera_project(origin, &u, &v, &depth));
	EXPECT_FALSE(uninitialized.camera_unproject(0.5f, 0.5f, 0.5f, world));
}
