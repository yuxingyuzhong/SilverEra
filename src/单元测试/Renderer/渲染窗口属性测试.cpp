/*
渲染层窗口属性测试（第一百一十一～一百一十二轮）
覆盖：全屏切换与查询（后端拒绝时仅验证状态一致）、显示缩放比例查询。
*/
#include <gtest/gtest.h>

//获取渲染器门面
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

//窗口属性测试夹具
class Renderer_Window_Test : public ::testing::Test
{
};

//全屏切换：成功时状态一致；失败时不改变状态
TEST_F(Renderer_Window_Test, 全屏切换与查询)
{
	engine::Renderer renderer;
	//未初始化守卫
	EXPECT_FALSE(renderer.window_fullscreen_set(true));
	EXPECT_FALSE(renderer.window_fullscreen());

	ASSERT_TRUE(renderer.init(hidden_config()));
	//初始非全屏
	EXPECT_FALSE(renderer.window_fullscreen());
	//尝试进入全屏（隐藏窗口下可能被后端拒绝）
	bool entered = renderer.window_fullscreen_set(true);
	if (entered)
		EXPECT_TRUE(renderer.window_fullscreen());
	//退出全屏后必须为非全屏
	EXPECT_TRUE(renderer.window_fullscreen_set(false));
	EXPECT_FALSE(renderer.window_fullscreen());
}

//显示缩放：正数且不小于 1（未初始化时回落 1）
TEST_F(Renderer_Window_Test, 显示缩放查询)
{
	engine::Renderer renderer;
	EXPECT_FLOAT_EQ(renderer.display_scale(), 1.0f);
	ASSERT_TRUE(renderer.init(hidden_config()));
	float scale = renderer.display_scale();
	EXPECT_GT(scale, 0.0f);
	EXPECT_GE(scale, 1.0f);
}
