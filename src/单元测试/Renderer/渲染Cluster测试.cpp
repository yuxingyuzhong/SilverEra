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
		for (uint32_t frame = 0; frame < 3; ++frame)
		{
			ASSERT_TRUE(renderer.mesh_draw_instanced(mesh, wall.data(), 1));
			ASSERT_TRUE(renderer.mesh_draw_instanced(mesh, behind[0].data(), static_cast<uint32_t>(behind.size())));
			ASSERT_TRUE(renderer.frame_render());
		}
		//末帧回读
		std::vector<uint8_t> pixels(pixel_count, 0u);
		uint32_t width = 0;
		uint32_t height = 0;
		ASSERT_TRUE(renderer.frame_render(pixels.data(), pixel_count, &width, &height));
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
	constexpr uint32_t LIST_CAPACITY = CLUSTERS * 10u;
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
	//三档光源规模：基础 2 条 / ＋2 点光 / ＋6 点光
	for (uint32_t stage = 0; stage < 3; ++stage)
	{
		uint32_t add_count = stage == 0 ? 0u : (stage == 1 ? 2u : 6u);
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
