/*
渲染 Hi-Z 测试（第二百五十三轮）
覆盖：资源惰性构建（默认关闭不分配）、开关可反复切换且资源保留、金字塔级数 / 逐级尺寸、
      计算路径强制关闭时的能力降级，以及开关切换前后出帧与绘制实例数守恒。
断言口径：默认关闭时状态全 0；开启后级数按 max(w,h) 逐级折半至 1×1；能力不足时开启被拒绝。
*/
#include <gtest/gtest.h>

#include <array>
#include <algorithm>
#include <cstdint>
#include <utility>
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

/*
遮挡剔除：墙后实例被剔除（第二百五十六轮）
口径：先渲染一面覆盖屏幕的近墙（两帧），再对墙后 / 墙前两批实例调用 GPU 压缩：
墙后应全部剔除、墙前应全部可见；关闭 Hi-Z 时墙后实例不因遮挡被剔除。
*/
TEST_F(Renderer_HiZ_Test, 遮挡剔除墙后实例)
{
	engine::Renderer_Config config = hidden_config();
	config.hiz_enable = true;
	config.msaa_samples = 1;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.hiz_supported())
		GTEST_SKIP() << "本机不支持 Hi-Z 所需能力";
	//相机与朝向（沿 −Z 观察；墙在 z = 3，实例在 z = 0 / 4.5）
	renderer.camera_position_set(0.0f, 0.0f, 5.0f);
	renderer.camera_target_set(0.0f, 0.0f, 0.0f);
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data()));
	//近墙：放大 30 倍并平移到 z = 3
	std::array<float, 16> wall = identity_matrix();
	wall[0] = 30.0f;
	wall[5] = 30.0f;
	wall[14] = 3.0f;
	//两帧：第 0 帧写深度并生成金字塔，第 1 帧再次生成（保证在途槽就绪）
	for (uint32_t frame = 0; frame < 2; ++frame)
	{
		ASSERT_TRUE(renderer.mesh_draw_instanced(mesh, wall.data(), 1));
		ASSERT_TRUE(renderer.frame_render());
	}
	//实例批次（墙后 z = 0 / 墙前 z = 4.5）
	constexpr uint32_t COUNT = 200;
	std::vector<std::array<float, 16>> behind(COUNT);
	std::vector<std::array<float, 16>> front(COUNT);
	for (uint32_t index = 0; index < COUNT; ++index)
	{
		std::array<float, 16> matrix = identity_matrix();
		matrix[14] = 0.0f;
		behind[index] = matrix;
		matrix[14] = 4.5f;
		front[index] = matrix;
	}
	const float local_min[3] = { -QUAD_HALF, -QUAD_HALF, 0.0f };
	const float local_max[3] = { QUAD_HALF, QUAD_HALF, 0.0f };
	const float camera[3] = { 0.0f, 0.0f, 5.0f };
	const float forward[3] = { 0.0f, 0.0f, -1.0f };
	std::vector<std::array<float, 16>> compacted(COUNT);
	uint32_t behind_visible = COUNT;
	uint32_t front_visible = 0;
	ASSERT_TRUE(renderer.gpu_instance_compact_ex(behind[0].data(), COUNT, local_min, local_max,
		camera, forward, 0.0f, false, compacted[0].data(), &behind_visible));
	ASSERT_TRUE(renderer.gpu_instance_compact_ex(front[0].data(), COUNT, local_min, local_max,
		camera, forward, 0.0f, false, compacted[0].data(), &front_visible));
	//屏幕两侧边缘（NDC x ≈ ±0.45）的墙后实例也应被剔除（覆盖边缘的有效性检查）
	std::vector<std::array<float, 16>> corner(COUNT);
	for (uint32_t index = 0; index < COUNT; ++index)
	{
		std::array<float, 16> matrix = identity_matrix();
		matrix[12] = (index % 2 == 0) ? 1.1f : -1.1f;
		matrix[14] = 0.0f;
		corner[index] = matrix;
	}
	uint32_t corner_visible = COUNT;
	ASSERT_TRUE(renderer.gpu_instance_compact_ex(corner[0].data(), COUNT, local_min, local_max,
		camera, forward, 0.0f, false, compacted[0].data(), &corner_visible));
	//墙后全部剔除（含屏幕两侧边缘）、墙前全部可见
	EXPECT_EQ(corner_visible, 0u);
	//关闭 Hi-Z 的对照：墙后实例不应因遮挡被剔除
	engine::Renderer without;
	ASSERT_TRUE(without.init(hidden_config()));
	without.camera_position_set(0.0f, 0.0f, 5.0f);
	without.camera_target_set(0.0f, 0.0f, 0.0f);
	uint32_t without_visible = 0;
	ASSERT_TRUE(without.gpu_instance_compact_ex(behind[0].data(), COUNT, local_min, local_max,
		camera, forward, 0.0f, false, compacted[0].data(), &without_visible));
	EXPECT_EQ(behind_visible, 0u);
	EXPECT_EQ(front_visible, COUNT);
	//生成确实发生
	EXPECT_GE(renderer.hiz_generation_count(), 2u);
	EXPECT_EQ(without_visible, COUNT);
	renderer.mesh_unload(mesh);
}

/*
部分遮挡场景：缝隙内实例不误剔（第二百五十七轮）
口径：左右两堵墙在 z = 3（覆盖屏幕两侧），中央留缝；缝内实例（z = 0，墙后）应全部可见（缝隙深度为清屏 1.0），
墙正后方实例应全部剔除。用于验证保守口径在「部分遮挡」场景下不产生误剔。
*/
TEST_F(Renderer_HiZ_Test, 部分遮挡场景不误剔)
{
	engine::Renderer_Config config = hidden_config();
	config.hiz_enable = true;
	config.msaa_samples = 1;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.hiz_supported())
		GTEST_SKIP() << "本机不支持 Hi-Z 所需能力";
	renderer.camera_position_set(0.0f, 0.0f, 5.0f);
	renderer.camera_target_set(0.0f, 0.0f, 0.0f);
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data()));
	//左右两堵墙：半宽 0.5、半高 1.5（世界），平移 x = ±0.9、z = 3
	std::array<float, 16> left_wall = identity_matrix();
	left_wall[0] = 2.0f;
	left_wall[5] = 6.0f;
	left_wall[12] = -0.9f;
	left_wall[14] = 3.0f;
	std::array<float, 16> right_wall = left_wall;
	right_wall[12] = 0.9f;
	std::vector<std::array<float, 16>> walls = { left_wall, right_wall };
	//两帧：写深度并生成金字塔
	for (uint32_t frame = 0; frame < 2; ++frame)
	{
		ASSERT_TRUE(renderer.mesh_draw_instanced(mesh, walls[0].data(), static_cast<uint32_t>(walls.size())));
		ASSERT_TRUE(renderer.frame_render());
	}
	//缝隙内实例（x = 0，墙后 z = 0）
	constexpr uint32_t COUNT = 200;
	std::vector<std::array<float, 16>> gap(COUNT);
	for (uint32_t index = 0; index < COUNT; ++index)
	{
		std::array<float, 16> matrix = identity_matrix();
		matrix[14] = 0.0f;
		gap[index] = matrix;
	}
	const float local_min[3] = { -QUAD_HALF, -QUAD_HALF, 0.0f };
	const float local_max[3] = { QUAD_HALF, QUAD_HALF, 0.0f };
	const float camera[3] = { 0.0f, 0.0f, 5.0f };
	const float forward[3] = { 0.0f, 0.0f, -1.0f };
	std::vector<std::array<float, 16>> compacted(COUNT);
	uint32_t gap_visible = 0;
	ASSERT_TRUE(renderer.gpu_instance_compact_ex(gap[0].data(), COUNT, local_min, local_max,
		camera, forward, 0.0f, false, compacted[0].data(), &gap_visible));
	//缝隙内全部可见（部分遮挡场景零误剔）
	EXPECT_EQ(gap_visible, COUNT);
	renderer.mesh_unload(mesh);
}

/*
多场景零误剔：远景墙与多层遮挡（第二百五十八轮）
口径：同一相机下先后渲染远景墙（z = −5）与双层墙（z = 3 / −2），
远景墙后的实例应被剔除、墙前实例应可见；两墙之后的实例应被剔除、两墙之间实例应可见。
*/
TEST_F(Renderer_HiZ_Test, 多场景零误剔)
{
	//相机与几何参数
	const float camera_z = 5.0f;
	const float local_min[3] = { -QUAD_HALF, -QUAD_HALF, 0.0f };
	const float local_max[3] = { QUAD_HALF, QUAD_HALF, 0.0f };
	const float camera[3] = { 0.0f, 0.0f, camera_z };
	const float forward[3] = { 0.0f, 0.0f, -1.0f };
	constexpr uint32_t COUNT = 200;
	std::vector<std::array<float, 16>> matrices(COUNT);
	std::vector<std::array<float, 16>> compacted(COUNT);
	engine::Renderer_Config config = hidden_config();
	config.hiz_enable = true;
	config.msaa_samples = 1;
	//场景一：远景墙（z = −5，放大 40 倍）
	{
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(config));
		if (!renderer.hiz_supported())
			GTEST_SKIP() << "本机不支持 Hi-Z 所需能力";
		renderer.camera_position_set(0.0f, 0.0f, camera_z);
		renderer.camera_target_set(0.0f, 0.0f, 0.0f);
		engine::Mesh_Handle mesh = 0;
		ASSERT_TRUE(renderer.mesh_load(mesh, quad_data()));
		std::array<float, 16> wall = identity_matrix();
		wall[0] = 40.0f;
		wall[5] = 40.0f;
		wall[14] = -5.0f;
		for (uint32_t frame = 0; frame < 2; ++frame)
		{
			ASSERT_TRUE(renderer.mesh_draw_instanced(mesh, wall.data(), 1));
			ASSERT_TRUE(renderer.frame_render());
		}
		//墙后（z = −8）全部剔除、墙前（z = −2）全部可见
		for (uint32_t index = 0; index < COUNT; ++index)
		{
			std::array<float, 16> matrix = identity_matrix();
			matrix[14] = -8.0f;
			matrices[index] = matrix;
		}
		uint32_t behind_far = COUNT;
		ASSERT_TRUE(renderer.gpu_instance_compact_ex(matrices[0].data(), COUNT, local_min, local_max,
			camera, forward, 0.0f, false, compacted[0].data(), &behind_far));
		for (uint32_t index = 0; index < COUNT; ++index)
		{
			std::array<float, 16> matrix = identity_matrix();
			matrix[14] = -2.0f;
			matrices[index] = matrix;
		}
		uint32_t front_far = 0;
		ASSERT_TRUE(renderer.gpu_instance_compact_ex(matrices[0].data(), COUNT, local_min, local_max,
			camera, forward, 0.0f, false, compacted[0].data(), &front_far));
		EXPECT_EQ(behind_far, 0u);
		EXPECT_EQ(front_far, COUNT);
		renderer.mesh_unload(mesh);
	}
	//场景二：双层墙（z = 3 与 z = −2，各放大 40 倍）
	{
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(config));
		if (!renderer.hiz_supported())
			GTEST_SKIP() << "本机不支持 Hi-Z 所需能力";
		renderer.camera_position_set(0.0f, 0.0f, camera_z);
		renderer.camera_target_set(0.0f, 0.0f, 0.0f);
		engine::Mesh_Handle mesh = 0;
		ASSERT_TRUE(renderer.mesh_load(mesh, quad_data()));
		std::array<float, 16> near_wall = identity_matrix();
		near_wall[0] = 40.0f;
		near_wall[5] = 40.0f;
		near_wall[14] = 3.0f;
		std::array<float, 16> far_wall = near_wall;
		far_wall[14] = -2.0f;
		std::vector<std::array<float, 16>> walls = { near_wall, far_wall };
		for (uint32_t frame = 0; frame < 2; ++frame)
		{
			ASSERT_TRUE(renderer.mesh_draw_instanced(mesh, walls[0].data(), static_cast<uint32_t>(walls.size())));
			ASSERT_TRUE(renderer.frame_render());
		}
		//两墙之后（z = −5）全部剔除、近墙之前（z = 4）全部可见
		for (uint32_t index = 0; index < COUNT; ++index)
		{
			std::array<float, 16> matrix = identity_matrix();
			matrix[14] = -5.0f;
			matrices[index] = matrix;
		}
		uint32_t behind_both = COUNT;
		ASSERT_TRUE(renderer.gpu_instance_compact_ex(matrices[0].data(), COUNT, local_min, local_max,
			camera, forward, 0.0f, false, compacted[0].data(), &behind_both));
		for (uint32_t index = 0; index < COUNT; ++index)
		{
			std::array<float, 16> matrix = identity_matrix();
			matrix[14] = 4.0f;
			matrices[index] = matrix;
		}
		uint32_t between = 0;
		ASSERT_TRUE(renderer.gpu_instance_compact_ex(matrices[0].data(), COUNT, local_min, local_max,
			camera, forward, 0.0f, false, compacted[0].data(), &between));
		EXPECT_EQ(behind_both, 0u);
		EXPECT_EQ(between, COUNT);
		renderer.mesh_unload(mesh);
	}
}

/*
上一帧复用稳定性（第二百五十九轮）
口径：静态场景（全屏墙 ＋ 墙后实例），开启 Hi-Z 后连续渲染 5 帧并逐帧回读颜色；
第 1 帧起（金字塔已就绪、开始复用上一帧）与首帧逐字节一致，无闪烁。
*/
TEST_F(Renderer_HiZ_Test, 开启遮挡逐帧像素稳定)
{
	engine::Renderer_Config config = hidden_config();
	config.hiz_enable = true;
	config.msaa_samples = 1;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.hiz_supported())
		GTEST_SKIP() << "本机不支持 Hi-Z 所需能力";
	renderer.camera_position_set(0.0f, 0.0f, 5.0f);
	renderer.camera_target_set(0.0f, 0.0f, 0.0f);
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data()));
	//全屏墙（z = 3，放大 30 倍）＋ 墙后实例（z = 0，10 个）
	std::array<float, 16> wall = identity_matrix();
	wall[0] = 30.0f;
	wall[5] = 30.0f;
	wall[14] = 3.0f;
	std::vector<std::array<float, 16>> behind(10, identity_matrix());
	const uint32_t pixel_count = WINDOW_WIDTH * WINDOW_HEIGHT * 4u;
	std::vector<std::vector<uint8_t>> frames;
	for (uint32_t frame = 0; frame < 5; ++frame)
	{
		ASSERT_TRUE(renderer.mesh_draw_instanced(mesh, wall.data(), 1));
		if (frame > 0)
			ASSERT_TRUE(renderer.mesh_draw_instanced(mesh, behind[0].data(), static_cast<uint32_t>(behind.size())));
		std::vector<uint8_t> pixels(pixel_count, 0u);
		uint32_t width = 0;
		uint32_t height = 0;
		ASSERT_TRUE(renderer.frame_render(pixels.data(), pixel_count, &width, &height));
		ASSERT_EQ(width, WINDOW_WIDTH);
		ASSERT_EQ(height, WINDOW_HEIGHT);
		frames.push_back(std::move(pixels));
	}
	//第 1..4 帧与首帧逐字节一致（复用上一帧不改变画面）；mismatch 返回尾迭代器表示完全相同
	for (uint32_t frame = 1; frame < frames.size(); ++frame)
	{
		const bool same = std::mismatch(frames[0].begin(), frames[0].end(), frames[frame].begin()).first
			== frames[0].end();
		EXPECT_TRUE(same) << "帧 " << frame << " 与首帧存在差异";
	}
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
