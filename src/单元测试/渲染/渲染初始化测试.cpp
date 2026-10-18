/*
渲染初始化测试（L1 无窗口）
覆盖：渲染层版本查询、未初始化默认态、非法配置被拒（宽/高为零、在途帧数越界）、
      隐藏窗口正常初始化、重复初始化幂等、关闭幂等与关闭后重初始化、未初始化时的公共方法守卫。
说明：本层只按渲染层公共面（渲染器.h）编写用例，不引入任何 Vulkan / SDL 类型。
*/
#include <gtest/gtest.h>

//获取渲染器门面与配置
#include "Application/Renderer/src/render/Renderer/渲染器.h"

namespace
{
	//构造隐藏窗口配置（尺寸取小以免拖慢构建后运行）
	engine::Renderer_Config hidden_config(uint32_t width = 320, uint32_t height = 180)
	{
		engine::Renderer_Config config;
		config.width = width;
		config.height = height;
		config.hidden = true;
		return config;
	}
}

//渲染初始化测试夹具
class Renderer_Init_Test : public ::testing::Test
{
};

//版本查询返回非空
TEST_F(Renderer_Init_Test, 版本查询非空)
{
	//渲染层版本号
	const std::string version = engine::renderer_version();
	//应为非空字符串
	EXPECT_FALSE(version.empty());
}

//默认构造后处于未初始化状态，各查询接口给出零值
TEST_F(Renderer_Init_Test, 默认构造未初始化)
{
	//未初始化的渲染器
	engine::Renderer renderer;
	//未初始化
	EXPECT_FALSE(renderer.valid());
	//帧计数为零
	EXPECT_EQ(renderer.frame_count(), 0u);
	//交换链尺寸为零
	EXPECT_EQ(renderer.extent_width(), 0u);
	EXPECT_EQ(renderer.extent_height(), 0u);
	//未初始化时推进一帧失败
	EXPECT_FALSE(renderer.frame_render());
	//未初始化时窗口不请求退出
	EXPECT_FALSE(renderer.close_requested());
}

//宽度为零的配置被拒绝
TEST_F(Renderer_Init_Test, 宽度为零被拒绝)
{
	engine::Renderer renderer;
	//用宽为零的配置初始化
	EXPECT_FALSE(renderer.init(hidden_config(0, 180)));
	//仍处于未初始化
	EXPECT_FALSE(renderer.valid());
}

//高度为零的配置被拒绝
TEST_F(Renderer_Init_Test, 高度为零被拒绝)
{
	engine::Renderer renderer;
	//用高为零的配置初始化
	EXPECT_FALSE(renderer.init(hidden_config(320, 0)));
	EXPECT_FALSE(renderer.valid());
}

//在途帧数为零被拒绝（下界越界）
TEST_F(Renderer_Init_Test, 在途帧数为零被拒绝)
{
	engine::Renderer renderer;
	//在途帧数为零的配置
	engine::Renderer_Config config = hidden_config();
	config.frame_overlap = 0;
	EXPECT_FALSE(renderer.init(config));
	EXPECT_FALSE(renderer.valid());
}

//在途帧数超上界（4）被拒绝
TEST_F(Renderer_Init_Test, 在途帧数越界被拒绝)
{
	engine::Renderer renderer;
	//在途帧数为 4 的配置
	engine::Renderer_Config config = hidden_config();
	config.frame_overlap = 4;
	EXPECT_FALSE(renderer.init(config));
	EXPECT_FALSE(renderer.valid());
}

//隐藏窗口可正常初始化，关闭幂等，并可在关闭后重新初始化
TEST_F(Renderer_Init_Test, 隐藏窗口初始化与幂等关闭)
{
	engine::Renderer renderer;
	//初始化隐藏窗口
	ASSERT_TRUE(renderer.init(hidden_config())) << "隐藏窗口初始化失败";
	//已初始化
	EXPECT_TRUE(renderer.valid());
	//交换链尺寸非零
	EXPECT_GT(renderer.extent_width(), 0u);
	EXPECT_GT(renderer.extent_height(), 0u);
	//隐藏窗口不请求退出
	EXPECT_FALSE(renderer.close_requested());
	//首次关闭
	renderer.shutdown();
	EXPECT_FALSE(renderer.valid());
	//重复关闭不报错（幂等）
	renderer.shutdown();
	EXPECT_FALSE(renderer.valid());
	//关闭后重新初始化
	ASSERT_TRUE(renderer.init(hidden_config())) << "关闭后重新初始化失败";
	EXPECT_TRUE(renderer.valid());
}

//已初始化时重复 init 视为幂等成功
TEST_F(Renderer_Init_Test, 重复初始化幂等)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//重复初始化应返回真且不破坏已初始化状态
	EXPECT_TRUE(renderer.init(hidden_config()));
	EXPECT_TRUE(renderer.valid());
}
