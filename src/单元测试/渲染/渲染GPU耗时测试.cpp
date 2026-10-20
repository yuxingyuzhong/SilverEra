/*
渲染 GPU 耗时测试（L2 隐藏窗口）
覆盖：未初始化不支持 GPU 时间戳；设备支持时「渲染若干帧后 gpu_frame_ms > 0」（含空帧清屏）；
      设备不支持时三个 GPU 字段恒 0（优雅降级）——**不跳过**，两种分支各自给出可构造断言（处置 TEST-68）。
断言不断言具体耗时数值（机器相关），只断言「支持时 GPU 帧耗时 > 0」与「不支持时字段恒 0」。
说明：GPU 时间戳结果在「同一帧槽下一次使用」时读回，故用例渲染帧数须不少于「在途帧数 + 1」。
*/
#include <gtest/gtest.h>

#include <cstdint>

//获取渲染器门面（含帧统计结构）与网格数据
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

	//构造三角形网格（三顶点、三索引）
	engine::Mesh_Data triangle_data(void)
	{
		engine::Mesh_Data data;
		data.vertices = {
			{ { 0.0f, 0.5f, 0.0f }, { 0.5f, 0.0f } },
			{ { 0.5f, -0.5f, 0.0f }, { 1.0f, 1.0f } },
			{ { -0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f } },
		};
		data.indices = { 0, 1, 2 };
		return data;
	}
}

//GPU 耗时测试夹具
class Renderer_Gpu_Test : public ::testing::Test
{
};

//未初始化时不支持 GPU 时间戳，统计字段为 0
TEST_F(Renderer_Gpu_Test, 未初始化不支持GPU时间戳)
{
	engine::Renderer renderer;
	EXPECT_FALSE(renderer.gpu_timestamp_supported());
	engine::Frame_Stats stats = renderer.frame_stats_get();
	EXPECT_EQ(stats.gpu_frame_ms, 0.0);
}

/*
绘制帧的 GPU 耗时：支持时大于零；不支持时三个 GPU 字段恒 0（不跳过，两分支均断言）
连续渲染 4 帧（在途帧数默认 2，故首个结果可被读回）。
*/
TEST_F(Renderer_Gpu_Test, 绘制帧GPU耗时随能力分支断言)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, triangle_data()));
	for (uint32_t index = 0; index < 4; ++index)
	{
		ASSERT_TRUE(renderer.mesh_draw(mesh, nullptr));
		ASSERT_TRUE(renderer.frame_render());
	}
	engine::Frame_Stats stats = renderer.frame_stats_get();
	EXPECT_EQ(stats.frames, 4u);
	//绘制项与实例数照常累计（与 GPU 能力无关）
	EXPECT_EQ(stats.draw_items, 4u);
	EXPECT_EQ(stats.instances, 4u);
	//若支持 GPU 时间戳
	if (renderer.gpu_timestamp_supported())
	{
		EXPECT_GT(stats.gpu_frame_ms, 0.0);
		EXPECT_EQ(stats.gpu_samples, 2u);
	}
	//不支持则三个字段恒 0（优雅降级，不作为失败）
	else
	{
		EXPECT_EQ(stats.gpu_frame_ms, 0.0);
		EXPECT_EQ(stats.gpu_total_ms, 0.0);
		EXPECT_EQ(stats.gpu_samples, 0u);
	}
	renderer.mesh_unload(mesh);
}

//空帧（仅清屏）绘制的 GPU 耗时：支持时大于零；不支持时三个 GPU 字段恒 0（不跳过）
TEST_F(Renderer_Gpu_Test, 空帧GPU耗时随能力分支断言)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//不登记任何绘制项，仅执行清屏与呈现
	for (uint32_t index = 0; index < 4; ++index)
		ASSERT_TRUE(renderer.frame_render());
	engine::Frame_Stats stats = renderer.frame_stats_get();
	EXPECT_EQ(stats.draw_items, 0u);
	EXPECT_EQ(stats.instances, 0u);
	//若支持 GPU 时间戳
	if (renderer.gpu_timestamp_supported())
	{
		EXPECT_GT(stats.gpu_frame_ms, 0.0);
		EXPECT_EQ(stats.gpu_samples, 2u);
	}
	//不支持则三个字段恒 0
	else
	{
		EXPECT_EQ(stats.gpu_frame_ms, 0.0);
		EXPECT_EQ(stats.gpu_total_ms, 0.0);
		EXPECT_EQ(stats.gpu_samples, 0u);
	}
}
