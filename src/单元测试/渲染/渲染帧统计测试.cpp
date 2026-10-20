/*
渲染帧统计测试（L2 隐藏窗口）
覆盖：未初始化取帧统计为零；空帧（无绘制项）的帧数推进与帧时间；
      绘制项数与实例数累计与登记项一致（逐次绘制 / 实例化绘制混合）；
      累计口径（frame_total_ms 单调不减且等于逐帧快照之和）与 GPU 样本数口径
      （支持时 gpu_samples = 帧数 − 在途帧数；不支持时两字段恒 0 的优雅降级）。
断言不断言具体耗时数值（机器相关），只断言「帧时间 > 0」「计数与登记项一致」与「累计 / 样本数关系」。
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

//渲染帧统计测试夹具
class Renderer_Stats_Test : public ::testing::Test
{
};

//未初始化时取帧统计，各项为零
TEST_F(Renderer_Stats_Test, 未初始化取帧统计为零)
{
	engine::Renderer renderer;
	engine::Frame_Stats stats = renderer.frame_stats_get();
	EXPECT_EQ(stats.frames, 0u);
	EXPECT_EQ(stats.draw_items, 0u);
	EXPECT_EQ(stats.instances, 0u);
	EXPECT_EQ(stats.frame_ms, 0.0);
}

//空帧（无绘制项）：帧数推进、绘制计数为零、帧时间大于零
TEST_F(Renderer_Stats_Test, 空帧计数为零且帧时间大于零)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//不登记任何绘制项，推进一帧
	ASSERT_TRUE(renderer.frame_render());
	engine::Frame_Stats stats = renderer.frame_stats_get();
	EXPECT_EQ(stats.frames, 1u);
	EXPECT_EQ(stats.draw_items, 0u);
	EXPECT_EQ(stats.instances, 0u);
	EXPECT_GT(stats.frame_ms, 0.0);
}

//绘制计数与登记项一致：逐次绘制项与实例化绘制项混合
TEST_F(Renderer_Stats_Test, 绘制计数与登记项一致)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, triangle_data()));
	//初始统计为零
	engine::Frame_Stats initial = renderer.frame_stats_get();
	EXPECT_EQ(initial.frames, 0u);
	EXPECT_EQ(initial.draw_items, 0u);
	EXPECT_EQ(initial.instances, 0u);

	//第一帧：登记一个逐次绘制项（1 实例）
	ASSERT_TRUE(renderer.mesh_draw(mesh, nullptr));
	ASSERT_TRUE(renderer.frame_render());
	engine::Frame_Stats after_first = renderer.frame_stats_get();
	EXPECT_EQ(after_first.frames, 1u);
	EXPECT_EQ(after_first.draw_items, 1u);
	EXPECT_EQ(after_first.instances, 1u);
	EXPECT_GT(after_first.frame_ms, 0.0);

	//第二帧：登记一个 3 实例的实例化绘制项
	ASSERT_TRUE(renderer.mesh_draw_instanced(mesh, nullptr, 3));
	ASSERT_TRUE(renderer.frame_render());
	engine::Frame_Stats after_second = renderer.frame_stats_get();
	EXPECT_EQ(after_second.frames, 2u);
	EXPECT_EQ(after_second.draw_items, 2u);
	EXPECT_EQ(after_second.instances, 4u);
	EXPECT_GT(after_second.frame_ms, 0.0);

	renderer.mesh_unload(mesh);
}

//累计主机侧耗时单调不减，且等于逐帧快照 frame_ms 之和
TEST_F(Renderer_Stats_Test, 累计主机侧耗时单调不减)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, triangle_data()));
	//上一帧的累计值（用于单调性比较）
	double previous_total = 0.0;
	//逐帧快照 frame_ms 之和
	double sum_frame_ms = 0.0;
	for (uint32_t index = 0; index < 5; ++index)
	{
		ASSERT_TRUE(renderer.mesh_draw(mesh, nullptr));
		ASSERT_TRUE(renderer.frame_render());
		engine::Frame_Stats stats = renderer.frame_stats_get();
		//累计值不得回退
		EXPECT_GE(stats.frame_total_ms, previous_total) << "第 " << index << " 帧累计耗时回退";
		//累计本帧快照耗时
		sum_frame_ms += stats.frame_ms;
		previous_total = stats.frame_total_ms;
	}
	engine::Frame_Stats stats = renderer.frame_stats_get();
	EXPECT_EQ(stats.frames, 5u);
	EXPECT_GT(stats.frame_total_ms, 0.0);
	//累计值恰为各帧快照之和（同一累加顺序，故按位相等）
	EXPECT_DOUBLE_EQ(stats.frame_total_ms, sum_frame_ms);
	renderer.mesh_unload(mesh);
}

//GPU 样本数与帧数关系符合「按帧槽环形读回」口径；不支持时两字段恒 0（优雅降级）
TEST_F(Renderer_Stats_Test, GPU样本数与帧数关系符合口径)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//在途帧数（配置默认 2）
	constexpr uint64_t OVERLAP = 2;
	//连续渲染 6 帧（帧槽首轮未写时间戳，故可读回样本数为 帧数 − 在途帧数）
	constexpr uint64_t FRAMES = 6;
	for (uint64_t index = 0; index < FRAMES; ++index)
		ASSERT_TRUE(renderer.frame_render());
	engine::Frame_Stats stats = renderer.frame_stats_get();
	EXPECT_EQ(stats.frames, FRAMES);
	//若设备支持 GPU 时间戳
	if (renderer.gpu_timestamp_supported())
	{
		EXPECT_EQ(stats.gpu_samples, FRAMES - OVERLAP);
		EXPECT_GT(stats.gpu_total_ms, 0.0);
	}
	//不支持则三个 GPU 字段恒 0（不报错、不影响渲染）
	else
	{
		EXPECT_EQ(stats.gpu_samples, 0u);
		EXPECT_EQ(stats.gpu_total_ms, 0.0);
		EXPECT_EQ(stats.gpu_frame_ms, 0.0);
	}
}
