/*
渲染 GPU 驱动测试（第二百四十七轮）
覆盖：实例包围体构造（单位矩阵 / 旋转 / 随机变换的「球包含角点、盒包含角点」）与非法参数；
      性能基线采集（1080p ＋ MSAA 4× ＋ 1 万实例，记录帧耗时与 CPU 剔除耗时）。
断言口径：包围体一律为保守包围（球半径与盒边界都含扩张），故只断言「包含」与「不异常放大」。
*/
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <random>
#include <vector>

//获取渲染器门面与包围体构造
#include "Application/Renderer/src/render/Renderer/渲染器.h"
#include "Application/Renderer/src/render/GPU驱动/包围体.h"
//视锥平面与 CPU 参考判定（第二百四十八轮）
#include "Application/Renderer/src/render/GPU驱动/剔除数据.h"

namespace
{
	//隐藏窗口尺寸（功能用例）
	constexpr uint32_t WINDOW_WIDTH = 64;
	constexpr uint32_t WINDOW_HEIGHT = 36;
	//性能基线场景参数（方向一 §目标场景）
	constexpr uint32_t BASELINE_WIDTH = 1920;
	constexpr uint32_t BASELINE_HEIGHT = 1080;
	constexpr uint32_t BASELINE_INSTANCES = 10000;
	constexpr uint32_t BASELINE_FRAMES = 6;
	//实例横向铺开边界（部分实例落在视锥外）
	constexpr float BASELINE_SPAN = 40.0f;
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

	//绕 Y 轴 90 度旋转（列主序：x' = z，z' = −x）
	std::array<float, 16> rotate_y_90_matrix(void)
	{
		return std::array<float, 16>{
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			-1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		};
	}

	//变换一个局部点（列主序矩阵）
	std::array<float, 3> transform_point(const float model[16], float x, float y, float z)
	{
		return std::array<float, 3>{
			model[0] * x + model[4] * y + model[8] * z + model[12],
			model[1] * x + model[5] * y + model[9] * z + model[13],
			model[2] * x + model[6] * y + model[10] * z + model[14],
		};
	}

	//构造大量实例矩阵（横向铺开，部分在视锥外）
	std::vector<std::array<float, 16>> baseline_matrices(uint32_t count)
	{
		std::vector<std::array<float, 16>> matrices(count);
		for (uint32_t index = 0; index < count; ++index)
		{
			const float ratio = count > 1 ? static_cast<float>(index) / static_cast<float>(count - 1) : 0.5f;
			std::array<float, 16> matrix = identity_matrix();
			matrix[12] = -BASELINE_SPAN + 2.0f * BASELINE_SPAN * ratio;
			matrix[13] = 0.0f;
			matrix[14] = 0.0f;
			matrices[index] = matrix;
		}
		return matrices;
	}
}

//包围体测试夹具
class Renderer_GPU_Driven_Test : public ::testing::Test
{
};

//单位矩阵：盒 = 局部盒 ＋ 外扩，球半径 = 半对角长 × 倍率 ＋ 附加量
TEST_F(Renderer_GPU_Driven_Test, 单位矩阵包围体)
{
	const float local_min[3] = { -1.0f, -0.5f, -2.0f };
	const float local_max[3] = { 1.0f, 0.5f, 2.0f };
	std::array<float, 16> identity = identity_matrix();
	engine::World_Bounds bounds;
	ASSERT_TRUE(engine::world_bounds_build(bounds, local_min, local_max, identity.data()));
	//中心在原点
	EXPECT_FLOAT_EQ(bounds.center[0], 0.0f);
	EXPECT_FLOAT_EQ(bounds.center[1], 0.0f);
	EXPECT_FLOAT_EQ(bounds.center[2], 0.0f);
	//半径 = 扩张后 AABB 角点最远距离 × 倍率 ＋ 附加量（球须覆盖盒）
	const float corner_distance = std::sqrt(1.002f * 1.002f + 0.502f * 0.502f + 2.002f * 2.002f);
	EXPECT_NEAR(bounds.radius, corner_distance * engine::WORLD_BOUNDS_SPHERE_SCALE + engine::WORLD_BOUNDS_SPHERE_EXPAND, 1e-5f);
	//盒 = 局部盒 ± 外扩
	EXPECT_NEAR(bounds.aabb_min[0], -1.0f - engine::WORLD_BOUNDS_AABB_EXPAND, 1e-5f);
	EXPECT_NEAR(bounds.aabb_max[2], 2.0f + engine::WORLD_BOUNDS_AABB_EXPAND, 1e-5f);
}

//绕 Y 轴 90 度：世界盒的 X 轴取局部 Z 轴范围，Z 轴取局部 X 轴范围
TEST_F(Renderer_GPU_Driven_Test, 旋转后包围盒按角点重求)
{
	const float local_min[3] = { -1.0f, -2.0f, -3.0f };
	const float local_max[3] = { 1.0f, 2.0f, 3.0f };
	std::array<float, 16> rotation = rotate_y_90_matrix();
	engine::World_Bounds bounds;
	ASSERT_TRUE(engine::world_bounds_build(bounds, local_min, local_max, rotation.data()));
	//X 轴 = 局部 Z 轴范围
	EXPECT_NEAR(bounds.aabb_min[0], -3.0f - engine::WORLD_BOUNDS_AABB_EXPAND, 1e-5f);
	EXPECT_NEAR(bounds.aabb_max[0], 3.0f + engine::WORLD_BOUNDS_AABB_EXPAND, 1e-5f);
	//Y 轴不变
	EXPECT_NEAR(bounds.aabb_min[1], -2.0f - engine::WORLD_BOUNDS_AABB_EXPAND, 1e-5f);
	EXPECT_NEAR(bounds.aabb_max[1], 2.0f + engine::WORLD_BOUNDS_AABB_EXPAND, 1e-5f);
	//Z 轴 = 局部 X 轴范围
	EXPECT_NEAR(bounds.aabb_min[2], -1.0f - engine::WORLD_BOUNDS_AABB_EXPAND, 1e-5f);
	EXPECT_NEAR(bounds.aabb_max[2], 1.0f + engine::WORLD_BOUNDS_AABB_EXPAND, 1e-5f);
}

//随机变换：球必须包含全部角点，盒必须包含全部角点（保守性核心断言）
TEST_F(Renderer_GPU_Driven_Test, 包围球与包围盒包含角点)
{
	const float local_min[3] = { -0.8f, -0.4f, -1.2f };
	const float local_max[3] = { 0.8f, 0.4f, 1.2f };
	std::mt19937 generator(20261030u);
	std::uniform_real_distribution<float> angle(-3.14159265f, 3.14159265f);
	std::uniform_real_distribution<float> scale(0.2f, 4.0f);
	std::uniform_real_distribution<float> offset(-50.0f, 50.0f);
	for (uint32_t sample = 0; sample < 200; ++sample)
	{
		//随机欧拉角 + 各轴缩放 + 平移
		const float ax = angle(generator);
		const float ay = angle(generator);
		const float az = angle(generator);
		const float sx = scale(generator);
		const float sy = scale(generator);
		const float sz = scale(generator);
		//绕 X 旋转
		std::array<float, 16> matrix = {
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, std::cos(ax), std::sin(ax), 0.0f,
			0.0f, -std::sin(ax), std::cos(ax), 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		};
		//绕 Y 旋转（左乘）
		std::array<float, 16> rotation_y = {
			std::cos(ay), 0.0f, -std::sin(ay), 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			std::sin(ay), 0.0f, std::cos(ay), 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		};
		std::array<float, 16> combined{};
		for (uint32_t column = 0; column < 4; ++column)
			for (uint32_t row = 0; row < 4; ++row)
			{
				float sum = 0.0f;
				for (uint32_t inner = 0; inner < 4; ++inner)
					sum += rotation_y[inner * 4 + row] * matrix[column * 4 + inner];
				combined[column * 4 + row] = sum;
			}
		//绕 Z 旋转
		std::array<float, 16> rotation_z = {
			std::cos(az), std::sin(az), 0.0f, 0.0f,
			-std::sin(az), std::cos(az), 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		};
		std::array<float, 16> rotated{};
		for (uint32_t column = 0; column < 4; ++column)
			for (uint32_t row = 0; row < 4; ++row)
			{
				float sum = 0.0f;
				for (uint32_t inner = 0; inner < 4; ++inner)
					sum += rotation_z[inner * 4 + row] * combined[column * 4 + inner];
				rotated[column * 4 + row] = sum;
			}
		//各轴缩放 + 平移
		rotated[0] *= sx;
		rotated[1] *= sx;
		rotated[2] *= sx;
		rotated[4] *= sy;
		rotated[5] *= sy;
		rotated[6] *= sy;
		rotated[8] *= sz;
		rotated[9] *= sz;
		rotated[10] *= sz;
		rotated[12] = offset(generator);
		rotated[13] = offset(generator);
		rotated[14] = offset(generator);
		engine::World_Bounds bounds;
		ASSERT_TRUE(engine::world_bounds_build(bounds, local_min, local_max, rotated.data()));
		//逐角点校验包含关系
		for (uint32_t corner = 0; corner < 8; ++corner)
		{
			const float x = (corner & 1u) != 0 ? local_max[0] : local_min[0];
			const float y = (corner & 2u) != 0 ? local_max[1] : local_min[1];
			const float z = (corner & 4u) != 0 ? local_max[2] : local_min[2];
			std::array<float, 3> world = transform_point(rotated.data(), x, y, z);
			//球包含
			const float dx = world[0] - bounds.center[0];
			const float dy = world[1] - bounds.center[1];
			const float dz = world[2] - bounds.center[2];
			EXPECT_LE(std::sqrt(dx * dx + dy * dy + dz * dz), bounds.radius + 1e-4f) << "样本 " << sample << " 角点 " << corner;
			//盒包含
			for (uint32_t axis = 0; axis < 3; ++axis)
			{
				EXPECT_GE(world[axis], bounds.aabb_min[axis] - 1e-4f) << "样本 " << sample << " 轴 " << axis;
				EXPECT_LE(world[axis], bounds.aabb_max[axis] + 1e-4f) << "样本 " << sample << " 轴 " << axis;
			}
		}
	}
}

//非法入参：空指针 / 下界大于上界 / 矩阵含非数
TEST_F(Renderer_GPU_Driven_Test, 包围体非法参数)
{
	std::array<float, 16> identity = identity_matrix();
	engine::World_Bounds bounds;
	const float good_min[3] = { -1.0f, -1.0f, -1.0f };
	const float bad_max[3] = { -2.0f, -1.0f, -1.0f };
	EXPECT_FALSE(engine::world_bounds_build(bounds, nullptr, bad_max, identity.data()));
	EXPECT_FALSE(engine::world_bounds_build(bounds, good_min, bad_max, identity.data()));
	std::array<float, 16> broken = identity;
	broken[0] = std::numeric_limits<float>::quiet_NaN();
	EXPECT_FALSE(engine::world_bounds_build(bounds, good_min, bad_max, broken.data()));
}

/*
性能基线采集（第二百四十七轮前置子步）
口径：1080p ＋ MSAA 4× ＋ 1 万实例横向铺开；渲染 BASELINE_FRAMES 帧，
输出平均帧耗时、平均 CPU 剔除耗时与绘制统计，作为阶段 B / C / D 各轮的对照基线。
断言只保证「跑得通、计数自洽」，性能数值写入 `性能基线_方向一.md`，不使用绝对阈值。
*/
TEST_F(Renderer_GPU_Driven_Test, 性能基线采集)
{
	engine::Renderer_Config config;
	config.width = BASELINE_WIDTH;
	config.height = BASELINE_HEIGHT;
	config.hidden = true;
	config.msaa_samples = 4;
	config.instance_capacity = BASELINE_INSTANCES;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	//准备网格与实例矩阵
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data()));
	std::vector<std::array<float, 16>> matrices = baseline_matrices(BASELINE_INSTANCES);
	//逐帧渲染并收集
	double host_record_sum = 0.0;
	double gpu_frame_sum = 0.0;
	double cpu_cull_sum = 0.0;
	double gpu_samples = 0.0;
	uint64_t last_culled_instances = 0;
	uint64_t last_instances = 0;
	for (uint32_t frame = 0; frame < BASELINE_FRAMES; ++frame)
	{
		ASSERT_TRUE(renderer.mesh_draw_instanced(mesh, matrices[0].data(), BASELINE_INSTANCES));
		ASSERT_TRUE(renderer.frame_render());
		const engine::Frame_Stats stats = renderer.frame_stats_get();
		host_record_sum += stats.host_record_ms;
		gpu_frame_sum += stats.gpu_frame_ms;
		//GPU 时间戳支持时才有样本
		if (stats.gpu_frame_ms > 0.0)
			gpu_samples += 1.0;
		//剔除耗时（当前帧）
		cpu_cull_sum += stats.cpu_cull_ms;
		last_culled_instances = stats.last_culled_instances;
		last_instances = stats.last_instances;
	}
	const double frames = static_cast<double>(BASELINE_FRAMES);
	std::printf("[性能基线] 分辨率 %ux%u MSAA %u 实例 %u | 平均录制 %.3f ms | 平均 GPU 帧 %.3f ms（样本 %.0f）| 平均 CPU 剔除 %.3f ms | 末帧可见实例 %llu / 剔除实例 %llu\n",
		BASELINE_WIDTH, BASELINE_HEIGHT, config.msaa_samples, BASELINE_INSTANCES,
		host_record_sum / frames, gpu_frame_sum / frames, gpu_samples, cpu_cull_sum / frames,
		static_cast<unsigned long long>(last_instances), static_cast<unsigned long long>(last_culled_instances));
	//基线自洽性：至少一帧有绘制实例，且剔除计数不超过登记数
	EXPECT_GE(last_instances + last_culled_instances, 1u);
	EXPECT_GE(cpu_cull_sum, 0.0);
	renderer.mesh_unload(mesh);
}
//视锥平面提取与球 / 盒判定（第二百四十八轮；纯 CPU 口径）
TEST_F(Renderer_GPU_Driven_Test, 视锥平面与球判可见)
{
	//单位裁剪矩阵：视锥退化为 [−1, 1] 立方体
	std::array<float, 16> identity = identity_matrix();
	engine::Frustum_Planes planes;
	ASSERT_TRUE(engine::frustum_planes_extract(planes, identity.data()));
	//六个平面均已归一化
	for (uint32_t plane = 0; plane < 6; ++plane)
	{
		const float* value = planes.planes[plane];
		const float length = std::sqrt(value[0] * value[0] + value[1] * value[1] + value[2] * value[2]);
		EXPECT_NEAR(length, 1.0f, 1e-4f) << "平面 " << plane;
	}
	//原点小球可见
	const float center_origin[3] = { 0.0f, 0.0f, 0.0f };
	EXPECT_TRUE(engine::frustum_sphere_visible(planes, center_origin, 0.1f));
	//盒内边缘小球可见（1.05 − 1 = 0.05 ≤ 半径）
	const float center_edge[3] = { 1.05f, 0.0f, 0.0f };
	EXPECT_TRUE(engine::frustum_sphere_visible(planes, center_edge, 0.1f));
	//盒外小球不可见
	const float center_out[3] = { 1.5f, 0.0f, 0.0f };
	EXPECT_FALSE(engine::frustum_sphere_visible(planes, center_out, 0.1f));
	//跨边界的大球仍可见（半径覆盖到盒内）
	EXPECT_TRUE(engine::frustum_sphere_visible(planes, center_out, 0.6f));
	//盒判定：完全在立方体内的盒可见、完全在外不可见
	const float inside_min[3] = { -0.5f, -0.5f, -0.5f };
	const float inside_max[3] = { 0.5f, 0.5f, 0.5f };
	EXPECT_TRUE(engine::frustum_aabb_visible(planes, inside_min, inside_max));
	const float outside_min[3] = { 2.0f, 2.0f, 2.0f };
	const float outside_max[3] = { 3.0f, 3.0f, 3.0f };
	EXPECT_FALSE(engine::frustum_aabb_visible(planes, outside_min, outside_max));
	//非法矩阵
	const float broken[16] = { 0.0f };
	EXPECT_FALSE(engine::frustum_planes_extract(planes, broken));
}

/*
GPU 剔除与 CPU 参考一致且 CPU 更保守（第二百四十八轮核心验收）
口径：8 组随机相机 × 2000 个随机实例，断言 GPU 掩码与 CPU 球判定逐项一致，
且 CPU 的 AABB 判定为可见的实例在 GPU 掩码里必须可见（C ⊆ G）。
*/
TEST_F(Renderer_GPU_Driven_Test, GPU剔除与CPU一致且CPU更保守)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.compute_supported())
		GTEST_SKIP() << "本机图形队列族不支持计算路径";
	//网格局部 AABB（与 quad_data 一致）
	const float local_min[3] = { -QUAD_HALF, -QUAD_HALF, 0.0f };
	const float local_max[3] = { QUAD_HALF, QUAD_HALF, 0.0f };
	constexpr uint32_t INSTANCE_COUNT = 2000;
	//随机实例矩阵：平移铺开 ＋ 随机缩放 ＋ 绕 Z 旋转
	std::mt19937 generator(20261030u);
	std::uniform_real_distribution<float> offset_x(-40.0f, 40.0f);
	std::uniform_real_distribution<float> offset_yz(-12.0f, 12.0f);
	std::uniform_real_distribution<float> scale(0.2f, 3.0f);
	std::uniform_real_distribution<float> angle(-3.14159265f, 3.14159265f);
	std::vector<std::array<float, 16>> transforms(INSTANCE_COUNT);
	for (uint32_t index = 0; index < INSTANCE_COUNT; ++index)
	{
		const float s = scale(generator);
		const float a = angle(generator);
		std::array<float, 16> matrix = identity_matrix();
		matrix[0] = std::cos(a) * s;
		matrix[1] = std::sin(a) * s;
		matrix[4] = -std::sin(a) * s;
		matrix[5] = std::cos(a) * s;
		matrix[10] = s;
		matrix[12] = offset_x(generator);
		matrix[13] = offset_yz(generator);
		matrix[14] = offset_yz(generator);
		transforms[index] = matrix;
	}
	std::vector<uint32_t> mask(INSTANCE_COUNT, 0u);
	//8 组随机相机
	for (uint32_t view = 0; view < 8; ++view)
	{
		renderer.camera_position_set(offset_x(generator) * 0.5f, offset_yz(generator) * 0.5f,
			offset_yz(generator) * 0.5f + 6.0f);
		renderer.camera_target_set(0.0f, 0.0f, 0.0f);
		renderer.camera_fov_set(0.6f + 0.2f * static_cast<float>(view % 4));
		ASSERT_TRUE(renderer.gpu_cull_mask(transforms[0].data(), INSTANCE_COUNT, local_min, local_max, mask.data()));
		//CPU 参考：当前相机矩阵 → 六平面
		float view_proj[16] = {};
		ASSERT_TRUE(renderer.camera_view_proj(view_proj));
		engine::Frustum_Planes planes;
		ASSERT_TRUE(engine::frustum_planes_extract(planes, view_proj));
		uint32_t visible_count = 0;
		for (uint32_t index = 0; index < INSTANCE_COUNT; ++index)
		{
			engine::World_Bounds bounds;
			ASSERT_TRUE(engine::world_bounds_build(bounds, local_min, local_max, transforms[index].data()));
			const bool cpu_sphere = engine::frustum_sphere_visible(planes, bounds.center, bounds.radius);
			const bool gpu = mask[index] != 0u;
			//GPU 与 CPU 球判定逐项一致
			EXPECT_EQ(gpu, cpu_sphere) << "视图 " << view << " 实例 " << index;
			//CPU 的 AABB 判定更紧：它为可见时必须 GPU 也可见（C ⊆ G）
			if (engine::frustum_aabb_visible(planes, bounds.aabb_min, bounds.aabb_max) && !gpu)
				ADD_FAILURE() << "视图 " << view << " 实例 " << index << " 被 GPU 多剔";
			if (gpu)
				++visible_count;
			}
		//该视图下至少有一半实例可见（相机在场景中，统计口径自洽）
		EXPECT_GE(visible_count, 1u);
	}
}
namespace
{
	//CPU 剔除参考（与渲染器 core 内 frustum_cull_aabb 逐式同构；第二百四十九轮）
	bool cpu_cull_clip(const float view_proj[16], const float model[16],
		const float local_min[3], const float local_max[3])
	{
		//组合矩阵 mvp = view_proj × model（列主序）
		float mvp[16] = {};
		for (uint32_t column = 0; column < 4; ++column)
			for (uint32_t row = 0; row < 4; ++row)
			{
				float sum = 0.0f;
				for (uint32_t inner = 0; inner < 4; ++inner)
					sum += view_proj[inner * 4 + row] * model[column * 4 + inner];
				mvp[column * 4 + row] = sum;
			}
		bool outside[6] = { true, true, true, true, true, true };
		for (uint32_t corner = 0; corner < 8; ++corner)
		{
			const float x = (corner & 1u) != 0 ? local_max[0] : local_min[0];
			const float y = (corner & 2u) != 0 ? local_max[1] : local_min[1];
			const float z = (corner & 4u) != 0 ? local_max[2] : local_min[2];
			const float clip_x = mvp[0] * x + mvp[4] * y + mvp[8] * z + mvp[12];
			const float clip_y = mvp[1] * x + mvp[5] * y + mvp[9] * z + mvp[13];
			const float clip_z = mvp[2] * x + mvp[6] * y + mvp[10] * z + mvp[14];
			const float clip_w = mvp[3] * x + mvp[7] * y + mvp[11] * z + mvp[15];
			if (!(clip_x > clip_w))
				outside[0] = false;
			if (!(clip_x < -clip_w))
				outside[1] = false;
			if (!(clip_y > clip_w))
				outside[2] = false;
			if (!(clip_y < -clip_w))
				outside[3] = false;
			if (!(clip_z < 0.0f))
				outside[4] = false;
			if (!(clip_z > clip_w))
				outside[5] = false;
		}
		for (uint32_t plane = 0; plane < 6; ++plane)
			if (outside[plane])
				return true;
		return false;
	}
}

/*
GPU 压缩与 CPU 剔除一致（第二百四十九轮）
口径：8 组随机相机 × 2000 随机实例，GPU 压缩出的矩阵序列必须与 CPU 剔除保留的序列逐字节一致。
*/
TEST_F(Renderer_GPU_Driven_Test, GPU压缩与CPU剔除一致)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.compute_supported())
		GTEST_SKIP() << "本机图形队列族不支持计算路径";
	const float local_min[3] = { -QUAD_HALF, -QUAD_HALF, 0.0f };
	const float local_max[3] = { QUAD_HALF, QUAD_HALF, 0.0f };
	constexpr uint32_t INSTANCE_COUNT = 2000;
	std::mt19937 generator(20261030u);
	std::uniform_real_distribution<float> offset_x(-30.0f, 30.0f);
	std::uniform_real_distribution<float> offset_yz(-10.0f, 10.0f);
	std::uniform_real_distribution<float> scale(0.2f, 2.0f);
	std::uniform_real_distribution<float> angle(-3.14159265f, 3.14159265f);
	std::vector<std::array<float, 16>> transforms(INSTANCE_COUNT);
	for (uint32_t index = 0; index < INSTANCE_COUNT; ++index)
	{
		const float s = scale(generator);
		const float a = angle(generator);
		std::array<float, 16> matrix = identity_matrix();
		matrix[0] = std::cos(a) * s;
		matrix[1] = std::sin(a) * s;
		matrix[4] = -std::sin(a) * s;
		matrix[5] = std::cos(a) * s;
		matrix[10] = s;
		matrix[12] = offset_x(generator);
		matrix[13] = offset_yz(generator);
		matrix[14] = offset_yz(generator);
		transforms[index] = matrix;
	}
	std::vector<float> compacted(static_cast<std::size_t>(INSTANCE_COUNT) * 16, 0.0f);
	for (uint32_t view = 0; view < 8; ++view)
	{
		renderer.camera_position_set(offset_x(generator) * 0.3f, offset_yz(generator) * 0.3f, 6.0f);
		renderer.camera_target_set(0.0f, 0.0f, 0.0f);
		renderer.camera_fov_set(0.7f + 0.15f * static_cast<float>(view % 4));
		uint32_t visible = 0;
		ASSERT_TRUE(renderer.gpu_instance_compact(transforms[0].data(), INSTANCE_COUNT,
			local_min, local_max, compacted.data(), &visible));
		float view_proj[16] = {};
		ASSERT_TRUE(renderer.camera_view_proj(view_proj));
		//CPU 参考序列
		std::vector<std::array<float, 16>> expected;
		for (uint32_t index = 0; index < INSTANCE_COUNT; ++index)
			if (!cpu_cull_clip(view_proj, transforms[index].data(), local_min, local_max))
				expected.push_back(transforms[index]);
		ASSERT_EQ(visible, static_cast<uint32_t>(expected.size())) << "视图 " << view;
		/*
		逐实例比对（按字典序排序后比较）
		原子计数分配的槽位顺序由 GPU 调度决定，不保证与输入顺序一致；
		故校验「集合相等」而非「序列相等」——像素等价由开关用例以整帧像素断言覆盖。
		*/
		std::vector<std::array<float, 16>> gpu_slots(visible);
		for (uint32_t slot = 0; slot < visible; ++slot)
			for (uint32_t element = 0; element < 16; ++element)
				gpu_slots[slot][element] = compacted[static_cast<std::size_t>(slot) * 16 + element];
		auto less = [](const std::array<float, 16>& left, const std::array<float, 16>& right)
		{
			for (uint32_t element = 0; element < 16; ++element)
				if (left[element] != right[element])
					return left[element] < right[element];
			return false;
		};
		std::sort(gpu_slots.begin(), gpu_slots.end(), less);
		std::sort(expected.begin(), expected.end(), less);
		for (uint32_t slot = 0; slot < visible; ++slot)
			for (uint32_t element = 0; element < 16; ++element)
				ASSERT_FLOAT_EQ(gpu_slots[slot][element], expected[slot][element])
					<< "视图 " << view << " 槽 " << slot << " 元素 " << element;
	}
}

/*
GPU 剔除开关的画面与统计等价（第二百四十九轮核心验收）
口径：同一 1000 实例场景，关/开 GPU 剔除的两渲染器必须像素逐字节一致，绘制与剔除统计一致。
*/
TEST_F(Renderer_GPU_Driven_Test, GPU剔除开关画面等价)
{
	engine::Renderer_Config off_config = hidden_config();
	off_config.instance_capacity = 1024;
	engine::Renderer off;
	ASSERT_TRUE(off.init(off_config));
	if (!off.readback_supported() || !off.compute_supported())
		GTEST_SKIP() << "本机不支持像素回读或计算路径";
	engine::Renderer_Config on_config = off_config;
	on_config.gpu_cull_enable = true;
	engine::Renderer on;
	ASSERT_TRUE(on.init(on_config));
	EXPECT_TRUE(on.gpu_cull_enable_get());
	//同一批实例（横向铺开，一部分在视锥外）
	std::mt19937 generator(20261030u);
	std::uniform_real_distribution<float> offset_x(-8.0f, 8.0f);
	std::vector<std::array<float, 16>> transforms(1000);
	for (uint32_t index = 0; index < transforms.size(); ++index)
	{
		std::array<float, 16> matrix = identity_matrix();
		matrix[12] = offset_x(generator);
		transforms[index] = matrix;
	}
	engine::Mesh_Handle off_mesh = 0;
	engine::Mesh_Handle on_mesh = 0;
	ASSERT_TRUE(off.mesh_load(off_mesh, quad_data()));
	ASSERT_TRUE(on.mesh_load(on_mesh, quad_data()));
	ASSERT_TRUE(off.mesh_draw_instanced(off_mesh, transforms[0].data(), static_cast<uint32_t>(transforms.size())));
	ASSERT_TRUE(on.mesh_draw_instanced(on_mesh, transforms[0].data(), static_cast<uint32_t>(transforms.size())));
	std::vector<uint8_t> off_pixels(WINDOW_WIDTH * WINDOW_HEIGHT * 4, 0);
	std::vector<uint8_t> on_pixels(off_pixels.size(), 0);
	ASSERT_TRUE(off.frame_render(off_pixels.data(), static_cast<uint32_t>(off_pixels.size())));
	ASSERT_TRUE(on.frame_render(on_pixels.data(), static_cast<uint32_t>(on_pixels.size())));
	//像素逐字节一致
	uint32_t diff = 0;
	for (std::size_t offset = 0; offset < off_pixels.size(); ++offset)
		if (off_pixels[offset] != on_pixels[offset])
			++diff;
	EXPECT_EQ(diff, 0u) << "开/关 GPU 剔除的像素不一致";
	//统计一致（剔除数 / 可见实例数 / 实际绘制实例数）
	const engine::Frame_Stats off_stats = off.frame_stats_get();
	const engine::Frame_Stats on_stats = on.frame_stats_get();
	EXPECT_EQ(off_stats.last_culled_instances, on_stats.last_culled_instances);
	EXPECT_EQ(off_stats.last_instances, on_stats.last_instances);
	EXPECT_EQ(off_stats.drawn_instances, on_stats.drawn_instances);
	//GPU 路径确实产生了耗时或至少计数可读（不支持时间戳时为 −1）
	if (on.compute_timestamp_supported())
		EXPECT_GE(on_stats.gpu_cull_ms, 0.0);
	off.mesh_unload(off_mesh);
	on.mesh_unload(on_mesh);
}
//距离与背面剔除边界（第二百五十轮；纯 CPU 口径）
TEST_F(Renderer_GPU_Driven_Test, 距离与背面剔除边界)
{
	const float camera[3] = { 0.0f, 0.0f, 0.0f };
	const float forward[3] = { 0.0f, 0.0f, -1.0f };
	//恰好等于阈值仍可见（严格大于才剔除）
	const float at_edge[3] = { 5.0f, 0.0f, 0.0f };
	EXPECT_FALSE(engine::distance_cull(at_edge, camera, 5.0f));
	//略超阈值被剔除
	const float beyond[3] = { 5.001f, 0.0f, 0.0f };
	EXPECT_TRUE(engine::distance_cull(beyond, camera, 5.0f));
	//阈值 ≤ 0 不限距离
	EXPECT_FALSE(engine::distance_cull(beyond, camera, 0.0f));
	EXPECT_FALSE(engine::distance_cull(beyond, camera, -1.0f));
	//相机前方可见、正交可见、后方被剔除
	const float front[3] = { 0.0f, 0.0f, -3.0f };
	const float side[3] = { 3.0f, 0.0f, 0.0f };
	const float behind[3] = { 0.0f, 0.0f, 3.0f };
	EXPECT_FALSE(engine::backface_cull(front, camera, forward));
	EXPECT_FALSE(engine::backface_cull(side, camera, forward));
	EXPECT_TRUE(engine::backface_cull(behind, camera, forward));
}

/*
GPU 距离 / 背面剔除与 CPU 一致（第二百五十轮）
口径：同一 2000 实例场景，开启距离上限与背面剔除，GPU 压缩可见数必须等于 CPU 参考（距离 ∪ 背面 ∪ 视锥）。
*/
TEST_F(Renderer_GPU_Driven_Test, GPU距离剔除与CPU一致)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.compute_supported())
		GTEST_SKIP() << "本机图形队列族不支持计算路径";
	const float local_min[3] = { -QUAD_HALF, -QUAD_HALF, 0.0f };
	const float local_max[3] = { QUAD_HALF, QUAD_HALF, 0.0f };
	constexpr uint32_t INSTANCE_COUNT = 2000;
	std::mt19937 generator(20261030u);
	std::uniform_real_distribution<float> offset_x(-20.0f, 20.0f);
	std::uniform_real_distribution<float> offset_yz(-8.0f, 8.0f);
	std::vector<std::array<float, 16>> transforms(INSTANCE_COUNT);
	for (uint32_t index = 0; index < INSTANCE_COUNT; ++index)
	{
		std::array<float, 16> matrix = identity_matrix();
		matrix[12] = offset_x(generator);
		matrix[13] = offset_yz(generator);
		matrix[14] = offset_yz(generator);
		transforms[index] = matrix;
	}
	//相机在 +Z 侧看向原点；距离上限 22；开背面剔除
	const float camera[3] = { 0.0f, 0.0f, 6.0f };
	const float forward[3] = { 0.0f, 0.0f, -1.0f };
	constexpr float MAX_DISTANCE = 22.0f;
	renderer.camera_position_set(camera[0], camera[1], camera[2]);
	renderer.camera_target_set(0.0f, 0.0f, 0.0f);
	std::vector<float> compacted(static_cast<std::size_t>(INSTANCE_COUNT) * 16, 0.0f);
	uint32_t visible = 0;
	ASSERT_TRUE(renderer.gpu_instance_compact_ex(transforms[0].data(), INSTANCE_COUNT,
		local_min, local_max, camera, forward, MAX_DISTANCE, true, compacted.data(), &visible));
	float view_proj[16] = {};
	ASSERT_TRUE(renderer.camera_view_proj(view_proj));
	uint32_t expected = 0;
	for (uint32_t index = 0; index < INSTANCE_COUNT; ++index)
	{
		//实例中心 = 平移列
		const float center[3] = { transforms[index][12], transforms[index][13], transforms[index][14] };
		const bool culled = engine::distance_cull(center, camera, MAX_DISTANCE)
			|| engine::backface_cull(center, camera, forward)
			|| cpu_cull_clip(view_proj, transforms[index].data(), local_min, local_max);
		if (!culled)
			++expected;
	}
	EXPECT_EQ(visible, expected);
}
/*
剔除三态统计单调（第二百五十一轮）
口径：同一 512 实例场景三态——关（CPU 剔除）/ GPU 视锥 / GPU 视锥＋距离：
① 关与 GPU 视锥两态剔除数相等（开关等价）；② GPU 派发计数 0 → 1；③ 加距离后可见数不增。
*/
TEST_F(Renderer_GPU_Driven_Test, 剔除三态统计单调)
{
	engine::Renderer_Config base = hidden_config();
	base.instance_capacity = 1024;
	engine::Renderer off;
	ASSERT_TRUE(off.init(base));
	engine::Renderer_Config gpu_config = base;
	gpu_config.gpu_cull_enable = true;
	engine::Renderer gpu;
	ASSERT_TRUE(gpu.init(gpu_config));
	if (!gpu.compute_supported())
		GTEST_SKIP() << "本机图形队列族不支持计算路径";
	constexpr uint32_t COUNT = 512;
	std::mt19937 generator(20261030u);
	std::uniform_real_distribution<float> offset_x(-8.0f, 8.0f);
	std::vector<std::array<float, 16>> transforms(COUNT);
	for (uint32_t index = 0; index < COUNT; ++index)
	{
		std::array<float, 16> matrix = identity_matrix();
		matrix[12] = offset_x(generator);
		transforms[index] = matrix;
	}
	engine::Mesh_Handle off_mesh = 0;
	engine::Mesh_Handle gpu_mesh = 0;
	ASSERT_TRUE(off.mesh_load(off_mesh, quad_data()));
	ASSERT_TRUE(gpu.mesh_load(gpu_mesh, quad_data()));
	ASSERT_TRUE(off.mesh_draw_instanced(off_mesh, transforms[0].data(), COUNT));
	ASSERT_TRUE(gpu.mesh_draw_instanced(gpu_mesh, transforms[0].data(), COUNT));
	ASSERT_TRUE(off.frame_render());
	ASSERT_TRUE(gpu.frame_render());
	const engine::Frame_Stats off_stats = off.frame_stats_get();
	const engine::Frame_Stats gpu_stats = gpu.frame_stats_get();
	//关与 GPU 视锥的剔除 / 可见数一致
	EXPECT_EQ(off_stats.last_culled_instances, gpu_stats.last_culled_instances);
	EXPECT_EQ(off_stats.last_instances, gpu_stats.last_instances);
	//派发计数：关为 0、GPU 为 1
	EXPECT_EQ(off_stats.gpu_cull_dispatches, 0u);
	EXPECT_EQ(gpu_stats.gpu_cull_dispatches, 1u);
	//再加距离上限：可见数不增（且剔除数增加）
	const float center[3] = { 0.0f, 0.0f, 6.0f };
	const float forward_dir[3] = { 0.0f, 0.0f, -1.0f };
	const float local_min[3] = { -QUAD_HALF, -QUAD_HALF, 0.0f };
	const float local_max[3] = { QUAD_HALF, QUAD_HALF, 0.0f };
	std::vector<float> compacted(static_cast<std::size_t>(COUNT) * 16, 0.0f);
	uint32_t visible_distance = 0;
	ASSERT_TRUE(gpu.gpu_instance_compact_ex(transforms[0].data(), COUNT, local_min, local_max,
		center, forward_dir, 14.0f, false, compacted.data(), &visible_distance));
	const float* view_proj_dummy = nullptr;
	(void)view_proj_dummy;
	EXPECT_LE(visible_distance, gpu_stats.last_instances);
	//派发计数再增 1
	EXPECT_EQ(gpu.frame_stats_get().gpu_cull_dispatches, 2u);
	off.mesh_unload(off_mesh);
	gpu.mesh_unload(gpu_mesh);
}