/*
粒子数据测试（第三百零七轮；深研方向二 阶段 A）
覆盖：粒子状态尺寸 / 偏移（GPU 布局）、发射器与模拟参数校验边界、容量常量；
不依赖 Vulkan 与渲染器实例。
*/
#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "Application/Renderer/src/render/Particle/粒子数据.h"

namespace
{
	//合法发射器基线（点发射、速度 1..2、寿命 0.5..1.5、数量 64）
	engine::Particle_Emitter base_emitter(void)
	{
		engine::Particle_Emitter emitter{};
		emitter.position[0] = 1.0f;
		emitter.position[1] = 2.0f;
		emitter.position[2] = 3.0f;
		emitter.direction[0] = 0.0f;
		emitter.direction[1] = 1.0f;
		emitter.direction[2] = 0.0f;
		emitter.speed_min = 1.0f;
		emitter.speed_max = 2.0f;
		emitter.lifetime_min = 0.5f;
		emitter.lifetime_max = 1.5f;
		emitter.shape = static_cast<uint32_t>(engine::Emit_Shape::Point);
		emitter.count = 64;
		return emitter;
	}

	//合法模拟参数基线（重力 -9.8、阻尼 0.98、地面 0、反弹 0.4、摩擦 0.2）
	engine::Particle_Params base_params(void)
	{
		engine::Particle_Params params{};
		params.gravity[0] = 0.0f;
		params.gravity[1] = -9.8f;
		params.gravity[2] = 0.0f;
		params.damping = 0.98f;
		params.floor_y = 0.0f;
		params.restitution = 0.4f;
		params.friction = 0.2f;
		return params;
	}
}

//粒子状态布局：32 字节，偏移与 GPU std430 一致
TEST(Particle_Data_Test, 粒子状态布局)
{
	EXPECT_EQ(sizeof(engine::Particle_State), engine::PARTICLE_STRIDE);
	EXPECT_EQ(offsetof(engine::Particle_State, position), 0u);
	EXPECT_EQ(offsetof(engine::Particle_State, velocity), 12u);
	EXPECT_EQ(offsetof(engine::Particle_State, age), 24u);
	EXPECT_EQ(offsetof(engine::Particle_State, lifetime), 28u);
}

//容量常量：默认 < 上限，上限与规划一致
TEST(Particle_Data_Test, 容量常量)
{
	EXPECT_LT(engine::PARTICLE_CAPACITY_DEFAULT, engine::PARTICLE_CAPACITY_MAX);
	EXPECT_EQ(engine::PARTICLE_CAPACITY_MAX, 1048576u);
	EXPECT_LE(engine::PARTICLE_CAPACITY_DEFAULT, engine::PARTICLE_CAPACITY_MAX);
}

//发射器：合法基线通过
TEST(Particle_Data_Test, 发射器合法基线)
{
	EXPECT_TRUE(engine::particle_emitter_valid(base_emitter()));
}

//发射器：非法形状 / 数量 / 速度 / 寿命 / 半尺寸 / 方向
TEST(Particle_Data_Test, 发射器非法输入)
{
	engine::Particle_Emitter emitter = base_emitter();
	//形状越界
	emitter.shape = 3u;
	EXPECT_FALSE(engine::particle_emitter_valid(emitter));
	emitter = base_emitter();
	//数量为 0
	emitter.count = 0u;
	EXPECT_FALSE(engine::particle_emitter_valid(emitter));
	emitter = base_emitter();
	//数量超上限
	emitter.count = engine::PARTICLE_EMIT_COUNT_MAX + 1u;
	EXPECT_FALSE(engine::particle_emitter_valid(emitter));
	emitter = base_emitter();
	//速度上界小于下界
	emitter.speed_max = 0.5f;
	EXPECT_FALSE(engine::particle_emitter_valid(emitter));
	emitter = base_emitter();
	//寿命为 0
	emitter.lifetime_min = 0.0f;
	EXPECT_FALSE(engine::particle_emitter_valid(emitter));
	emitter = base_emitter();
	//半尺寸为负
	emitter.extents[0] = -1.0f;
	EXPECT_FALSE(engine::particle_emitter_valid(emitter));
	emitter = base_emitter();
	//方向为零向量
	emitter.direction[0] = 0.0f;
	emitter.direction[1] = 0.0f;
	emitter.direction[2] = 0.0f;
	EXPECT_FALSE(engine::particle_emitter_valid(emitter));
	emitter = base_emitter();
	//非有限值
	emitter.speed_min = std::numeric_limits<float>::quiet_NaN();
	EXPECT_FALSE(engine::particle_emitter_valid(emitter));
}

//模拟参数：合法基线通过；阻尼 / 反弹 / 摩擦越界拒绝
TEST(Particle_Data_Test, 模拟参数校验)
{
	EXPECT_TRUE(engine::particle_params_valid(base_params()));
	engine::Particle_Params params = base_params();
	params.damping = 1.5f;
	EXPECT_FALSE(engine::particle_params_valid(params));
	params = base_params();
	params.restitution = -0.1f;
	EXPECT_FALSE(engine::particle_params_valid(params));
	params = base_params();
	params.friction = 2.0f;
	EXPECT_FALSE(engine::particle_params_valid(params));
	params = base_params();
	params.gravity[1] = std::numeric_limits<float>::quiet_NaN();
	EXPECT_FALSE(engine::particle_params_valid(params));
	/*
	颜色覆盖（第三百五十四轮）
	启用时两端颜色各通道须落在 [0, 1]：越界 / 负值 / 非有限拒绝，合法接受；
	未启用时颜色字段不参与校验（旧调用方的零初始化参数保持可用）。
	*/
	params = base_params();
	params.color_enabled = 1;
	params.color_start[2] = 2.0f;
	EXPECT_FALSE(engine::particle_params_valid(params));
	params = base_params();
	params.color_enabled = 1;
	params.color_end[0] = -0.1f;
	EXPECT_FALSE(engine::particle_params_valid(params));
	params = base_params();
	params.color_enabled = 1;
	params.color_start[1] = std::numeric_limits<float>::quiet_NaN();
	EXPECT_FALSE(engine::particle_params_valid(params));
	params = base_params();
	params.color_start[2] = 2.0f;
	EXPECT_TRUE(engine::particle_params_valid(params));
	params = base_params();
	params.color_enabled = 1;
	params.color_start[2] = 1.0f;
	params.color_end[1] = 1.0f;
	EXPECT_TRUE(engine::particle_params_valid(params));
	/*
	软边宽度（第三百五十五轮）
	落在 [0, 1]：越界 / 非有限拒绝，区间内接受（0 = 内置 0.65 内半径）。
	*/
	params = base_params();
	params.edge_softness = 1.5f;
	EXPECT_FALSE(engine::particle_params_valid(params));
	params = base_params();
	params.edge_softness = std::numeric_limits<float>::quiet_NaN();
	EXPECT_FALSE(engine::particle_params_valid(params));
	params = base_params();
	params.edge_softness = 0.5f;
	EXPECT_TRUE(engine::particle_params_valid(params));
	/*
	软粒子深度档位（第三百六十六轮）
	落在 [0, 1]：越界 / 非有限拒绝，区间内接受（0 = 关闭）。
	*/
	params = base_params();
	params.soft_depth_range = 1.5f;
	EXPECT_FALSE(engine::particle_params_valid(params));
	params = base_params();
	params.soft_depth_range = std::numeric_limits<float>::quiet_NaN();
	EXPECT_FALSE(engine::particle_params_valid(params));
	params = base_params();
	params.soft_depth_range = 0.005f;
	EXPECT_TRUE(engine::particle_params_valid(params));
}
