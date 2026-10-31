/*
渲染 Hi-Z 测试（第二百五十三轮）
覆盖：资源惰性构建（默认关闭不分配）、开关可反复切换且资源保留、金字塔级数 / 逐级尺寸、
      计算路径强制关闭时的能力降级，以及开关切换前后出帧与绘制实例数守恒。
断言口径：默认关闭时状态全 0；开启后级数按 max(w,h) 逐级折半至 1×1；能力不足时开启被拒绝。
*/
#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <vector>

//获取渲染器门面
#include "Application/Renderer/src/render/Renderer/渲染器.h"

namespace
{
	//隐藏窗口尺寸（功能用例；64×36 逐级折半得到 7 级）
	constexpr uint32_t WINDOW_WIDTH = 64;
	constexpr uint32_t WINDOW_HEIGHT = 36;
	//预期级数（64→32→16→8→4→2→1；36→18→9→4→2→1→1）
	constexpr uint32_t EXPECTED_LEVELS = 7;
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

//Hi-Z 资源测试夹具
class Renderer_HiZ_Test : public ::testing::Test
{
};

/*
默认关闭且资源惰性
口径：初始化后开关为假、状态全 0；开启后才建立资源；关闭只置标志、资源保留供再次开启复用。
*/
TEST_F(Renderer_HiZ_Test, 默认关闭且资源惰性)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//能力不足则跳过（本机应具备计算与 R32 浮点存储图像）
	if (!renderer.hiz_supported())
		GTEST_SKIP() << "本机不支持 Hi-Z 所需能力（计算 ＋ R32 浮点存储图像）";
	EXPECT_FALSE(renderer.hiz_enable_get());
	//未开启时不存在资源
	uint32_t width = 1;
	uint32_t height = 1;
	uint32_t levels = 1;
	EXPECT_FALSE(renderer.hiz_state_get(width, height, levels));
	EXPECT_EQ(width, 0u);
	EXPECT_EQ(height, 0u);
	EXPECT_EQ(levels, 0u);
	//开启后资源建立
	ASSERT_TRUE(renderer.hiz_enable_set(true));
	EXPECT_TRUE(renderer.hiz_enable_get());
	ASSERT_TRUE(renderer.hiz_state_get(width, height, levels));
	EXPECT_EQ(width, WINDOW_WIDTH);
	EXPECT_EQ(height, WINDOW_HEIGHT);
	EXPECT_EQ(levels, EXPECTED_LEVELS);
	//关闭只置标志，资源保留
	ASSERT_TRUE(renderer.hiz_enable_set(false));
	EXPECT_FALSE(renderer.hiz_enable_get());
	EXPECT_TRUE(renderer.hiz_state_get(width, height, levels));
	EXPECT_EQ(levels, EXPECTED_LEVELS);
}

/*
金字塔级数与逐级尺寸
口径：级数 = 自 max(w,h) 逐级折半至 1×1 的级数；第 i 级尺寸 = (max(1, w >> i), max(1, h >> i))。
*/
TEST_F(Renderer_HiZ_Test, 金字塔级数与逐级尺寸)
{
	engine::Renderer_Config config = hidden_config();
	config.hiz_enable = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.hiz_supported())
		GTEST_SKIP() << "本机不支持 Hi-Z 所需能力";
	EXPECT_TRUE(renderer.hiz_enable_get());
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t levels = 0;
	ASSERT_TRUE(renderer.hiz_state_get(width, height, levels));
	EXPECT_EQ(width, WINDOW_WIDTH);
	EXPECT_EQ(height, WINDOW_HEIGHT);
	EXPECT_EQ(levels, EXPECTED_LEVELS);
	//逐级尺寸（64×36 折半链）
	const uint32_t expected[EXPECTED_LEVELS][2] = {
		{ 64, 36 }, { 32, 18 }, { 16, 9 }, { 8, 4 }, { 4, 2 }, { 2, 1 }, { 1, 1 },
	};
	for (uint32_t level = 0; level < EXPECTED_LEVELS; ++level)
	{
		ASSERT_TRUE(renderer.hiz_level_extent(level, width, height)) << "级 " << level;
		EXPECT_EQ(width, expected[level][0]) << "级 " << level;
		EXPECT_EQ(height, expected[level][1]) << "级 " << level;
	}
	//越界级返回假且出参归零
	EXPECT_FALSE(renderer.hiz_level_extent(EXPECTED_LEVELS, width, height));
	EXPECT_EQ(width, 0u);
	EXPECT_EQ(height, 0u);
}

/*
计算路径强制关闭时的降级
口径：`compute_enable = false` 时 Hi-Z 能力为假，开启请求被拒绝且开关保持关闭、无资源。
*/
TEST_F(Renderer_HiZ_Test, 计算关闭时不可用)
{
	engine::Renderer_Config config = hidden_config();
	config.compute_enable = false;
	config.hiz_enable = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	EXPECT_FALSE(renderer.hiz_supported());
	EXPECT_FALSE(renderer.hiz_enable_get());
	//开启被拒绝
	EXPECT_FALSE(renderer.hiz_enable_set(true));
	EXPECT_FALSE(renderer.hiz_enable_get());
	uint32_t width = 1;
	uint32_t height = 1;
	uint32_t levels = 1;
	EXPECT_FALSE(renderer.hiz_state_get(width, height, levels));
	EXPECT_EQ(levels, 0u);
}

/*
金字塔逐帧生成（第二百五十四轮）
口径：单采样路径开启 Hi-Z 后，每帧场景退场时录制「深度拷贝 ＋ 逐级降采样」；
生成次数随帧数增长，且在途帧槽均在两帧内各自生成过一次。
*/
TEST_F(Renderer_HiZ_Test, 金字塔逐帧生成)
{
	engine::Renderer_Config config = hidden_config();
	config.hiz_enable = true;
	config.msaa_samples = 1;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.hiz_supported())
		GTEST_SKIP() << "本机不支持 Hi-Z 所需能力";
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data()));
	//三个实例（同一网格三批）
	std::array<float, 16> identity = identity_matrix();
	std::vector<std::array<float, 16>> matrices = { identity, identity, identity };
	for (uint32_t frame = 0; frame < 3; ++frame)
	{
		ASSERT_TRUE(renderer.mesh_draw_instanced(mesh, matrices[0].data(), static_cast<uint32_t>(matrices.size())));
		ASSERT_TRUE(renderer.frame_render());
	}
	//三帧均完成生成
	EXPECT_GE(renderer.hiz_generation_count(), 3u);
	//两个在途帧槽均已有有效历史
	for (uint32_t slot = 0; slot < 2; ++slot)
		EXPECT_TRUE(renderer.hiz_history_ready(slot)) << "帧槽 " << slot;
	renderer.mesh_unload(mesh);
}

/*
关闭时不生成
口径：默认关闭时全部帧均不录制生成命令，历史保持无效、生成计数为 0。
*/
TEST_F(Renderer_HiZ_Test, 关闭时不生成)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data()));
	std::array<float, 16> identity = identity_matrix();
	for (uint32_t frame = 0; frame < 3; ++frame)
	{
		ASSERT_TRUE(renderer.mesh_draw_instanced(mesh, identity.data(), 1));
		ASSERT_TRUE(renderer.frame_render());
	}
	EXPECT_EQ(renderer.hiz_generation_count(), 0u);
	EXPECT_FALSE(renderer.hiz_history_ready(0));
	EXPECT_FALSE(renderer.hiz_history_ready(1));
	renderer.mesh_unload(mesh);
}

TEST_F(Renderer_HiZ_Test, 开关切换前后出帧守恒)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data()));
	//两个实例（同一网格两批）
	std::array<float, 16> identity = identity_matrix();
	std::array<float, 16> shifted = identity;
	shifted[12] = 0.6f;
	std::vector<std::array<float, 16>> matrices = { identity, shifted };
	//先关闭态出帧
	ASSERT_TRUE(renderer.mesh_draw_instanced(mesh, matrices[0].data(), static_cast<uint32_t>(matrices.size())));
	ASSERT_TRUE(renderer.frame_render());
	//关闭态本帧实际绘制实例数（统计为累计值，故取本帧前后差值）
	const engine::Frame_Stats off_stats = renderer.frame_stats_get();
	const uint64_t off_drawn = off_stats.drawn_instances;
	//开启 Hi-Z（能力不足时保持关闭，仍须正常出帧）
	if (renderer.hiz_supported())
		ASSERT_TRUE(renderer.hiz_enable_set(true));
	ASSERT_TRUE(renderer.mesh_draw_instanced(mesh, matrices[0].data(), static_cast<uint32_t>(matrices.size())));
	ASSERT_TRUE(renderer.frame_render());
	const engine::Frame_Stats on_stats = renderer.frame_stats_get();
	const uint64_t on_drawn = on_stats.drawn_instances - off_drawn;
	//两态本帧实际绘制实例数一致
	EXPECT_EQ(off_drawn, on_drawn);
	EXPECT_GE(on_drawn, 1u);
	renderer.mesh_unload(mesh);
}
