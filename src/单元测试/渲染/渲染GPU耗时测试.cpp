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

/*
空帧（仅清屏）绘制的 GPU 耗时：支持时大于零；不支持时三个 GPU 字段恒 0（不跳过）
*/
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

/*
两级后处理链路径的 GPU 耗时旁证（处置 R-FRAME-11 的链路径口径）
断言：①链路径的绘制统计包含场景项 ＋ 各级后处理项；②支持时 gpu_frame_ms > 0 且样本数符合
「帧数 − 在途帧数」的滞后口径；③不支持时三字段恒 0。
**不比较**「两级 ≥ 一级」的耗时大小（换机 / 驱动波动会假失败）；多 pass 覆盖由终点时间戳位于
收尾转换之后保证（源码口径），本用例只钉住「链路径确实产生样本」。
*/
TEST_F(Renderer_Gpu_Test, 两级链GPU耗时随能力分支断言)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//场景目标与第二目标（与交换链同尺寸，满足后处理 1:1 取源）
	uint64_t scene_target = 0;
	uint64_t second_target = 0;
	ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
	ASSERT_TRUE(renderer.render_target_create(second_target, renderer.extent_width(), renderer.extent_height()));
	//登记两条后处理管线并设两级链
	uint64_t invert = 0;
	uint64_t threshold = 0;
	ASSERT_TRUE(renderer.pipeline_load(invert, "后处理.vert.spv", "反色.frag.spv"));
	ASSERT_TRUE(renderer.pipeline_load(threshold, "后处理.vert.spv", "亮度阈值.frag.spv"));
	uint64_t chain[2] = { invert, threshold };
	ASSERT_TRUE(renderer.post_chain_set(chain, 2));
	//设定同帧后处理呈现目标组（场景目标 ＋ 中间目标）
	uint64_t chain_targets[2] = { scene_target, second_target };
	ASSERT_TRUE(renderer.render_target_present_chain_set(chain_targets, 2));
	//登记场景绘制
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, triangle_data()));
	for (uint32_t index = 0; index < 4; ++index)
	{
		ASSERT_TRUE(renderer.mesh_draw(mesh, nullptr));
		ASSERT_TRUE(renderer.frame_render());
	}
	engine::Frame_Stats stats = renderer.frame_stats_get();
	EXPECT_EQ(stats.frames, 4u);
	//链路径：每帧 1 个场景项 ＋ 2 级后处理项
	EXPECT_EQ(stats.draw_items, 12u);
	EXPECT_EQ(stats.instances, 12u);
	EXPECT_EQ(stats.drawn_items, stats.draw_items) << "链路径存在被跳过的后处理项";
	EXPECT_EQ(stats.drawn_instances, stats.instances);
	//若支持 GPU 时间戳
	if (renderer.gpu_timestamp_supported())
	{
		EXPECT_GT(stats.gpu_frame_ms, 0.0) << "链路径未产生 GPU 耗时（时间戳未覆盖全部 pass）";
		EXPECT_EQ(stats.gpu_samples, 2u) << "链路径样本数须符合「帧数 − 在途帧数」";
	}
	//不支持则三字段恒 0
	else
	{
		EXPECT_EQ(stats.gpu_frame_ms, 0.0);
		EXPECT_EQ(stats.gpu_total_ms, 0.0);
		EXPECT_EQ(stats.gpu_samples, 0u);
	}
	renderer.mesh_unload(mesh);
	renderer.pipeline_unload(invert);
	renderer.pipeline_unload(threshold);
	renderer.render_target_destroy(second_target);
	renderer.render_target_destroy(scene_target);
}
