/*
渲染帧测试（L2 隐藏窗口）
覆盖：连续 60 帧推进、帧计数与成功帧数一致、在途帧数 1/2/3 均可跑帧、
      关闭垂直同步可跑帧、关闭后推进失败。
说明：隐藏窗口仍创建真实窗口与表面，故需真实 Vulkan 设备与显示环境。
*/
#include <gtest/gtest.h>

//获取渲染器门面与配置
#include "Application/Renderer/src/render/Renderer/渲染器.h"

namespace
{
	//构造隐藏窗口配置
	engine::Renderer_Config hidden_config(uint32_t frame_overlap, bool vsync)
	{
		engine::Renderer_Config config;
		config.width = 320;
		config.height = 180;
		config.hidden = true;
		config.frame_overlap = frame_overlap;
		config.vsync = vsync;
		return config;
	}
}

//渲染帧测试夹具
class Renderer_Frame_Test : public ::testing::Test
{
};

//连续推进 60 帧全部成功，帧计数与成功帧数一致
TEST_F(Renderer_Frame_Test, 连续六十帧推进成功)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(2, true))) << "隐藏窗口初始化失败";
	//成功推进的帧数
	uint64_t expected = 0;
	//连续推进 60 帧
	for (uint32_t index = 0; index < 60; ++index)
	{
		//若本帧成功则计数自增，否则记失败
		if (renderer.frame_render())
			++expected;
		else
			ADD_FAILURE() << "第 " << index << " 帧推进失败";
	}
	//帧计数等于成功帧数
	EXPECT_EQ(renderer.frame_count(), expected);
	//且等于 60
	EXPECT_EQ(renderer.frame_count(), 60u);
}

//在途帧数为 1（单缓冲）时可跑帧
TEST_F(Renderer_Frame_Test, 单缓冲可跑帧)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(1, true)));
	for (uint32_t index = 0; index < 10; ++index)
		EXPECT_TRUE(renderer.frame_render());
}

//在途帧数为 3（三缓冲）时可跑帧
TEST_F(Renderer_Frame_Test, 三缓冲可跑帧)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(3, true)));
	for (uint32_t index = 0; index < 10; ++index)
		EXPECT_TRUE(renderer.frame_render());
}

//关闭垂直同步（立即呈现模式）时可跑帧
TEST_F(Renderer_Frame_Test, 关闭垂直同步可跑帧)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(2, false)));
	for (uint32_t index = 0; index < 10; ++index)
		EXPECT_TRUE(renderer.frame_render());
}

//关闭后推进一帧失败
TEST_F(Renderer_Frame_Test, 关闭后推进失败)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(2, true)));
	renderer.shutdown();
	//关闭后推进应失败
	EXPECT_FALSE(renderer.frame_render());
}
