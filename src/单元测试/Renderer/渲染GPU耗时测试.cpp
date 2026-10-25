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

	//构造带法线的四边形（阴影用例的接收面 / 遮挡面）
	engine::Mesh_Data normal_quad(void)
	{
		engine::Mesh_Data data;
		data.vertices = {
			{ { -1.0f, -1.0f, 0.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f } },
			{ { 1.0f, -1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, -1.0f } },
			{ { 1.0f, 1.0f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, -1.0f } },
			{ { -1.0f, 1.0f, 0.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f } },
		};
		data.indices = { 0, 1, 2, 0, 2, 3 };
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

/*
直通路径的 GPU 分解（第一百二十八轮）
无后处理时：场景耗时 > 0、后处理耗时约 0、阴影耗时 0，且场景耗时不超过总耗时。
设备不支持时间戳时三项恒 0（不跳过）。
*/
TEST_F(Renderer_Gpu_Test, 直通路径GPU分解)
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
	if (renderer.gpu_timestamp_supported())
	{
		EXPECT_GT(stats.gpu_scene_ms, 0.0) << "场景阶段未产生 GPU 耗时";
		EXPECT_LE(stats.gpu_post_ms, 0.05) << "直通路径不应有后处理耗时";
		EXPECT_LE(stats.gpu_scene_ms, stats.gpu_frame_ms + 0.01) << "场景耗时超过总耗时";
	}
	else
	{
		EXPECT_EQ(stats.gpu_scene_ms, 0.0);
		EXPECT_EQ(stats.gpu_post_ms, 0.0);
	}
	EXPECT_EQ(stats.gpu_shadow_ms, 0.0) << "未启用阴影时阴影耗时须为 0";
	renderer.mesh_unload(mesh);
}

/*
后处理链的 GPU 分解（第一百二十八轮）
单级后处理链：场景与后处理各有非零耗时，两者之和不超过总耗时。
*/
TEST_F(Renderer_Gpu_Test, 后处理链GPU分解)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	uint64_t scene_target = 0;
	ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
	uint64_t invert = 0;
	ASSERT_TRUE(renderer.pipeline_load(invert, "后处理.vert.spv", "反色.frag.spv"));
	ASSERT_TRUE(renderer.post_chain_set(&invert, 1));
	renderer.render_target_present_set(scene_target);
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, triangle_data()));
	for (uint32_t index = 0; index < 4; ++index)
	{
		ASSERT_TRUE(renderer.mesh_draw(mesh, nullptr));
		ASSERT_TRUE(renderer.frame_render());
	}
	engine::Frame_Stats stats = renderer.frame_stats_get();
	if (renderer.gpu_timestamp_supported())
	{
		EXPECT_GT(stats.gpu_scene_ms, 0.0) << "链路径场景阶段未产生耗时";
		EXPECT_GT(stats.gpu_post_ms, 0.0) << "链路径未产生后处理耗时";
		EXPECT_LE(stats.gpu_scene_ms + stats.gpu_post_ms, stats.gpu_frame_ms + 0.01)
			<< "场景 ＋ 后处理超过总耗时";
	}
	else
	{
		EXPECT_EQ(stats.gpu_scene_ms, 0.0);
		EXPECT_EQ(stats.gpu_post_ms, 0.0);
	}
	renderer.mesh_unload(mesh);
	renderer.pipeline_unload(invert);
	renderer.render_target_destroy(scene_target);
}

/*
阴影 pass 独立计时（第一百二十八轮）
开启方向光阴影后，阴影耗时（独立提交）应大于 0；关闭阴影后不再刷新。
*/
TEST_F(Renderer_Gpu_Test, 阴影pass独立计时)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Mesh_Handle receiver = 0;
	engine::Mesh_Handle occluder = 0;
	ASSERT_TRUE(renderer.mesh_load(receiver, normal_quad()));
	ASSERT_TRUE(renderer.mesh_load(occluder, normal_quad()));

	//场景：方向光沿 +Z（光源在 -Z 侧），强度 1
	engine::Camera_Data camera{};
	engine::Light_Data light{};
	light.direction[2] = 1.0f;
	light.intensity = 1.0f;
	light.ambient = 0.0f;
	engine::Fog_Data fog{};
	renderer.scene_apply(camera, light, fog);
	renderer.shadow_enable_set(true);
	ASSERT_TRUE(renderer.shadow_enabled());

	//遮挡面缩放 0.6、平移到 z = -0.5
	float transform[16] = {};
	transform[0] = 0.6f;
	transform[5] = 0.6f;
	transform[10] = 0.6f;
	transform[14] = -0.5f;
	transform[15] = 1.0f;
	for (uint32_t index = 0; index < 4; ++index)
	{
		ASSERT_TRUE(renderer.mesh_draw(occluder, transform));
		ASSERT_TRUE(renderer.mesh_draw(receiver, nullptr));
		ASSERT_TRUE(renderer.frame_render());
	}
	engine::Frame_Stats stats = renderer.frame_stats_get();
	RecordProperty("shadow_pass_gpu_ms", stats.gpu_shadow_ms);
	if (renderer.gpu_timestamp_supported())
		EXPECT_GT(stats.gpu_shadow_ms, 0.0) << "阴影 pass 未产生独立 GPU 耗时";
	else
		EXPECT_EQ(stats.gpu_shadow_ms, 0.0);

	renderer.shadow_enable_set(false);
	renderer.mesh_unload(receiver);
	renderer.mesh_unload(occluder);
}

/*
GPU 分解累计口径（第一百三十一轮）
直通路径渲染 4 帧后：场景累计与总累计同一读回口径（样本数一致）；
支持时「场景累计 ＋ 后处理累计 ≤ 总累计」；阴影未启用时阴影累计与样本数恒 0。
不支持时间戳时四项恒 0（不跳过）。
*/
TEST_F(Renderer_Gpu_Test, GPU分解累计口径)
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
	RecordProperty("gpu_scene_total_ms", stats.gpu_scene_total_ms);
	RecordProperty("gpu_total_ms", stats.gpu_total_ms);
	if (renderer.gpu_timestamp_supported())
	{
		//场景累计随每次读回累加，故与总累计同为正且不超过总累计
		EXPECT_GT(stats.gpu_scene_total_ms, 0.0) << "场景累计未随读回累加";
		EXPECT_LE(stats.gpu_post_total_ms, 0.05) << "直通路径不应有后处理累计";
		EXPECT_LE(stats.gpu_scene_total_ms + stats.gpu_post_total_ms, stats.gpu_total_ms + 0.01)
			<< "分段累计超过总累计";
		//样本数口径与总累计一致（两帧可读回）
		EXPECT_EQ(stats.gpu_samples, 2u);
	}
	else
	{
		EXPECT_EQ(stats.gpu_scene_total_ms, 0.0);
		EXPECT_EQ(stats.gpu_post_total_ms, 0.0);
	}
	//未启用阴影：阴影累计与样本数恒 0
	EXPECT_EQ(stats.gpu_shadow_total_ms, 0.0);
	EXPECT_EQ(stats.gpu_shadow_samples, 0u);

	renderer.mesh_unload(mesh);
}

/*
GPU 分解累计随阴影按需重绘递增（第一百三十一轮）
固定场景连续 4 帧：阴影只在首帧按需重绘，故阴影样本数应为 1，阴影累计等于该次耗时且 > 0。
*/
TEST_F(Renderer_Gpu_Test, 阴影累计随按需重绘递增)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Mesh_Handle receiver = 0;
	engine::Mesh_Handle occluder = 0;
	ASSERT_TRUE(renderer.mesh_load(receiver, normal_quad()));
	ASSERT_TRUE(renderer.mesh_load(occluder, normal_quad()));
	engine::Camera_Data camera{};
	engine::Light_Data light{};
	light.direction[2] = 1.0f;
	light.intensity = 1.0f;
	light.ambient = 0.0f;
	engine::Fog_Data fog{};
	renderer.scene_apply(camera, light, fog);
	renderer.shadow_enable_set(true);
	float transform[16] = {};
	transform[0] = 0.6f;
	transform[5] = 0.6f;
	transform[10] = 0.6f;
	transform[14] = -0.5f;
	transform[15] = 1.0f;
	//场景与变换完全不变 → 阴影签名不变 → 仅首帧重绘
	for (uint32_t index = 0; index < 4; ++index)
	{
		ASSERT_TRUE(renderer.mesh_draw(occluder, transform));
		ASSERT_TRUE(renderer.mesh_draw(receiver, nullptr));
		ASSERT_TRUE(renderer.frame_render());
	}
	engine::Frame_Stats stats = renderer.frame_stats_get();
	RecordProperty("gpu_shadow_samples", static_cast<int>(stats.gpu_shadow_samples));
	if (renderer.gpu_timestamp_supported())
	{
		EXPECT_EQ(stats.gpu_shadow_samples, 1u) << "按需重绘之外的帧不应产生阴影样本";
		EXPECT_GT(stats.gpu_shadow_total_ms, 0.0) << "阴影累计未随读回累加";
	}
	else
	{
		EXPECT_EQ(stats.gpu_shadow_samples, 0u);
		EXPECT_EQ(stats.gpu_shadow_total_ms, 0.0);
	}

	renderer.shadow_enable_set(false);
	renderer.mesh_unload(receiver);
	renderer.mesh_unload(occluder);
}

/*
帧统计清零含分段累计（第一百三十一轮）
渲染若干帧后清零：分段累计与阴影样本归零，最近一帧即时量保持不变。
*/
TEST_F(Renderer_Gpu_Test, 清零含分段累计)
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
	const double last_scene_ms = renderer.frame_stats_get().gpu_scene_ms;

	renderer.frame_stats_reset();
	engine::Frame_Stats stats = renderer.frame_stats_get();
	EXPECT_EQ(stats.frames, 0u);
	EXPECT_EQ(stats.gpu_total_ms, 0.0);
	EXPECT_EQ(stats.gpu_samples, 0u);
	EXPECT_EQ(stats.gpu_scene_total_ms, 0.0);
	EXPECT_EQ(stats.gpu_post_total_ms, 0.0);
	EXPECT_EQ(stats.gpu_shadow_total_ms, 0.0);
	EXPECT_EQ(stats.gpu_shadow_samples, 0u);
	//最近一帧即时量不受清零影响
	EXPECT_EQ(stats.gpu_scene_ms, last_scene_ms);

	renderer.mesh_unload(mesh);
}
