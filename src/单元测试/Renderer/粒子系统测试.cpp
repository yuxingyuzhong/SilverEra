/*
粒子池测试（第三百零八轮；深研方向二 阶段 A）
覆盖：默认容量、满载拒绝、回收复用（LIFO）、重复 / 越界回收、清空复位、容量钳制。
*/
#include <gtest/gtest.h>

#include <cstdint>

#include "Application/Renderer/src/render/Particle/粒子系统.h"

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
