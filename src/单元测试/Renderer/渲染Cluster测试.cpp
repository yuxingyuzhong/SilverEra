/*
渲染Cluster光照测试（第二百六十六轮）
覆盖：光源记录 SSBO 上传（未初始化拒绝 / 默认关闭不上传 / 开启后按帧上传 / 光源增减即时反映 / 关闭归零）。
*/
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <vector>

#include "Application/Renderer/src/render/Renderer/渲染器.h"

namespace
{
	//隐藏窗口尺寸
	constexpr uint32_t WINDOW_WIDTH = 64;
	constexpr uint32_t WINDOW_HEIGHT = 36;
	//小四边形半边长
	constexpr float QUAD_HALF = 0.25f;

	//构造隐藏窗口配置
	engine::Renderer_Config hidden_config(void)
	{
		engine::Renderer_Config config;
		config.width = WINDOW_WIDTH;
		config.height = WINDOW_HEIGHT;
		config.hidden = true;
		return config;
	}

	//构造中心小四边形（两个三角形）
	engine::Mesh_Data quad_data(void)
	{
		engine::Mesh_Data data;
		data.vertices = {
			{ { -QUAD_HALF, -QUAD_HALF, 0.0f }, { 0.0f, 0.0f } },
			{ { QUAD_HALF, -QUAD_HALF, 0.0f }, { 1.0f, 0.0f } },
			{ { QUAD_HALF, QUAD_HALF, 0.0f }, { 1.0f, 1.0f } },
			{ { -QUAD_HALF, QUAD_HALF, 0.0f }, { 0.0f, 1.0f } },
		};
		data.indices = { 0, 1, 2, 0, 2, 3 };
		return data;
	}

	//单位模型矩阵（列主序）
	std::array<float, 16> identity_matrix(void)
	{
		return std::array<float, 16>{
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		};
	}

	/*
	构造带法线的中心小四边形（第二百八十九轮）
	光照分支需要法线（无法线时片元按满照处理，点光差异不可见），故簇采样用例统一使用本网格。
	*/
	engine::Mesh_Data quad_data_lit(void)
	{
		engine::Mesh_Data data;
		data.vertices = {
			{ { -1.0f, -1.0f, 0.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } },
			{ { 1.0f, -1.0f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } },
			{ { 1.0f, 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } },
			{ { -1.0f, 1.0f, 0.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } },
		};
		data.indices = { 0, 1, 2, 0, 2, 3 };
		return data;
	}
}

TEST(Renderer_Cluster_Test, 光源记录上传与开关)
{
	engine::Renderer renderer;
	//未初始化：能力与开关均为假、计数为 0、启用被拒
	EXPECT_FALSE(renderer.cluster_supported());
	EXPECT_FALSE(renderer.cluster_enable_get());
	EXPECT_EQ(renderer.cluster_light_count(), 0u);
	EXPECT_FALSE(renderer.cluster_enable_set(true));

	ASSERT_TRUE(renderer.init(hidden_config()));
	//默认关闭：不创建资源、不上传
	EXPECT_FALSE(renderer.cluster_enable_get());
	EXPECT_EQ(renderer.cluster_light_count(), 0u);
	//本机不支持计算路径时 Cluster 保持关闭（与 GPU 路径降级口径一致）
	if (!renderer.cluster_supported())
	{
		GTEST_SKIP() << "本机不支持计算路径，Cluster 光照保持关闭";
	}

	//开启后首帧上传主方向光 ＋ 补光两条记录
	ASSERT_TRUE(renderer.cluster_enable_set(true));
	EXPECT_TRUE(renderer.cluster_enable_get());
	ASSERT_TRUE(renderer.frame_render());
	EXPECT_EQ(renderer.cluster_light_count(), 2u);

	//追加一点光与一聚光后，下一帧上传 4 条
	const float point_position[3] = { 1.0f, 2.0f, 3.0f };
	const float point_color[3] = { 0.2f, 0.4f, 0.6f };
	const float spot_direction[3] = { 0.0f, -1.0f, 0.0f };
	ASSERT_TRUE(renderer.light_add_point(point_position, point_color, 7.0f, 0.5f, 0.1f, 1, 0.8f, 32.0f));
	ASSERT_TRUE(renderer.light_add_spot(point_position, spot_direction, point_color, 7.0f, 0.5f, 30.0f, 45.0f, 0.1f));
	ASSERT_TRUE(renderer.frame_render());
	EXPECT_EQ(renderer.cluster_light_count(), 4u);

	//清空光源后回到两条基础记录
	renderer.lights_clear();
	ASSERT_TRUE(renderer.frame_render());
	EXPECT_EQ(renderer.cluster_light_count(), 2u);

	//关闭后计数归零、开关为假
	ASSERT_TRUE(renderer.cluster_enable_set(false));
	EXPECT_FALSE(renderer.cluster_enable_get());
	EXPECT_EQ(renderer.cluster_light_count(), 0u);
}

//分簇网格与 CPU 逐簇计数参考（第二百六十七轮）
TEST(Renderer_Cluster_Test, 分簇网格与逐簇计数参考)
{
	engine::Renderer renderer;
	//未初始化：网格查询失败、设置被拒、计数失败
	uint32_t grid_x = 1;
	uint32_t grid_y = 1;
	uint32_t grid_z = 1;
	EXPECT_FALSE(renderer.cluster_grid_get(grid_x, grid_y, grid_z));
	EXPECT_EQ(renderer.cluster_grid_cluster_count(), 0u);
	EXPECT_FALSE(renderer.cluster_grid_set(16, 9, 24));
	std::vector<uint32_t> counts(4096);
	uint32_t total = 0;
	EXPECT_FALSE(renderer.cluster_counts_reference_ex(counts.data(), static_cast<uint32_t>(counts.size()), &total));

	ASSERT_TRUE(renderer.init(hidden_config()));
	//默认网格 16 × 9 × 24 = 3456 簇
	ASSERT_TRUE(renderer.cluster_grid_get(grid_x, grid_y, grid_z));
	EXPECT_EQ(grid_x, 16u);
	EXPECT_EQ(grid_y, 9u);
	EXPECT_EQ(grid_z, 24u);
	EXPECT_EQ(renderer.cluster_grid_cluster_count(), 3456u);

	//网格钳制：全 0 → 每轴 1；100 × 100 × 100 → 总簇数不超过 4096
	ASSERT_TRUE(renderer.cluster_grid_set(0, 0, 0));
	EXPECT_EQ(renderer.cluster_grid_cluster_count(), 1u);
	ASSERT_TRUE(renderer.cluster_grid_set(100, 100, 100));
	EXPECT_LE(renderer.cluster_grid_cluster_count(), 4096u);
	//恢复默认网格
	ASSERT_TRUE(renderer.cluster_grid_set(16, 9, 24));

	//容量不足被拒
	EXPECT_FALSE(renderer.cluster_counts_reference_ex(counts.data(), 100, &total));

	//仅主方向光（补光强度 0 不计）：全部簇计数 1
	ASSERT_TRUE(renderer.cluster_counts_reference_ex(counts.data(), 3456, &total));
	EXPECT_EQ(total, 3456u);
	for (uint32_t index = 0; index < 3456; ++index)
		EXPECT_EQ(counts[index], 1u);

	//一盏大半径点光（跨近平面 → 整屏全深度保守覆盖）：全部簇计数 2
	const float position[3] = { 0.0f, 0.0f, 0.0f };
	const float color[3] = { 1.0f, 1.0f, 1.0f };
	ASSERT_TRUE(renderer.light_add_point(position, color, 100.0f, 0.5f, 0.1f));
	ASSERT_TRUE(renderer.cluster_counts_reference_ex(counts.data(), 3456, &total));
	for (uint32_t index = 0; index < 3456; ++index)
		EXPECT_EQ(counts[index], 2u);

	//清空点光后回到 1
	renderer.lights_clear();
	ASSERT_TRUE(renderer.cluster_counts_reference_ex(counts.data(), 3456, &total));
	for (uint32_t index = 0; index < 3456; ++index)
		EXPECT_EQ(counts[index], 1u);
}

//GPU 逐簇计数与 CPU 参考一致（第二百六十八轮）
TEST(Renderer_Cluster_Test, GPU逐簇计数与CPU参考一致)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	std::vector<uint32_t> cpu_counts(4096);
	std::vector<uint32_t> gpu_counts(4096);
	uint32_t cpu_total = 0;
	uint32_t gpu_total = 0;
	//容量不足被拒
	EXPECT_FALSE(renderer.cluster_counts_gpu(gpu_counts.data(), 100, &gpu_total));
	//本机不支持计算路径时跳过（GPU 计数降级口径）
	if (!renderer.compute_supported())
	{
		GTEST_SKIP() << "本机不支持计算路径，GPU 逐簇计数不可用";
	}

	//仅主方向光：GPU 与 CPU 逐簇一致
	ASSERT_TRUE(renderer.cluster_counts_reference_ex(cpu_counts.data(), 3456, &cpu_total));
	ASSERT_TRUE(renderer.cluster_counts_gpu(gpu_counts.data(), 3456, &gpu_total));
	ASSERT_EQ(cpu_total, gpu_total);
	for (uint32_t index = 0; index < cpu_total; ++index)
		EXPECT_EQ(cpu_counts[index], gpu_counts[index]);

	//一盏大半径点光（全屏全深度）：GPU 与 CPU 仍逐簇一致
	const float position[3] = { 0.0f, 0.0f, 0.0f };
	const float color[3] = { 1.0f, 1.0f, 1.0f };
	ASSERT_TRUE(renderer.light_add_point(position, color, 100.0f, 0.5f, 0.1f));
	ASSERT_TRUE(renderer.cluster_counts_reference_ex(cpu_counts.data(), 3456, &cpu_total));
	ASSERT_TRUE(renderer.cluster_counts_gpu(gpu_counts.data(), 3456, &gpu_total));
	ASSERT_EQ(cpu_total, gpu_total);
	for (uint32_t index = 0; index < cpu_total; ++index)
		EXPECT_EQ(cpu_counts[index], gpu_counts[index]);
}

//块内前缀和与 CPU 参考一致（第二百六十九轮）
TEST(Renderer_Cluster_Test, 块内前缀和与CPU参考一致)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//固定计数序列（3456 簇；值域 0..19，便于人工对数）
	constexpr uint32_t COUNT = 3456;
	constexpr uint32_t BLOCKS = 14;
	std::vector<uint32_t> counts(COUNT);
	for (uint32_t index = 0; index < COUNT; ++index)
		counts[index] = index % 7u + index % 13u;
	std::vector<uint32_t> cpu_prefix(COUNT);
	std::vector<uint32_t> gpu_prefix(COUNT);
	std::vector<uint32_t> cpu_totals(BLOCKS);
	std::vector<uint32_t> gpu_totals(BLOCKS);
	uint32_t cpu_blocks = 0;
	uint32_t gpu_blocks = 0;
	//CPU 参考：容量不足被拒
	EXPECT_FALSE(renderer.cluster_prefix_block_scan_cpu(counts.data(), COUNT, cpu_prefix.data(),
		cpu_totals.data(), 4, &cpu_blocks));
	//GPU：容量不足被拒
	EXPECT_FALSE(renderer.cluster_prefix_block_scan_gpu(counts.data(), COUNT, gpu_prefix.data(),
		gpu_totals.data(), 4, &gpu_blocks));
	//本机不支持计算路径时跳过（GPU 扫描降级口径）
	if (!renderer.compute_supported())
	{
		GTEST_SKIP() << "本机不支持计算路径，GPU 块内扫描不可用";
	}

	ASSERT_TRUE(renderer.cluster_prefix_block_scan_cpu(counts.data(), COUNT, cpu_prefix.data(),
		cpu_totals.data(), BLOCKS, &cpu_blocks));
	ASSERT_EQ(cpu_blocks, BLOCKS);
	ASSERT_TRUE(renderer.cluster_prefix_block_scan_gpu(counts.data(), COUNT, gpu_prefix.data(),
		gpu_totals.data(), BLOCKS, &gpu_blocks));
	ASSERT_EQ(gpu_blocks, BLOCKS);
	//逐元素前缀和与逐块总和一致
	for (uint32_t index = 0; index < COUNT; ++index)
		EXPECT_EQ(cpu_prefix[index], gpu_prefix[index]);
	for (uint32_t block = 0; block < BLOCKS; ++block)
		EXPECT_EQ(cpu_totals[block], gpu_totals[block]);
}

//全量前缀和与 CPU 参考一致（第二百七十轮）
TEST(Renderer_Cluster_Test, 全量前缀和与CPU参考一致)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//固定计数序列（4096 簇 = 16 块，覆盖块上限）
	constexpr uint32_t COUNT = 4096;
	std::vector<uint32_t> counts(COUNT);
	for (uint32_t index = 0; index < COUNT; ++index)
		counts[index] = index % 9u + index % 5u;
	std::vector<uint32_t> cpu_prefix(COUNT);
	std::vector<uint32_t> gpu_prefix(COUNT);
	//本机不支持计算路径时跳过（GPU 全量扫描降级口径）
	if (!renderer.compute_supported())
	{
		GTEST_SKIP() << "本机不支持计算路径，GPU 全量扫描不可用";
	}

	ASSERT_TRUE(renderer.cluster_prefix_scan_cpu(counts.data(), COUNT, cpu_prefix.data()));
	ASSERT_TRUE(renderer.cluster_prefix_scan_gpu(counts.data(), COUNT, gpu_prefix.data()));
	//逐元素一致，且单调不减、末项等于全量总和
	uint64_t total_sum = 0;
	for (uint32_t index = 0; index < COUNT; ++index)
	{
		EXPECT_EQ(cpu_prefix[index], gpu_prefix[index]);
		total_sum += counts[index];
	}
	EXPECT_EQ(gpu_prefix[COUNT - 1], static_cast<uint32_t>(total_sum));
}

//簇光源列表与 CPU 参考成员一致（第二百七十一轮）
TEST(Renderer_Cluster_Test, 簇光源列表与CPU参考一致)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	constexpr uint32_t CLUSTERS = 3456;
	//场景：主方向光 ＋ 一盏点光 ＋ 一盏聚光
	const float position[3] = { 0.0f, 0.0f, 0.0f };
	const float color[3] = { 1.0f, 1.0f, 1.0f };
	const float spot_direction[3] = { 0.0f, -1.0f, 0.0f };
	ASSERT_TRUE(renderer.light_add_point(position, color, 10.0f, 0.5f, 0.1f));
	ASSERT_TRUE(renderer.light_add_spot(position, spot_direction, color, 10.0f, 0.5f, 25.0f, 40.0f, 0.1f));
	//逐簇计数
	std::vector<uint32_t> counts(CLUSTERS);
	uint32_t total = 0;
	ASSERT_TRUE(renderer.cluster_counts_reference_ex(counts.data(), CLUSTERS, &total));
	ASSERT_EQ(total, CLUSTERS);
	uint32_t entries = 0;
	for (uint32_t cluster = 0; cluster < CLUSTERS; ++cluster)
		entries += counts[cluster];
	//本机不支持计算路径时跳过（GPU 列表降级口径）
	if (!renderer.compute_supported())
	{
		GTEST_SKIP() << "本机不支持计算路径，GPU 簇列表不可用";
	}

	std::vector<uint32_t> cpu_list(entries);
	std::vector<uint32_t> gpu_list(entries);
	std::vector<uint32_t> cpu_offsets(CLUSTERS);
	std::vector<uint32_t> gpu_offsets(CLUSTERS);
	uint32_t cpu_entries = 0;
	uint32_t gpu_entries = 0;
	//容量不足被拒
	EXPECT_FALSE(renderer.cluster_list_build_gpu(gpu_list.data(), entries - 1, gpu_offsets.data(),
		counts.data(), CLUSTERS, &gpu_entries));
	ASSERT_TRUE(renderer.cluster_list_build_cpu(cpu_list.data(), entries, cpu_offsets.data(),
		counts.data(), CLUSTERS, &cpu_entries));
	ASSERT_TRUE(renderer.cluster_list_build_gpu(gpu_list.data(), entries, gpu_offsets.data(),
		counts.data(), CLUSTERS, &gpu_entries));
	EXPECT_EQ(cpu_entries, entries);
	EXPECT_EQ(gpu_entries, entries);
	//偏移一致，且逐簇成员集合一致（簇内顺序由 GPU 原子竞争决定，故排序后比较）
	for (uint32_t cluster = 0; cluster < CLUSTERS; ++cluster)
		EXPECT_EQ(cpu_offsets[cluster], gpu_offsets[cluster]);
	for (uint32_t cluster = 0; cluster < CLUSTERS; ++cluster)
	{
		uint32_t begin = cpu_offsets[cluster];
		uint32_t end = begin + counts[cluster];
		//取两侧同簇切片并排序
		std::vector<uint32_t> cpu_slice(cpu_list.begin() + begin, cpu_list.begin() + end);
		std::vector<uint32_t> gpu_slice(gpu_list.begin() + begin, gpu_list.begin() + end);
		std::sort(cpu_slice.begin(), cpu_slice.end());
		std::sort(gpu_slice.begin(), gpu_slice.end());
		EXPECT_EQ(cpu_slice, gpu_slice);
	}
}

//簇数据整备与观测（第二百七十二轮）
TEST(Renderer_Cluster_Test, 簇数据整备与观测)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	constexpr uint32_t CLUSTERS = 3456;
	//场景：主方向光 ＋ 点光 ＋ 聚光
	const float position[3] = { 0.0f, 0.0f, 0.0f };
	const float color[3] = { 1.0f, 1.0f, 1.0f };
	const float spot_direction[3] = { 0.0f, -1.0f, 0.0f };
	ASSERT_TRUE(renderer.light_add_point(position, color, 10.0f, 0.5f, 0.1f));
	ASSERT_TRUE(renderer.light_add_spot(position, spot_direction, color, 10.0f, 0.5f, 25.0f, 40.0f, 0.1f));
	//参考计数与条目数
	std::vector<uint32_t> reference_counts(CLUSTERS);
	uint32_t total = 0;
	ASSERT_TRUE(renderer.cluster_counts_reference_ex(reference_counts.data(), CLUSTERS, &total));
	uint32_t entries = 0;
	for (uint32_t cluster = 0; cluster < CLUSTERS; ++cluster)
		entries += reference_counts[cluster];
	ASSERT_GT(entries, 0u);

	//簇容量不足被拒
	std::vector<uint32_t> counts_cpu(CLUSTERS);
	std::vector<uint32_t> offsets_cpu(CLUSTERS);
	std::vector<uint32_t> list_cpu(entries);
	uint32_t built_entries = 0;
	EXPECT_FALSE(renderer.cluster_data_build(counts_cpu.data(), offsets_cpu.data(), list_cpu.data(),
		100, entries, &built_entries, false));
	//CPU 路径整备：计数与参考一致、条目与观测一致
	ASSERT_TRUE(renderer.cluster_data_build(counts_cpu.data(), offsets_cpu.data(), list_cpu.data(),
		CLUSTERS, entries, &built_entries, false));
	EXPECT_EQ(built_entries, entries);
	EXPECT_EQ(renderer.cluster_entries_get(), entries);
	EXPECT_GE(renderer.cluster_build_ms_get(), 0.0);
	for (uint32_t cluster = 0; cluster < CLUSTERS; ++cluster)
		EXPECT_EQ(counts_cpu[cluster], reference_counts[cluster]);
	//列表容量不足被拒
	EXPECT_FALSE(renderer.cluster_data_build(counts_cpu.data(), offsets_cpu.data(), list_cpu.data(),
		CLUSTERS, entries - 1, &built_entries, false));

	//本机不支持计算路径时跳过（GPU 整备降级口径）
	if (!renderer.compute_supported())
	{
		GTEST_SKIP() << "本机不支持计算路径，GPU 簇数据整备不可用";
	}
	//GPU 路径整备：计数 / 偏移 / 条目与 CPU 一致
	std::vector<uint32_t> counts_gpu(CLUSTERS);
	std::vector<uint32_t> offsets_gpu(CLUSTERS);
	std::vector<uint32_t> list_gpu(entries);
	uint32_t gpu_entries = 0;
	ASSERT_TRUE(renderer.cluster_data_build(counts_gpu.data(), offsets_gpu.data(), list_gpu.data(),
		CLUSTERS, entries, &gpu_entries, true));
	EXPECT_EQ(gpu_entries, entries);
	EXPECT_EQ(renderer.cluster_entries_get(), entries);
	for (uint32_t cluster = 0; cluster < CLUSTERS; ++cluster)
	{
		EXPECT_EQ(counts_gpu[cluster], counts_cpu[cluster]);
		EXPECT_EQ(offsets_gpu[cluster], offsets_cpu[cluster]);
	}
}

//帧内整备与统计（第二百七十三轮）
TEST(Renderer_Cluster_Test, 帧内整备与统计)
{
	//默认关闭：帧统计簇字段为零（零开销）
	engine::Renderer off;
	ASSERT_TRUE(off.init(hidden_config()));
	ASSERT_TRUE(off.frame_render());
	engine::Frame_Stats stats_off = off.frame_stats_get();
	EXPECT_DOUBLE_EQ(stats_off.cluster_build_ms, 0.0);
	EXPECT_EQ(stats_off.cluster_entries, 0u);

	//开启：需要计算能力（不支持时跳过）
	engine::Renderer on;
	ASSERT_TRUE(on.init(hidden_config()));
	if (!on.cluster_supported())
	{
		GTEST_SKIP() << "本机不支持计算路径，帧内簇整备不可用";
	}
	ASSERT_TRUE(on.cluster_enable_set(true));
	const float position[3] = { 0.0f, 0.0f, 0.0f };
	const float color[3] = { 1.0f, 1.0f, 1.0f };
	ASSERT_TRUE(on.light_add_point(position, color, 10.0f, 0.5f, 0.1f));
	ASSERT_TRUE(on.frame_render());
	engine::Frame_Stats stats_on = on.frame_stats_get();
	EXPECT_GT(stats_on.cluster_entries, 0u);
	EXPECT_GE(stats_on.cluster_build_ms, 0.0);
	EXPECT_EQ(on.cluster_entries_get(), stats_on.cluster_entries);

	//关闭后观测清零
	ASSERT_TRUE(on.cluster_enable_set(false));
	ASSERT_TRUE(on.frame_render());
	engine::Frame_Stats stats_closed = on.frame_stats_get();
	EXPECT_EQ(stats_closed.cluster_entries, 0u);
}

/*
开关组合矩阵（第二百七十四轮；为 16 组合降级矩阵预演）
口径：`cluster_enable × gpu_cull_enable × hiz_enable` 八种组合在静态场景（全屏墙 ＋ 墙后实例）下均正常出帧，
且最终画面逐字节一致（三项能力都不改变被墙完全覆盖的像素）。
*/
TEST(Renderer_Cluster_Test, 开关组合矩阵)
{
	const uint32_t pixel_count = WINDOW_WIDTH * WINDOW_HEIGHT * 4u;
	std::vector<std::vector<uint8_t>> results;
	//八种组合
	for (uint32_t combo = 0; combo < 8; ++combo)
	{
		engine::Renderer_Config config = hidden_config();
		config.msaa_samples = 1;
		config.gpu_cull_enable = (combo & 1u) != 0;
		config.hiz_enable = (combo & 2u) != 0;
		config.cluster_enable = (combo & 4u) != 0;
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(config));
		//请求 Cluster 但不支持时跳过（降级矩阵口径）
		if (config.cluster_enable)
		{
			if (!renderer.cluster_supported())
				GTEST_SKIP() << "本机不支持计算路径，无法覆盖 Cluster 开启组合";
			ASSERT_TRUE(renderer.cluster_enable_set(true));
		}
		renderer.camera_position_set(0.0f, 0.0f, 5.0f);
		renderer.camera_target_set(0.0f, 0.0f, 0.0f);
		engine::Mesh_Handle mesh = 0;
		ASSERT_TRUE(renderer.mesh_load(mesh, quad_data()));
		//全屏墙（z = 3，放大 30 倍）＋ 墙后 10 实例
		std::array<float, 16> wall = identity_matrix();
		wall[0] = 30.0f;
		wall[5] = 30.0f;
		wall[14] = 3.0f;
		std::vector<std::array<float, 16>> behind(10, identity_matrix());
		//逐帧回读（无回读帧后再请求回读会得到清屏色，故每帧都带回读；第二百九十三轮）
		std::vector<uint8_t> pixels(pixel_count, 0u);
		uint32_t width = 0;
		uint32_t height = 0;
		for (uint32_t frame = 0; frame < 3; ++frame)
		{
			ASSERT_TRUE(renderer.mesh_draw_instanced(mesh, wall.data(), 1));
			ASSERT_TRUE(renderer.mesh_draw_instanced(mesh, behind[0].data(), static_cast<uint32_t>(behind.size())));
			ASSERT_TRUE(renderer.frame_render(pixels.data(), pixel_count, &width, &height));
		}
		ASSERT_EQ(width, WINDOW_WIDTH);
		ASSERT_EQ(height, WINDOW_HEIGHT);
		results.push_back(std::move(pixels));
		renderer.mesh_unload(mesh);
	}
	//八种组合画面逐字节一致
	for (uint32_t combo = 1; combo < results.size(); ++combo)
	{
		const bool same = std::mismatch(results[0].begin(), results[0].end(), results[combo].begin()).first
			== results[0].end();
		EXPECT_TRUE(same) << "组合 " << combo << " 与组合 0 画面不一致";
	}
}

//构建性能对照（第二百七十六轮；CPU / GPU 整备耗时基线）
TEST(Renderer_Cluster_Test, 构建性能对照)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	constexpr uint32_t CLUSTERS = 3456;
	constexpr uint32_t LIST_CAPACITY = CLUSTERS * 70u;
	constexpr uint32_t RUNS = 5;
	//本机不支持计算路径时跳过（GPU 基线不可测）
	if (!renderer.compute_supported())
	{
		GTEST_SKIP() << "本机不支持计算路径，GPU 构建基线不可测";
	}

	std::vector<uint32_t> counts_cpu(CLUSTERS);
	std::vector<uint32_t> offsets_cpu(CLUSTERS);
	std::vector<uint32_t> list_cpu(LIST_CAPACITY);
	std::vector<uint32_t> counts_gpu(CLUSTERS);
	std::vector<uint32_t> offsets_gpu(CLUSTERS);
	std::vector<uint32_t> list_gpu(LIST_CAPACITY);
	//开启 Cluster 以解除 8 盏登记上限（第二百八十轮）
	ASSERT_TRUE(renderer.cluster_enable_set(true));
	//四档光源规模：2 条基础 / ＋2 / ＋6 / ＋62（对应 2 / 4 / 8 / 64 条记录）
	for (uint32_t stage = 0; stage < 4; ++stage)
	{
		uint32_t add_count = stage == 0 ? 0u : (stage == 1 ? 2u : (stage == 2 ? 6u : 62u));
		for (uint32_t index = 0; index < add_count; ++index)
		{
			const float position[3] = { static_cast<float>(index) * 0.5f - 1.0f, 0.0f, 0.0f };
			const float color[3] = { 1.0f, 0.6f, 0.3f };
			ASSERT_TRUE(renderer.light_add_point(position, color, 8.0f, 0.5f, 0.1f));
		}
		//CPU 路径：5 次取最小耗时
		uint32_t cpu_entries = 0;
		double cpu_best_ms = 1e9;
		for (uint32_t run = 0; run < RUNS; ++run)
		{
			ASSERT_TRUE(renderer.cluster_data_build(counts_cpu.data(), offsets_cpu.data(), list_cpu.data(),
				CLUSTERS, LIST_CAPACITY, &cpu_entries, false));
			cpu_best_ms = (std::min)(cpu_best_ms, renderer.cluster_build_ms_get());
		}
		//GPU 路径：5 次取最小耗时
		uint32_t gpu_entries = 0;
		double gpu_best_ms = 1e9;
		for (uint32_t run = 0; run < RUNS; ++run)
		{
			ASSERT_TRUE(renderer.cluster_data_build(counts_gpu.data(), offsets_gpu.data(), list_gpu.data(),
				CLUSTERS, LIST_CAPACITY, &gpu_entries, true));
			gpu_best_ms = (std::min)(gpu_best_ms, renderer.cluster_build_ms_get());
		}
		//两条路径结果一致
		EXPECT_EQ(cpu_entries, gpu_entries);
		for (uint32_t cluster = 0; cluster < CLUSTERS; ++cluster)
			EXPECT_EQ(counts_cpu[cluster], counts_gpu[cluster]);
		//输出基线
		std::cout << "[Cluster构建] 光源 " << (2u + add_count) << " | CPU " << cpu_best_ms
			<< " ms / GPU " << gpu_best_ms << " ms | 条目 " << cpu_entries << std::endl;
	}
}

/*
组合矩阵：阴影与 MSAA（第二百七十八轮）
口径：固定「阴影开关 × 采样数」下，`cluster_enable` 开 / 关两态画面逐字节一致；
覆盖 1× / 4× MSAA 与阴影开 / 关四种环境（共八次出帧）。
*/
TEST(Renderer_Cluster_Test, 组合矩阵_阴影与MSAA)
{
	const uint32_t pixel_count = WINDOW_WIDTH * WINDOW_HEIGHT * 4u;
	const uint32_t msaa_list[2] = { 1u, 4u };
	for (uint32_t shadow_index = 0; shadow_index < 2; ++shadow_index)
		for (uint32_t msaa_index = 0; msaa_index < 2; ++msaa_index)
		{
			//同一环境下的两态结果（下标 0 = Cluster 关、1 = Cluster 开）
			std::vector<std::vector<uint8_t>> pair;
			for (uint32_t cluster_index = 0; cluster_index < 2; ++cluster_index)
			{
				engine::Renderer_Config config = hidden_config();
				config.msaa_samples = msaa_list[msaa_index];
				engine::Renderer renderer;
				ASSERT_TRUE(renderer.init(config));
				if (cluster_index != 0)
				{
					if (!renderer.cluster_supported())
						GTEST_SKIP() << "本机不支持计算路径，无法覆盖 Cluster 开启组合";
					ASSERT_TRUE(renderer.cluster_enable_set(true));
				}
				renderer.shadow_enable_set(shadow_index != 0);
				renderer.camera_position_set(0.0f, 0.0f, 5.0f);
				renderer.camera_target_set(0.0f, 0.0f, 0.0f);
				engine::Mesh_Handle mesh = 0;
				ASSERT_TRUE(renderer.mesh_load(mesh, quad_data()));
				//全屏墙（z = 3，放大 30 倍）＋ 墙后 10 实例
				std::array<float, 16> wall = identity_matrix();
				wall[0] = 30.0f;
				wall[5] = 30.0f;
				wall[14] = 3.0f;
				std::vector<std::array<float, 16>> behind(10, identity_matrix());
				//逐帧回读（无回读帧后再请求回读会得到清屏色，故每帧都带回读；第二百九十三轮）
				std::vector<uint8_t> pixels(pixel_count, 0u);
				uint32_t width = 0;
				uint32_t height = 0;
				for (uint32_t frame = 0; frame < 3; ++frame)
				{
					ASSERT_TRUE(renderer.mesh_draw_instanced(mesh, wall.data(), 1));
					ASSERT_TRUE(renderer.mesh_draw_instanced(mesh, behind[0].data(),
						static_cast<uint32_t>(behind.size())));
					ASSERT_TRUE(renderer.frame_render(pixels.data(), pixel_count, &width, &height));
				}
				ASSERT_EQ(width, WINDOW_WIDTH);
				ASSERT_EQ(height, WINDOW_HEIGHT);
				pair.push_back(std::move(pixels));
				renderer.mesh_unload(mesh);
			}
			//同一环境下 Cluster 开 / 关画面逐字节一致
			const bool same = std::mismatch(pair[0].begin(), pair[0].end(), pair[1].begin()).first
				== pair[0].end();
			EXPECT_TRUE(same) << "阴影 " << shadow_index << " / MSAA " << msaa_list[msaa_index]
				<< " 下 Cluster 开关改变了画面";
		}
}

//光源登记上限与截断（第二百八十轮）
TEST(Renderer_Cluster_Test, 光源登记上限与截断)
{
	//关闭 Cluster：登记上限 8 盏，第 9 盏被拒并置截断标志
	engine::Renderer off;
	ASSERT_TRUE(off.init(hidden_config()));
	for (uint32_t index = 0; index < 8; ++index)
	{
		const float position[3] = { static_cast<float>(index) * 0.4f - 1.4f, 0.0f, 0.0f };
		const float color[3] = { 1.0f, 1.0f, 1.0f };
		ASSERT_TRUE(off.light_add_point(position, color, 6.0f, 0.4f, 0.1f));
	}
	const float extra_position[3] = { 2.0f, 0.0f, 0.0f };
	const float extra_color[3] = { 1.0f, 1.0f, 1.0f };
	EXPECT_FALSE(off.light_add_point(extra_position, extra_color, 6.0f, 0.4f, 0.1f));
	ASSERT_TRUE(off.frame_render());
	EXPECT_EQ(off.frame_stats_get().light_truncated, 1u);
	EXPECT_EQ(off.point_light_count(), 8u);
	off.lights_clear();
	EXPECT_EQ(off.point_light_count(), 0u);

	//开启 Cluster：可登记超过 8 盏（本用例登记 16 盏）
	engine::Renderer on;
	ASSERT_TRUE(on.init(hidden_config()));
	if (!on.cluster_supported())
	{
		GTEST_SKIP() << "本机不支持计算路径，Cluster 开启路径不可用";
	}
	ASSERT_TRUE(on.cluster_enable_set(true));
	for (uint32_t index = 0; index < 16; ++index)
	{
		const float position[3] = { static_cast<float>(index) * 0.25f - 2.0f, 0.0f, 0.0f };
		const float color[3] = { 0.8f, 0.8f, 1.0f };
		ASSERT_TRUE(on.light_add_point(position, color, 6.0f, 0.4f, 0.1f));
	}
	EXPECT_EQ(on.point_light_count(), 16u);
	EXPECT_EQ(on.light_record_count(), 18u);
	ASSERT_TRUE(on.frame_render());
	EXPECT_EQ(on.frame_stats_get().light_truncated, 0u);

	//关闭 Cluster 后再次登记被拒，并置截断标志
	ASSERT_TRUE(on.cluster_enable_set(false));
	EXPECT_FALSE(on.light_add_point(extra_position, extra_color, 6.0f, 0.4f, 0.1f));
	ASSERT_TRUE(on.frame_render());
	EXPECT_EQ(on.frame_stats_get().light_truncated, 1u);
}

//光源容量边界（第二百八十三轮）
TEST(Renderer_Cluster_Test, 光源容量边界)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//本机不支持计算路径时跳过（Cluster 开启路径不可用）
	if (!renderer.cluster_supported())
	{
		GTEST_SKIP() << "本机不支持计算路径，Cluster 开启路径不可用";
	}
	ASSERT_TRUE(renderer.cluster_enable_set(true));
	//登记到容量上限（1024 盏）
	constexpr uint32_t CAPACITY = 1024;
	for (uint32_t index = 0; index < CAPACITY; ++index)
	{
		const float position[3] = { static_cast<float>(index % 32) * 0.1f, 0.0f, 0.0f };
		const float color[3] = { 1.0f, 1.0f, 1.0f };
		ASSERT_TRUE(renderer.light_add_point(position, color, 4.0f, 0.2f, 0.1f));
	}
	EXPECT_EQ(renderer.point_light_count(), CAPACITY);
	//记录数 = 主方向光 ＋ 补光 ＋ 逐盏（容量约束下有效数仍为 1024）
	EXPECT_EQ(renderer.light_record_count(), CAPACITY + 2u);
	EXPECT_EQ(engine::light_effective_count(CAPACITY + 2u, true), CAPACITY);
	//第 1025 盏被拒
	const float extra_position[3] = { 3.0f, 0.0f, 0.0f };
	const float extra_color[3] = { 1.0f, 1.0f, 1.0f };
	EXPECT_FALSE(renderer.light_add_point(extra_position, extra_color, 4.0f, 0.2f, 0.1f));
	EXPECT_EQ(renderer.point_light_count(), CAPACITY);
	//清空后归零
	renderer.lights_clear();
	EXPECT_EQ(renderer.point_light_count(), 0u);
	EXPECT_EQ(renderer.light_record_count(), 2u);
}

/*
图形 set0 簇数据缓冲上传（第二百八十八轮）
口径：本轮着色器尚未读取簇缓冲，开 / 关两态画面逐字节一致；
开启时 cluster_gpu_bytes 反映「计数 ＋ 偏移整块 ＋ 列表条目」的上传量，关闭时恒 0。
*/
TEST(Renderer_Cluster_Test, 图形set0簇缓冲上传)
{
	const uint32_t pixel_count = WINDOW_WIDTH * WINDOW_HEIGHT * 4u;
	//两态（下标 0 = 关闭、1 = 开启）
	std::vector<std::vector<uint8_t>> results;
	for (uint32_t cluster_index = 0; cluster_index < 2; ++cluster_index)
	{
		engine::Renderer_Config config = hidden_config();
		config.cluster_enable = cluster_index != 0;
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(config));
		//开启态需要计算路径
		if (config.cluster_enable)
		{
			if (!renderer.cluster_supported())
				GTEST_SKIP() << "本机不支持计算路径，无法覆盖簇缓冲上传";
			ASSERT_TRUE(renderer.cluster_enable_set(true));
		}
		renderer.camera_position_set(0.0f, 0.0f, 5.0f);
		renderer.camera_target_set(0.0f, 0.0f, 0.0f);
		engine::Mesh_Handle mesh = 0;
		ASSERT_TRUE(renderer.mesh_load(mesh, quad_data()));
		//一盏点光（半径 10 保守覆盖全簇）
		const float position[3] = { 0.0f, 0.0f, 0.0f };
		const float color[3] = { 1.0f, 1.0f, 1.0f };
		ASSERT_TRUE(renderer.light_add_point(position, color, 10.0f, 0.5f, 0.1f));
		//逐帧绘制并回读（同上：每帧都带回读）
		std::vector<uint8_t> pixels(pixel_count, 0u);
		for (uint32_t frame = 0; frame < 3; ++frame)
		{
			ASSERT_TRUE(renderer.mesh_draw(mesh, nullptr));
			ASSERT_TRUE(renderer.frame_render(pixels.data(), pixel_count, nullptr, nullptr));
		}
		engine::Frame_Stats stats = renderer.frame_stats_get();
		//关闭：零上传
		if (cluster_index == 0)
			EXPECT_EQ(stats.cluster_gpu_bytes, 0u);
		else
		{
			//开启：条目 > 0，上传量 = 计数 ＋ 偏移（各 3456 × 4）＋ 列表（条目 × 4）
			EXPECT_GT(stats.cluster_entries, 0u);
			EXPECT_EQ(stats.cluster_gpu_bytes, 3456u * 8u + stats.cluster_entries * 4u);
		}
		results.push_back(std::move(pixels));
		renderer.mesh_unload(mesh);
	}
	//两态画面逐字节一致（着色器本轮不读取簇缓冲）
	const bool same = std::mismatch(results[0].begin(), results[0].end(), results[1].begin()).first
		== results[0].end();
	EXPECT_TRUE(same) << "簇缓冲接入后开 / 关两态画面不一致";
}


/*
片元簇采样：八盏光源等价（第二百八十九轮）
口径：六盏点光 ＋ 两盏聚光（半径均大到保守覆盖全屏全深度），
Cluster 开启时片元经逐簇列表累加，关闭时走原 8 盏 UBO 前向循环，两态画面须逐字节一致。
*/
TEST(Renderer_Cluster_Test, 片元簇采样_八盏等价)
{
	const uint32_t pixel_count = WINDOW_WIDTH * WINDOW_HEIGHT * 4u;
	std::vector<std::vector<uint8_t>> results;
	for (uint32_t cluster_index = 0; cluster_index < 2; ++cluster_index)
	{
		engine::Renderer_Config config = hidden_config();
		config.msaa_samples = 1;
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(config));
		//开启态需要计算路径
		if (cluster_index != 0)
		{
			if (!renderer.cluster_supported())
				GTEST_SKIP() << "本机不支持计算路径，簇采样路径不可用";
			ASSERT_TRUE(renderer.cluster_enable_set(true));
		}
		uint64_t texture = 0;
		std::vector<uint8_t> texels(2u * 2u * 4u, 255u);
		ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
		renderer.texture_bind_set(texture);
		//关闭主方向光（满照会饱和画面，掩盖点光差异）
		const float no_direction[3] = { 0.0f, 0.0f, -1.0f };
		const float no_color[3] = { 1.0f, 1.0f, 1.0f };
		renderer.light_set(no_direction, no_color, 0.0f, 0.0f);
		engine::Mesh_Handle mesh = 0;
		ASSERT_TRUE(renderer.mesh_load(mesh, quad_data_lit()));
		//六盏点光 ＋ 两盏聚光（半径 20，均保守覆盖全部簇）
		const float color[3] = { 0.3f, 0.5f, 0.7f };
		for (uint32_t index = 0; index < 6; ++index)
		{
			const float position[3] = { static_cast<float>(index) - 2.5f, 0.0f, 1.5f };
			ASSERT_TRUE(renderer.light_add_point(position, color, 20.0f, 0.05f, 0.0f));
		}
		const float direction[3] = { 0.0f, 0.0f, -1.0f };
		for (uint32_t index = 0; index < 2; ++index)
		{
			const float position[3] = { index == 0 ? -4.0f : 4.0f, 0.0f, 1.5f };
			ASSERT_TRUE(renderer.light_add_spot(position, direction, color, 20.0f, 0.05f, 30.0f, 45.0f, 0.0f));
		}
		//逐帧绘制并回读（本机连续「无回读帧」后再请求回读会得到整幅清屏色，故每帧都带回读）
		std::vector<uint8_t> pixels(pixel_count, 0u);
		for (uint32_t frame = 0; frame < 3; ++frame)
		{
			ASSERT_TRUE(renderer.mesh_draw(mesh, nullptr));
			ASSERT_TRUE(renderer.frame_render(pixels.data(), pixel_count, nullptr, nullptr));
		}
		//开启态：簇条目已整备并上传
		if (cluster_index != 0)
			EXPECT_GT(renderer.frame_stats_get().cluster_entries, 0u);
		results.push_back(std::move(pixels));
		renderer.mesh_unload(mesh);
		renderer.texture_unload(texture);
	}
	//两态画面逐字节一致（同一 8 盏光源、同一累加顺序）
	const bool same = std::mismatch(results[0].begin(), results[0].end(), results[1].begin()).first
		== results[0].end();
	EXPECT_TRUE(same) << "八盏光源下簇采样与原前向路径画面不一致";
}

/*
片元簇采样：九盏光源扩展（第二百八十九轮）
口径：Cluster 关闭时第 9 盏被拒并置 `light_truncated`；开启时第 9 盏参与照明，
故两态画面必然不同（证明片元确实读取了簇列表与光源记录）。
*/
TEST(Renderer_Cluster_Test, 片元簇采样_九盏扩展)
{
	const uint32_t pixel_count = WINDOW_WIDTH * WINDOW_HEIGHT * 4u;
	std::vector<std::vector<uint8_t>> results;
	for (uint32_t cluster_index = 0; cluster_index < 2; ++cluster_index)
	{
		engine::Renderer_Config config = hidden_config();
		config.msaa_samples = 1;
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(config));
		//开启态需要计算路径
		if (cluster_index != 0)
		{
			if (!renderer.cluster_supported())
				GTEST_SKIP() << "本机不支持计算路径，簇采样路径不可用";
			ASSERT_TRUE(renderer.cluster_enable_set(true));
		}
		uint64_t texture = 0;
		std::vector<uint8_t> texels(2u * 2u * 4u, 255u);
		ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
		renderer.texture_bind_set(texture);
		//关闭主方向光（满照会饱和画面，掩盖第 9 盏差异）
		const float no_direction[3] = { 0.0f, 0.0f, -1.0f };
		const float no_color[3] = { 1.0f, 1.0f, 1.0f };
		renderer.light_set(no_direction, no_color, 0.0f, 0.0f);
		engine::Mesh_Handle mesh = 0;
		ASSERT_TRUE(renderer.mesh_load(mesh, quad_data_lit()));
		//前 8 盏（半径 20 覆盖全部簇；强度压低避免画面饱和后第 9 盏不可见）
		const float color[3] = { 0.4f, 0.5f, 0.6f };
		for (uint32_t index = 0; index < 8; ++index)
		{
			const float position[3] = { static_cast<float>(index) - 3.5f, 0.0f, 1.5f };
			ASSERT_TRUE(renderer.light_add_point(position, color, 20.0f, 0.02f, 0.0f));
		}
		//第 9 盏：关闭时被拒，开启时接受（正前方高亮，确保产生可见照度差）
		const float ninth_position[3] = { 0.0f, 0.0f, 3.0f };
		const bool ninth_added = renderer.light_add_point(ninth_position, color, 20.0f, 0.5f, 0.0f);
		if (cluster_index == 0)
			EXPECT_FALSE(ninth_added);
		else
			ASSERT_TRUE(ninth_added);
		//逐帧绘制并回读（本机连续「无回读帧」后再请求回读会得到整幅清屏色，故每帧都带回读）
		std::vector<uint8_t> pixels(pixel_count, 0u);
		for (uint32_t frame = 0; frame < 3; ++frame)
		{
			ASSERT_TRUE(renderer.mesh_draw(mesh, nullptr));
			ASSERT_TRUE(renderer.frame_render(pixels.data(), pixel_count, nullptr, nullptr));
		}
		engine::Frame_Stats stats = renderer.frame_stats_get();
		//关闭态：截断标志置 1；开启态：无截断
		EXPECT_EQ(stats.light_truncated, cluster_index == 0 ? 1u : 0u);
		results.push_back(std::move(pixels));
		renderer.mesh_unload(mesh);
		renderer.texture_unload(texture);
	}
	//两态画面不同（第 9 盏在开启态参与照明）
	const bool same = std::mismatch(results[0].begin(), results[0].end(), results[1].begin()).first
		== results[0].end();
	EXPECT_FALSE(same) << "第 9 盏在簇采样开启态未产生可见差异";
}

/*
片元簇采样：MSAA 4× 等价（第二百九十轮）
口径：4× MSAA ＋ 六点光 ＋ 两聚光（覆盖全簇），真实光照下开 / 关两态画面逐字节一致；
确认簇分支与多重采样渲染路径（含解析）组合后不产生像素差异。
*/
TEST(Renderer_Cluster_Test, 片元簇采样_MSAA四倍等价)
{
	const uint32_t pixel_count = WINDOW_WIDTH * WINDOW_HEIGHT * 4u;
	std::vector<std::vector<uint8_t>> results;
	for (uint32_t cluster_index = 0; cluster_index < 2; ++cluster_index)
	{
		engine::Renderer_Config config = hidden_config();
		config.msaa_samples = 4;
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(config));
		//开启态需要计算路径
		if (cluster_index != 0)
		{
			if (!renderer.cluster_supported())
				GTEST_SKIP() << "本机不支持计算路径，簇采样路径不可用";
			ASSERT_TRUE(renderer.cluster_enable_set(true));
		}
		uint64_t texture = 0;
		std::vector<uint8_t> texels(2u * 2u * 4u, 255u);
		ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
		renderer.texture_bind_set(texture);
		//关闭主方向光（满照会饱和画面）
		const float no_direction[3] = { 0.0f, 0.0f, -1.0f };
		const float no_color[3] = { 1.0f, 1.0f, 1.0f };
		renderer.light_set(no_direction, no_color, 0.0f, 0.0f);
		engine::Mesh_Handle mesh = 0;
		ASSERT_TRUE(renderer.mesh_load(mesh, quad_data_lit()));
		//六盏点光 ＋ 两盏聚光（半径 20，均保守覆盖全部簇）
		const float color[3] = { 0.3f, 0.5f, 0.7f };
		for (uint32_t index = 0; index < 6; ++index)
		{
			const float position[3] = { static_cast<float>(index) - 2.5f, 0.0f, 1.5f };
			ASSERT_TRUE(renderer.light_add_point(position, color, 20.0f, 0.05f, 0.0f));
		}
		const float direction[3] = { 0.0f, 0.0f, -1.0f };
		for (uint32_t index = 0; index < 2; ++index)
		{
			const float position[3] = { index == 0 ? -4.0f : 4.0f, 0.0f, 1.5f };
			ASSERT_TRUE(renderer.light_add_spot(position, direction, color, 20.0f, 0.05f, 30.0f, 45.0f, 0.0f));
		}
		//逐帧绘制并回读
		std::vector<uint8_t> pixels(pixel_count, 0u);
		for (uint32_t frame = 0; frame < 3; ++frame)
		{
			ASSERT_TRUE(renderer.mesh_draw(mesh, nullptr));
			ASSERT_TRUE(renderer.frame_render(pixels.data(), pixel_count, nullptr, nullptr));
		}
		results.push_back(std::move(pixels));
		renderer.mesh_unload(mesh);
		renderer.texture_unload(texture);
	}
	//两态画面逐字节一致（4× MSAA 解析后）
	const bool same = std::mismatch(results[0].begin(), results[0].end(), results[1].begin()).first
		== results[0].end();
	EXPECT_TRUE(same) << "MSAA 4× 下簇采样与原前向路径画面不一致";
}

/*
大光源场景：512 盏（第二百九十一轮）
口径：Cluster 开启登记 512 盏点光（记录 514 条），场景正常出帧且簇条目 / 上传量非零；
CPU 与 GPU 整备结果为同一份计数，并输出两路径耗时（每路径 2 次取最小）。
*/
TEST(Renderer_Cluster_Test, 大光源场景_五百一十二盏)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.cluster_supported())
	{
		GTEST_SKIP() << "本机不支持计算路径，大光源场景不可用";
	}
	ASSERT_TRUE(renderer.cluster_enable_set(true));
	constexpr uint32_t LIGHT_COUNT = 512;
	const float color[3] = { 0.6f, 0.7f, 0.8f };
	//32 × 16 网格分布（相机前方，半径 2 覆盖局部簇）
	for (uint32_t index = 0; index < LIGHT_COUNT; ++index)
	{
		const float position[3] = { static_cast<float>(index % 32) * 0.4f - 6.4f,
			static_cast<float>(index / 32) * 0.3f - 2.4f, 1.0f };
		ASSERT_TRUE(renderer.light_add_point(position, color, 2.0f, 0.05f, 0.0f));
	}
	EXPECT_EQ(renderer.point_light_count(), LIGHT_COUNT);
	EXPECT_EQ(renderer.light_record_count(), LIGHT_COUNT + 2u);

	//出帧（逐帧回读口径）
	uint64_t texture = 0;
	std::vector<uint8_t> texels(2u * 2u * 4u, 255u);
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	renderer.texture_bind_set(texture);
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data_lit()));
	const uint32_t pixel_count = WINDOW_WIDTH * WINDOW_HEIGHT * 4u;
	std::vector<uint8_t> pixels(pixel_count, 0u);
	for (uint32_t frame = 0; frame < 2; ++frame)
	{
		ASSERT_TRUE(renderer.mesh_draw(mesh, nullptr));
		ASSERT_TRUE(renderer.frame_render(pixels.data(), pixel_count, nullptr, nullptr));
	}
	//簇条目与上传量非零
	const engine::Frame_Stats stats = renderer.frame_stats_get();
	EXPECT_GT(stats.cluster_entries, 0u);
	EXPECT_GT(stats.cluster_gpu_bytes, 0u);

	//CPU / GPU 整备对照（列表容量上界 = 簇数 × 记录数）
	constexpr uint32_t CLUSTERS = 3456;
	constexpr uint32_t LIST_CAPACITY = CLUSTERS * (LIGHT_COUNT + 2u);
	std::vector<uint32_t> counts_cpu(CLUSTERS);
	std::vector<uint32_t> offsets_cpu(CLUSTERS);
	std::vector<uint32_t> list_cpu(LIST_CAPACITY);
	std::vector<uint32_t> counts_gpu(CLUSTERS);
	std::vector<uint32_t> offsets_gpu(CLUSTERS);
	std::vector<uint32_t> list_gpu(LIST_CAPACITY);
	uint32_t cpu_entries = 0;
	uint32_t gpu_entries = 0;
	double cpu_best_ms = 1e9;
	double gpu_best_ms = 1e9;
	for (uint32_t run = 0; run < 2; ++run)
	{
		ASSERT_TRUE(renderer.cluster_data_build(counts_cpu.data(), offsets_cpu.data(), list_cpu.data(),
			CLUSTERS, LIST_CAPACITY, &cpu_entries, false));
		cpu_best_ms = (std::min)(cpu_best_ms, renderer.cluster_build_ms_get());
		ASSERT_TRUE(renderer.cluster_data_build(counts_gpu.data(), offsets_gpu.data(), list_gpu.data(),
			CLUSTERS, LIST_CAPACITY, &gpu_entries, true));
		gpu_best_ms = (std::min)(gpu_best_ms, renderer.cluster_build_ms_get());
	}
	//两路径同一份计数
	EXPECT_EQ(cpu_entries, gpu_entries);
	for (uint32_t cluster = 0; cluster < CLUSTERS; ++cluster)
		EXPECT_EQ(counts_cpu[cluster], counts_gpu[cluster]);
	std::cout << "[大光源512] CPU " << cpu_best_ms << " ms / GPU " << gpu_best_ms << " ms | 条目 "
		<< cpu_entries << std::endl;

	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

/*
簇网格切换与缓冲重建（第二百九十二轮）
口径：运行期切换网格（3456 → 512 → 4032 簇），图形 set0 簇缓冲按容量增长重建并重写绑定；
每档出帧后 `cluster_gpu_bytes = 簇数 × 8 ＋ 条目 × 4`，且条目 > 0。
*/
TEST(Renderer_Cluster_Test, 簇网格切换与缓冲重建)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.cluster_supported())
	{
		GTEST_SKIP() << "本机不支持计算路径，簇缓冲重建不可用";
	}
	ASSERT_TRUE(renderer.cluster_enable_set(true));
	//一盏点光（半径 10 保守覆盖全簇）
	const float position[3] = { 0.0f, 0.0f, 0.0f };
	const float color[3] = { 1.0f, 1.0f, 1.0f };
	ASSERT_TRUE(renderer.light_add_point(position, color, 10.0f, 0.5f, 0.1f));
	uint64_t texture = 0;
	std::vector<uint8_t> texels(2u * 2u * 4u, 255u);
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	renderer.texture_bind_set(texture);
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data_lit()));
	//三档网格：默认 3456 → 缩小 512 → 超出总簇上限被收缩到 4032（32 × 18 × 7）
	const uint32_t grid_sets[3][3] = {
		{ 16, 9, 24 },
		{ 8, 8, 8 },
		{ 32, 18, 24 },
	};
	const uint32_t expected_total[3] = { 3456u, 512u, 4032u };
	const uint32_t pixel_count = WINDOW_WIDTH * WINDOW_HEIGHT * 4u;
	std::vector<uint8_t> pixels(pixel_count, 0u);
	for (uint32_t stage = 0; stage < 3; ++stage)
	{
		ASSERT_TRUE(renderer.cluster_grid_set(grid_sets[stage][0], grid_sets[stage][1], grid_sets[stage][2]));
		EXPECT_EQ(renderer.cluster_grid_cluster_count(), expected_total[stage]);
		ASSERT_TRUE(renderer.mesh_draw(mesh, nullptr));
		ASSERT_TRUE(renderer.frame_render(pixels.data(), pixel_count, nullptr, nullptr));
		const engine::Frame_Stats stats = renderer.frame_stats_get();
		//每档：条目 > 0 且上传量 = 簇数 × 8 ＋ 条目 × 4
		EXPECT_GT(stats.cluster_entries, 0u);
		EXPECT_EQ(stats.cluster_gpu_bytes, expected_total[stage] * 8u + stats.cluster_entries * 4u);
	}
	//恢复默认网格
	ASSERT_TRUE(renderer.cluster_grid_set(16, 9, 24));
	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}
