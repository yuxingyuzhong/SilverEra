/*
渲染粒子门面测试（第三百一十轮；深研方向二 阶段 A）
覆盖：未初始化拒绝、默认关闭、开关与容量钳制、发射器 / 参数校验、发射与步进、关闭再开启沿用池内容。
*/
#include <gtest/gtest.h>

#include <cstdint>

#include "Application/Renderer/src/render/Renderer/渲染器.h"

namespace
{
	//隐藏窗口配置
	engine::Renderer_Config hidden_config(void)
	{
		engine::Renderer_Config config;
		config.width = 64;
		config.height = 36;
		config.hidden = true;
		return config;
	}

	//点发射器（数量 32、寿命 0.5..1.0）
	engine::Particle_Emitter point_emitter(void)
	{
		engine::Particle_Emitter emitter{};
		emitter.position[0] = 0.0f;
		emitter.position[1] = 1.0f;
		emitter.position[2] = 0.0f;
		emitter.direction[1] = 1.0f;
		emitter.speed_min = 1.0f;
		emitter.speed_max = 2.0f;
		emitter.lifetime_min = 0.5f;
		emitter.lifetime_max = 1.0f;
		emitter.shape = static_cast<uint32_t>(engine::Emit_Shape::Point);
		emitter.count = 32;
		return emitter;
	}
}

//未初始化：路径不可用、开关与容量设置被拒
TEST(Renderer_Particle_Test, 未初始化拒绝)
{
	engine::Renderer renderer;
	EXPECT_FALSE(renderer.particle_supported());
	EXPECT_FALSE(renderer.particle_enable_set(true));
	EXPECT_FALSE(renderer.particle_capacity_set(16));
	EXPECT_EQ(renderer.particle_capacity_get(), 0u);
	EXPECT_EQ(renderer.particle_active_count(), 0u);
}

//默认关闭：初始化后开关关闭、容量为默认值、活跃为 0
TEST(Renderer_Particle_Test, 默认关闭)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	EXPECT_TRUE(renderer.particle_supported());
	EXPECT_FALSE(renderer.particle_enable_get());
	EXPECT_EQ(renderer.particle_capacity_get(), engine::PARTICLE_CAPACITY_DEFAULT);
	EXPECT_EQ(renderer.particle_active_count(), 0u);
	//关闭时发射 / 步进返回 0
	EXPECT_EQ(renderer.particle_emit(8), 0u);
	EXPECT_EQ(renderer.particle_step(1.0f / 60.0f), 0u);
}

//容量钳制：0 → 1；超上限 → 上限
TEST(Renderer_Particle_Test, 容量钳制)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	ASSERT_TRUE(renderer.particle_enable_set(true));
	ASSERT_TRUE(renderer.particle_capacity_set(0));
	EXPECT_EQ(renderer.particle_capacity_get(), 1u);
	ASSERT_TRUE(renderer.particle_capacity_set(engine::PARTICLE_CAPACITY_MAX + 1u));
	EXPECT_EQ(renderer.particle_capacity_get(), engine::PARTICLE_CAPACITY_MAX);
}

//发射器 / 参数：关闭时拒绝；开启后非法配置拒绝、合法接受
TEST(Renderer_Particle_Test, 配置校验)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Particle_Emitter emitter = point_emitter();
	engine::Particle_Params params{};
	params.damping = 1.0f;
	params.restitution = 0.4f;
	params.friction = 0.2f;
	//关闭态拒绝
	EXPECT_FALSE(renderer.particle_emitter_set(emitter));
	EXPECT_FALSE(renderer.particle_params_set(params));
	ASSERT_TRUE(renderer.particle_enable_set(true));
	//非法发射器（数量 0）
	engine::Particle_Emitter bad_emitter = emitter;
	bad_emitter.count = 0;
	EXPECT_FALSE(renderer.particle_emitter_set(bad_emitter));
	//非法参数（阻尼 > 1）
	engine::Particle_Params bad_params = params;
	bad_params.damping = 2.0f;
	EXPECT_FALSE(renderer.particle_params_set(bad_params));
	//合法
	EXPECT_TRUE(renderer.particle_emitter_set(emitter));
	EXPECT_TRUE(renderer.particle_params_set(params));
}

//发射与步进：数量、寿命回收、关闭再开启沿用池内容
TEST(Renderer_Particle_Test, 发射与步进)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	ASSERT_TRUE(renderer.particle_enable_set(true));
	ASSERT_TRUE(renderer.particle_capacity_set(64));
	ASSERT_TRUE(renderer.particle_emitter_set(point_emitter()));
	engine::Particle_Params params{};
	params.damping = 1.0f;
	params.restitution = 0.4f;
	params.friction = 0.2f;
	ASSERT_TRUE(renderer.particle_params_set(params));
	EXPECT_EQ(renderer.particle_emit(64), 32u);
	EXPECT_EQ(renderer.particle_active_count(), 32u);
	//单步：寿命未到，无死亡
	EXPECT_EQ(renderer.particle_step(1.0f / 60.0f), 0u);
	EXPECT_EQ(renderer.particle_active_count(), 32u);
	//一秒后全部回收（寿命 ≤ 1.0 s）
	uint32_t died = 0;
	for (uint32_t step = 0; step < 61; ++step)
		died += renderer.particle_step(1.0f / 60.0f);
	EXPECT_EQ(died, 32u);
	EXPECT_EQ(renderer.particle_active_count(), 0u);
	//关闭不释放池内容：关态查询 / 发射为 0，重新开启恢复
	EXPECT_EQ(renderer.particle_emit(8), 8u);
	ASSERT_TRUE(renderer.particle_enable_set(false));
	EXPECT_EQ(renderer.particle_emit(8), 0u);
	EXPECT_EQ(renderer.particle_active_count(), 0u);
	ASSERT_TRUE(renderer.particle_enable_set(true));
	EXPECT_EQ(renderer.particle_active_count(), 8u);
}
