/*
渲染粒子门面测试（第三百一十轮；深研方向二 阶段 A）
覆盖：未初始化拒绝、默认关闭、开关与容量钳制、发射器 / 参数校验、发射与步进、关闭再开启沿用池内容。
第三百一十一轮补：GPU 推进资源可用性与零步自检。
第三百一十二轮补：GPU 单步推进与 CPU 参考逐粒子对照。
第三百一十三轮补：GPU 生命周期与死亡同步。
第三百一十四轮补：粒子 billboard 绘制与画面定位。
*/
#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <vector>

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

//GPU 推进资源（第三百一十一轮）：关闭态不可用；开启后可用且零步自检为恒等
TEST(Renderer_Particle_Test, GPU推进自检)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//关闭态路径不可用
	EXPECT_FALSE(renderer.particle_gpu_available());
	EXPECT_FALSE(renderer.particle_gpu_probe());
	ASSERT_TRUE(renderer.particle_enable_set(true));
	ASSERT_TRUE(renderer.particle_capacity_set(64));
	ASSERT_TRUE(renderer.particle_emitter_set(point_emitter()));
	engine::Particle_Params params{};
	params.damping = 1.0f;
	params.restitution = 0.4f;
	params.friction = 0.2f;
	ASSERT_TRUE(renderer.particle_params_set(params));
	//本机计算路径可用：惰性创建后即可用
	EXPECT_TRUE(renderer.particle_gpu_available());
	//空池零步自检
	EXPECT_TRUE(renderer.particle_gpu_probe());
	//发射后零步自检逐粒子恒等（dt = 0、阻尼 1）
	EXPECT_EQ(renderer.particle_emit(32), 32u);
	EXPECT_EQ(renderer.particle_active_count(), 32u);
	EXPECT_TRUE(renderer.particle_gpu_probe());
	//容量变化后按新容量重建
	ASSERT_TRUE(renderer.particle_capacity_set(128));
	EXPECT_EQ(renderer.particle_capacity_get(), 128u);
	EXPECT_TRUE(renderer.particle_gpu_available());
	EXPECT_TRUE(renderer.particle_gpu_probe());
}

/*
GPU 单步对照（第三百一十二轮）
同种子 / 同配置的两套渲染器各发射 32 个粒子，CPU 参考与 GPU 推进连跑 10 步；
逐粒子比对位置 / 速度 / 年龄 / 寿命（阈值 1e-5）。
*/
TEST(Renderer_Particle_Test, GPU推进单步对照)
{
	engine::Particle_Emitter emitter = point_emitter();
	emitter.lifetime_min = 2.0f;
	emitter.lifetime_max = 3.0f;
	engine::Particle_Params params{};
	params.gravity[1] = -2.0f;
	params.damping = 0.9f;
	params.restitution = 0.4f;
	params.friction = 0.2f;
	engine::Renderer cpu;
	engine::Renderer gpu;
	ASSERT_TRUE(cpu.init(hidden_config()));
	ASSERT_TRUE(gpu.init(hidden_config()));
	ASSERT_TRUE(cpu.particle_enable_set(true));
	ASSERT_TRUE(gpu.particle_enable_set(true));
	ASSERT_TRUE(cpu.particle_capacity_set(64));
	ASSERT_TRUE(gpu.particle_capacity_set(64));
	ASSERT_TRUE(cpu.particle_emitter_set(emitter));
	ASSERT_TRUE(gpu.particle_emitter_set(emitter));
	ASSERT_TRUE(cpu.particle_params_set(params));
	ASSERT_TRUE(gpu.particle_params_set(params));
	ASSERT_EQ(cpu.particle_emit(32), 32u);
	ASSERT_EQ(gpu.particle_emit(32), 32u);
	ASSERT_TRUE(gpu.particle_gpu_available());
	//步长非法拒绝
	EXPECT_FALSE(gpu.particle_gpu_step(0.0f));
	EXPECT_FALSE(gpu.particle_gpu_step(-1.0f));
	EXPECT_FALSE(gpu.particle_gpu_step(std::numeric_limits<float>::quiet_NaN()));
	//十步推进（同参数同顺序）
	for (uint32_t step = 0; step < 10; ++step)
	{
		cpu.particle_step(0.05f);
		ASSERT_TRUE(gpu.particle_gpu_step(0.05f));
	}
	//逐活跃索引比对
	uint32_t compared = 0;
	for (uint32_t index = 0; index < 64; ++index)
	{
		engine::Particle_State expected{};
		engine::Particle_State actual{};
		//CPU 侧非活跃索引跳过
		if (!cpu.particle_state_get(index, expected))
			continue;
		ASSERT_TRUE(gpu.particle_state_get(index, actual));
		for (uint32_t axis = 0; axis < 3; ++axis)
		{
			EXPECT_NEAR(expected.position[axis], actual.position[axis], 1e-5f);
			EXPECT_NEAR(expected.velocity[axis], actual.velocity[axis], 1e-5f);
		}
		EXPECT_NEAR(expected.age, actual.age, 1e-5f);
		EXPECT_NEAR(expected.lifetime, actual.lifetime, 1e-5f);
		++compared;
	}
	EXPECT_EQ(compared, 32u);
	EXPECT_EQ(gpu.particle_active_count(), 32u);
}

/*
GPU 生命周期与死亡同步（第三百一十三轮）
寿命 0.25..0.3、步长 0.05 的 32 个粒子：逐步比对 CPU / GPU 死亡数与活跃数，
全部死亡后确认无幽灵粒子、继续推进不复活、再次发射沿用回收索引。
*/
TEST(Renderer_Particle_Test, GPU生命周期与死亡同步)
{
	engine::Particle_Emitter emitter = point_emitter();
	emitter.lifetime_min = 0.25f;
	emitter.lifetime_max = 0.3f;
	engine::Particle_Params params{};
	params.gravity[1] = -1.0f;
	params.damping = 0.9f;
	params.restitution = 0.4f;
	params.friction = 0.2f;
	engine::Renderer cpu;
	engine::Renderer gpu;
	ASSERT_TRUE(cpu.init(hidden_config()));
	ASSERT_TRUE(gpu.init(hidden_config()));
	ASSERT_TRUE(cpu.particle_enable_set(true));
	ASSERT_TRUE(gpu.particle_enable_set(true));
	ASSERT_TRUE(cpu.particle_capacity_set(64));
	ASSERT_TRUE(gpu.particle_capacity_set(64));
	ASSERT_TRUE(cpu.particle_emitter_set(emitter));
	ASSERT_TRUE(gpu.particle_emitter_set(emitter));
	ASSERT_TRUE(cpu.particle_params_set(params));
	ASSERT_TRUE(gpu.particle_params_set(params));
	ASSERT_EQ(cpu.particle_emit(32), 32u);
	ASSERT_EQ(gpu.particle_emit(32), 32u);
	ASSERT_TRUE(gpu.particle_gpu_available());
	//逐步比对死亡数与活跃数
	uint32_t total_died = 0;
	for (uint32_t step = 0; step < 10; ++step)
	{
		uint32_t cpu_died = cpu.particle_step(0.05f);
		ASSERT_TRUE(gpu.particle_gpu_step(0.05f));
		EXPECT_EQ(cpu_died, gpu.particle_gpu_died_last());
		EXPECT_EQ(cpu.particle_active_count(), gpu.particle_active_count());
		total_died += gpu.particle_gpu_died_last();
	}
	EXPECT_EQ(total_died, 32u);
	EXPECT_EQ(gpu.particle_active_count(), 0u);
	//无幽灵粒子：全部索引不可读
	uint32_t readable = 0;
	for (uint32_t index = 0; index < 64; ++index)
	{
		engine::Particle_State state{};
		if (gpu.particle_state_get(index, state))
			++readable;
	}
	EXPECT_EQ(readable, 0u);
	//全部死亡后继续推进不复活
	ASSERT_TRUE(gpu.particle_gpu_step(0.05f));
	EXPECT_EQ(gpu.particle_gpu_died_last(), 0u);
	EXPECT_EQ(gpu.particle_active_count(), 0u);
	//再次发射沿用回收索引，且状态可读
	EXPECT_EQ(gpu.particle_emit(32), 32u);
	EXPECT_EQ(gpu.particle_active_count(), 32u);
	readable = 0;
	for (uint32_t index = 0; index < 64; ++index)
	{
		engine::Particle_State state{};
		if (gpu.particle_state_get(index, state))
			++readable;
	}
	EXPECT_EQ(readable, 32u);
}

/*
粒子 billboard 定位（第三百一十四轮）
64×64 离屏目标、相机 (0,0,3) 注视原点：原点处单个粒子绘制后中心像素亮、边角像素保持清屏色；
关闭粒子后不再登记绘制项。
*/
TEST(Renderer_Particle_Test, 粒子billboard定位)
{
	engine::Renderer_Config config;
	config.width = 64;
	config.height = 64;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	const float clear[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	renderer.clear_color_set(clear);
	renderer.camera_position_set(0.0f, 0.0f, 3.0f);
	renderer.camera_target_set(0.0f, 0.0f, 0.0f);
	ASSERT_TRUE(renderer.particle_enable_set(true));
	ASSERT_TRUE(renderer.particle_capacity_set(8));
	engine::Particle_Emitter emitter{};
	emitter.direction[2] = -1.0f;
	emitter.speed_min = 0.0f;
	emitter.speed_max = 0.0f;
	emitter.lifetime_min = 1.0f;
	emitter.lifetime_max = 1.0f;
	emitter.shape = static_cast<uint32_t>(engine::Emit_Shape::Point);
	emitter.count = 1;
	engine::Particle_Params params{};
	params.damping = 1.0f;
	ASSERT_TRUE(renderer.particle_emitter_set(emitter));
	ASSERT_TRUE(renderer.particle_params_set(params));
	EXPECT_EQ(renderer.particle_emit(1), 1u);
	//尺寸非法拒绝
	EXPECT_FALSE(renderer.particle_draw(0.0f));
	EXPECT_FALSE(renderer.particle_draw(-1.0f));
	//登记一个实例化绘制项
	ASSERT_TRUE(renderer.particle_draw(0.5f));
	uint64_t target = 0;
	ASSERT_TRUE(renderer.render_target_create(target, 64, 64));
	renderer.render_target_set(target);
	ASSERT_TRUE(renderer.frame_render());
	std::vector<uint8_t> pixels(64 * 64 * 4, 0);
	uint32_t width = 0;
	uint32_t height = 0;
	ASSERT_TRUE(renderer.render_target_readback(target, pixels.data(), static_cast<uint32_t>(pixels.size()),
		&width, &height));
	EXPECT_EQ(width, 64u);
	EXPECT_EQ(height, 64u);
	//中心像素（BGRA：红色下标 2）显著亮于清屏色
	std::size_t center = (static_cast<std::size_t>(32) * 64 + 32) * 4;
	std::size_t corner = (static_cast<std::size_t>(4) * 64 + 4) * 4;
	EXPECT_GT(pixels[center + 2], 40u);
	EXPECT_GT(pixels[center + 0], 4u);
	EXPECT_LT(pixels[corner + 2], 8u);
	//关闭粒子后不再登记绘制项
	ASSERT_TRUE(renderer.particle_enable_set(false));
	EXPECT_FALSE(renderer.particle_draw(0.5f));
	EXPECT_FALSE(renderer.particle_gpu_available());
	renderer.render_target_set(0);
	renderer.render_target_destroy(target);
}
