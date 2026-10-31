/*
渲染Cluster光照测试（第二百六十六轮）
覆盖：光源记录 SSBO 上传（未初始化拒绝 / 默认关闭不上传 / 开启后按帧上传 / 光源增减即时反映 / 关闭归零）。
*/
#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

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
