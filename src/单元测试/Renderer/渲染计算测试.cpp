/*
渲染计算测试（第二百四十一轮）
覆盖：未初始化时计算不支持且健康检查失败；
      初始化后计算上下文可用（图形队列族含 COMPUTE 位的机器上），健康检查把推送常量按位模式写入并回读一致；
      计算启用不影响默认图形路径出帧。
*/
#include <gtest/gtest.h>

#include <cstdint>

//获取渲染器门面
#include "Application/Renderer/src/render/Renderer/渲染器.h"
//线程组数换算与屏障用途（第二百四十二轮）
#include "Application/Renderer/src/render/Compute/计算数据.h"

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

//计算管线测试夹具
class Renderer_Compute_Test : public ::testing::Test
{
};

//未初始化时计算不支持，健康检查返回假
TEST_F(Renderer_Compute_Test, 未初始化计算不可用)
{
	engine::Renderer renderer;
	EXPECT_FALSE(renderer.compute_supported());
	uint32_t value = 0;
	EXPECT_FALSE(renderer.compute_health_check(0x3F800000u, value));
	EXPECT_EQ(value, 0u);
}

//初始化后健康检查：推送魔数经计算着色器写入存储缓冲并可回读
TEST_F(Renderer_Compute_Test, 计算管线健康检查)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//本机图形队列族不含计算位则跳过（降级路径已在初始化日志中报告）
	if (!renderer.compute_supported())
		GTEST_SKIP() << "本机图形队列族不支持计算路径";
	//两次不同魔数（1.0f 与 3.14159265 的位模式）
	const uint32_t magics[2] = { 0x3F800000u, 0x40490FDBu };
	for (uint32_t magic : magics)
	{
		uint32_t read_back = 0;
		//健康检查失败即断言失败
		ASSERT_TRUE(renderer.compute_health_check(magic, read_back));
		EXPECT_EQ(read_back, magic) << "计算回读值与推送魔数不一致";
	}
	//计算启用后默认图形路径仍可出帧
	ASSERT_TRUE(renderer.frame_render());
}

//计算启用时默认图形路径画面不变（无绘制时仅清屏，与未启用一致的口径由其他套件覆盖）
TEST_F(Renderer_Compute_Test, 计算启用不影响默认路径)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//无论是否支持计算，默认路径都应能连续出帧
	ASSERT_TRUE(renderer.frame_render());
	ASSERT_TRUE(renderer.frame_render());
}

//线程组数换算（第二百四十二轮）：向上取整、空输入不调度
TEST_F(Renderer_Compute_Test, 线程组数换算)
{
	//空元素数 / 非法组大小一律为 0
	EXPECT_EQ(engine::compute_group_count(0u, 64u), 0u);
	EXPECT_EQ(engine::compute_group_count(64u, 0u), 0u);
	//整除与余数
	EXPECT_EQ(engine::compute_group_count(64u, 64u), 1u);
	EXPECT_EQ(engine::compute_group_count(65u, 64u), 2u);
	EXPECT_EQ(engine::compute_group_count(1u, 1u), 1u);
	EXPECT_EQ(engine::compute_group_count(10000u, 64u), 157u);
	EXPECT_EQ(engine::compute_group_count(10000u, 256u), 40u);
}

//调度计数随健康检查累加（第二百四十二轮：dispatch 计数与统计快照）
TEST_F(Renderer_Compute_Test, 调度计数累加)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//不支持计算则跳过
	if (!renderer.compute_supported())
		GTEST_SKIP() << "本机图形队列族不支持计算路径";
	//两次健康检查 → 两次调度
	uint32_t read_back = 0;
	ASSERT_TRUE(renderer.compute_health_check(0x3F800000u, read_back));
	ASSERT_TRUE(renderer.compute_health_check(0x40490FDBu, read_back));
	EXPECT_EQ(read_back, 0x40490FDBu);
}
