/*
粒子池测试（第三百零八轮；深研方向二 阶段 A）
覆盖：默认容量、满载拒绝、回收复用（LIFO）、重复 / 越界回收、清空复位、容量钳制。
*/
#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>

#include "Engine/EngineCore/src/core/effect/粒子系统.h"

//默认容量与初始状态
TEST(Particle_System_Test, 默认状态)
{
	engine::Particle_System system;
	EXPECT_EQ(system.capacity_get(), engine::PARTICLE_CAPACITY_DEFAULT);
	EXPECT_EQ(system.active_count_get(), 0u);
}

//满载拒绝：分配次数等于容量后返回假
TEST(Particle_System_Test, 满载拒绝)
{
	engine::Particle_System system;
	system.capacity_set(4);
	uint32_t index = 0;
	for (uint32_t count = 0; count < 4; ++count)
		ASSERT_TRUE(system.spawn(index));
	EXPECT_EQ(system.active_count_get(), 4u);
	uint32_t extra = 0;
	EXPECT_FALSE(system.spawn(extra));
	EXPECT_EQ(system.active_count_get(), 4u);
}

//回收复用：LIFO 顺序；重复回收与越界回收被拒
TEST(Particle_System_Test, 回收复用)
{
	engine::Particle_System system;
	system.capacity_set(4);
	uint32_t first = 0;
	uint32_t second = 0;
	ASSERT_TRUE(system.spawn(first));
	ASSERT_TRUE(system.spawn(second));
	//回收后立即复用同一索引
	ASSERT_TRUE(system.recycle(second));
	uint32_t reused = 0;
	ASSERT_TRUE(system.spawn(reused));
	EXPECT_EQ(reused, second);
	//再次回收后变为空闲；重复回收被拒
	ASSERT_TRUE(system.recycle(second));
	EXPECT_FALSE(system.recycle(second));
	//越界回收
	EXPECT_FALSE(system.recycle(99u));
	//活跃计数：仅首粒子仍活跃
	EXPECT_EQ(system.active_count_get(), 1u);
}

//清空复位：活跃归零且可再次分配满容量
TEST(Particle_System_Test, 清空复位)
{
	engine::Particle_System system;
	system.capacity_set(3);
	uint32_t index = 0;
	for (uint32_t count = 0; count < 3; ++count)
		ASSERT_TRUE(system.spawn(index));
	system.clear();
	EXPECT_EQ(system.active_count_get(), 0u);
	for (uint32_t count = 0; count < 3; ++count)
		ASSERT_TRUE(system.spawn(index));
	EXPECT_EQ(system.active_count_get(), 3u);
}

//容量钳制：0 → 1；超上限 → 上限
TEST(Particle_System_Test, 容量钳制)
{
	engine::Particle_System system;
	system.capacity_set(0);
	EXPECT_EQ(system.capacity_get(), 1u);
	system.capacity_set(engine::PARTICLE_CAPACITY_MAX + 1u);
	EXPECT_EQ(system.capacity_get(), engine::PARTICLE_CAPACITY_MAX);
}

/*
参考模拟：离散落体与解析公式对照（第三百零九轮）
v += g·dt → v *= damping(1) → p += v·dt，N 步后 y = -g·dt²·N(N+1)/2。
*/
TEST(Particle_System_Test, 参考模拟_落体)
{
	engine::Particle_System system;
	system.capacity_set(1);
	uint32_t index = 0;
	ASSERT_TRUE(system.spawn(index));
	engine::Particle_State& state = system.data_get()[index];
	state.position[0] = 0.0f;
	state.position[1] = 0.0f;
	state.position[2] = 0.0f;
	state.velocity[0] = 0.0f;
	state.velocity[1] = 0.0f;
	state.velocity[2] = 0.0f;
	state.age = 0.0f;
	state.lifetime = 100.0f;
	engine::Particle_Params params{};
	params.gravity[1] = -9.8f;
	params.damping = 1.0f;
	params.restitution = 0.5f;
	params.friction = 0.2f;
	const float dt = 1.0f / 60.0f;
	constexpr uint32_t STEPS = 60;
	uint32_t died = 0;
	for (uint32_t step = 0; step < STEPS; ++step)
		died += system.step(params, dt);
	EXPECT_EQ(died, 0u);
	//离散落体位移
	const float expected = -9.8f * dt * dt * static_cast<float>(STEPS * (STEPS + 1) / 2);
	EXPECT_NEAR(state.position[1], expected, 1e-4f);
	EXPECT_NEAR(state.age, 1.0f, 1e-5f);
}

//寿命回收：年龄达到寿命即回收，活跃数归零
TEST(Particle_System_Test, 寿命回收)
{
	engine::Particle_System system;
	system.capacity_set(2);
	uint32_t index = 0;
	ASSERT_TRUE(system.spawn(index));
	system.data_get()[index].lifetime = 0.1f;
	engine::Particle_Params params{};
	params.damping = 1.0f;
	const float dt = 1.0f / 60.0f;
	uint32_t died = 0;
	//7 步 = 0.1167 s > 0.1 s
	for (uint32_t step = 0; step < 7; ++step)
		died += system.step(params, dt);
	EXPECT_EQ(died, 1u);
	EXPECT_EQ(system.active_count_get(), 0u);
}

//点发射：数量 / 位置 / 速度与寿命区间
TEST(Particle_System_Test, 发射_点)
{
	engine::Particle_System system;
	system.capacity_set(64);
	engine::Particle_Emitter emitter{};
	emitter.position[0] = 1.0f;
	emitter.position[1] = 2.0f;
	emitter.position[2] = 3.0f;
	emitter.direction[1] = 1.0f;
	emitter.speed_min = 1.0f;
	emitter.speed_max = 2.0f;
	emitter.lifetime_min = 0.5f;
	emitter.lifetime_max = 1.5f;
	emitter.shape = static_cast<uint32_t>(engine::Emit_Shape::Point);
	emitter.count = 32;
	const uint32_t spawned = system.emit(emitter, 64);
	EXPECT_EQ(spawned, 32u);
	const engine::Particle_State* states = system.data_get();
	const uint8_t* alive = system.alive_get();
	for (uint32_t index = 0; index < system.capacity_get(); ++index)
	{
		//仅活跃索引检查
		if (alive[index] == 0u)
			continue;
		EXPECT_FLOAT_EQ(states[index].position[0], 1.0f);
		EXPECT_FLOAT_EQ(states[index].position[1], 2.0f);
		EXPECT_FLOAT_EQ(states[index].position[2], 3.0f);
		const float speed = std::sqrt(states[index].velocity[0] * states[index].velocity[0]
			+ states[index].velocity[1] * states[index].velocity[1]
			+ states[index].velocity[2] * states[index].velocity[2]);
		EXPECT_GE(speed, emitter.speed_min - 1e-4f);
		EXPECT_LE(speed, emitter.speed_max + 1e-4f);
		EXPECT_GE(states[index].lifetime, emitter.lifetime_min);
		EXPECT_LE(states[index].lifetime, emitter.lifetime_max);
		EXPECT_EQ(states[index].age, 0.0f);
	}
}

//盒发射：位置落在半尺寸内
TEST(Particle_System_Test, 发射_盒)
{
	engine::Particle_System system;
	system.capacity_set(64);
	engine::Particle_Emitter emitter{};
	emitter.position[0] = 0.0f;
	emitter.position[1] = 5.0f;
	emitter.position[2] = 0.0f;
	emitter.extents[0] = 1.0f;
	emitter.extents[1] = 0.5f;
	emitter.extents[2] = 2.0f;
	emitter.direction[1] = -1.0f;
	emitter.speed_min = 0.5f;
	emitter.speed_max = 1.0f;
	emitter.lifetime_min = 0.2f;
	emitter.lifetime_max = 2.0f;
	emitter.shape = static_cast<uint32_t>(engine::Emit_Shape::Box);
	emitter.count = 48;
	ASSERT_EQ(system.emit(emitter, 48), 48u);
	const engine::Particle_State* states = system.data_get();
	const uint8_t* alive = system.alive_get();
	//盒内逐分量检查（只查活跃索引）
	for (uint32_t index = 0; index < system.capacity_get(); ++index)
	{
		if (alive[index] == 0u)
			continue;
		EXPECT_LE(std::fabs(states[index].position[0] - emitter.position[0]), emitter.extents[0] + 1e-4f);
		EXPECT_LE(std::fabs(states[index].position[1] - emitter.position[1]), emitter.extents[1] + 1e-4f);
		EXPECT_LE(std::fabs(states[index].position[2] - emitter.position[2]), emitter.extents[2] + 1e-4f);
	}
}

//确定性：同种子两次发射逐字段一致
TEST(Particle_System_Test, 发射_确定性)
{
	engine::Particle_Emitter emitter{};
	emitter.position[0] = 0.0f;
	emitter.position[1] = 1.0f;
	emitter.position[2] = 0.0f;
	emitter.direction[1] = 1.0f;
	emitter.speed_min = 1.0f;
	emitter.speed_max = 3.0f;
	emitter.lifetime_min = 0.5f;
	emitter.lifetime_max = 1.0f;
	emitter.shape = static_cast<uint32_t>(engine::Emit_Shape::Sphere_Shell);
	emitter.extents[0] = 0.4f;
	emitter.count = 16;
	engine::Particle_System first;
	engine::Particle_System second;
	first.capacity_set(16);
	second.capacity_set(16);
	first.seed_set(7u);
	second.seed_set(7u);
	ASSERT_EQ(first.emit(emitter, 16), 16u);
	ASSERT_EQ(second.emit(emitter, 16), 16u);
	const engine::Particle_State* a = first.data_get();
	const engine::Particle_State* b = second.data_get();
	for (uint32_t index = 0; index < 16; ++index)
	{
		EXPECT_EQ(a[index].position[0], b[index].position[0]);
		EXPECT_EQ(a[index].position[1], b[index].position[1]);
		EXPECT_EQ(a[index].position[2], b[index].position[2]);
		EXPECT_EQ(a[index].velocity[0], b[index].velocity[0]);
		EXPECT_EQ(a[index].velocity[1], b[index].velocity[1]);
		EXPECT_EQ(a[index].velocity[2], b[index].velocity[2]);
		EXPECT_EQ(a[index].lifetime, b[index].lifetime);
	}
}
