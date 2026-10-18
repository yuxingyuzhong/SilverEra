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
