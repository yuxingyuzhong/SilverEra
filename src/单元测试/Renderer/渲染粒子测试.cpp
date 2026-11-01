/*
渲染粒子门面测试（第三百一十轮；深研方向二 阶段 A）
覆盖：未初始化拒绝、默认关闭、开关与容量钳制、发射器 / 参数校验、发射与步进、关闭再开启沿用池内容。
第三百一十一轮补：GPU 推进资源可用性与零步自检。
第三百一十二轮补：GPU 单步推进与 CPU 参考逐粒子对照。
第三百一十三轮补：GPU 生命周期与死亡同步。
第三百一十四轮补：粒子 billboard 绘制与画面定位。
第三百一十五轮补：颜色 / 尺寸随年龄渐变。
*/
#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>

#include "Application/Renderer/src/render/Renderer/渲染器.h"
#include "src/主调/能力展示场景.h"

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

/*
粒子年龄颜色曲线（第三百一十五轮）
同一粒子（速度 0、重力 0、寿命 1.0）：年龄 0 与年龄 0.8 两帧的中心像素比较，
年轻者绿色分量更高、两者红色分量均保持高位（暖白 → 暗红渐变）。
*/
TEST(Renderer_Particle_Test, 粒子年龄颜色曲线)
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
	ASSERT_EQ(renderer.particle_emit(1), 1u);
	uint64_t target = 0;
	ASSERT_TRUE(renderer.render_target_create(target, 64, 64));
	renderer.render_target_set(target);
	std::size_t center = (static_cast<std::size_t>(32) * 64 + 32) * 4;
	std::vector<uint8_t> pixels(64 * 64 * 4, 0);
	uint32_t width = 0;
	uint32_t height = 0;
	//第一帧：年龄 0（暖白）
	ASSERT_TRUE(renderer.particle_draw(0.6f));
	ASSERT_TRUE(renderer.frame_render());
	ASSERT_TRUE(renderer.render_target_readback(target, pixels.data(), static_cast<uint32_t>(pixels.size()),
		&width, &height));
	uint8_t young_green = pixels[center + 1];
	uint8_t young_red = pixels[center + 2];
	//推进到年龄 0.8（未死亡）
	for (uint32_t step = 0; step < 8; ++step)
		renderer.particle_step(0.1f);
	ASSERT_EQ(renderer.particle_active_count(), 1u);
	//第二帧：年龄 0.8（暗红）
	ASSERT_TRUE(renderer.particle_draw(0.6f));
	ASSERT_TRUE(renderer.frame_render());
	std::fill(pixels.begin(), pixels.end(), 0);
	ASSERT_TRUE(renderer.render_target_readback(target, pixels.data(), static_cast<uint32_t>(pixels.size()),
		&width, &height));
	uint8_t old_green = pixels[center + 1];
	uint8_t old_red = pixels[center + 2];
	//年轻者更偏暖白（绿色更高），两者红色均保持高位
	EXPECT_GT(young_green, 100u);
	EXPECT_GT(static_cast<uint32_t>(young_green), static_cast<uint32_t>(old_green) + 30u);
	EXPECT_GT(young_red, 100u);
	EXPECT_GT(old_red, 100u);
	renderer.render_target_set(0);
	renderer.render_target_destroy(target);
}

/*
粒子混合档与深度写入（第三百一十六轮）
两枚重合粒子：Alpha 档中心像素为单层颜色，切换 Additive 后叠加增亮；
默认档为 Alpha、非法档拒绝、深度写入开关可读写（切换触发管线重建）。
*/
TEST(Renderer_Particle_Test, 粒子混合档与深度写入)
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
	emitter.count = 2;
	engine::Particle_Params params{};
	params.damping = 1.0f;
	ASSERT_TRUE(renderer.particle_emitter_set(emitter));
	ASSERT_TRUE(renderer.particle_params_set(params));
	ASSERT_EQ(renderer.particle_emit(2), 2u);
	//默认档位
	EXPECT_EQ(renderer.particle_blend_get(), engine::Blend_Mode::Alpha);
	EXPECT_FALSE(renderer.particle_depth_write_get());
	//非法档拒绝
	EXPECT_FALSE(renderer.particle_blend_set(engine::Blend_Mode::Opaque));
	uint64_t target = 0;
	ASSERT_TRUE(renderer.render_target_create(target, 64, 64));
	renderer.render_target_set(target);
	std::size_t center = (static_cast<std::size_t>(32) * 64 + 32) * 4;
	std::vector<uint8_t> pixels(64 * 64 * 4, 0);
	uint32_t width = 0;
	uint32_t height = 0;
	//Alpha 档：单层颜色
	ASSERT_TRUE(renderer.particle_blend_set(engine::Blend_Mode::Alpha));
	ASSERT_TRUE(renderer.particle_draw(0.6f));
	ASSERT_TRUE(renderer.frame_render());
	ASSERT_TRUE(renderer.render_target_readback(target, pixels.data(), static_cast<uint32_t>(pixels.size()),
		&width, &height));
	uint8_t alpha_blue = pixels[center + 0];
	//Additive 档：叠加增亮
	ASSERT_TRUE(renderer.particle_blend_set(engine::Blend_Mode::Additive));
	EXPECT_EQ(renderer.particle_blend_get(), engine::Blend_Mode::Additive);
	ASSERT_TRUE(renderer.particle_draw(0.6f));
	ASSERT_TRUE(renderer.frame_render());
	std::fill(pixels.begin(), pixels.end(), 0);
	ASSERT_TRUE(renderer.render_target_readback(target, pixels.data(), static_cast<uint32_t>(pixels.size()),
		&width, &height));
	uint8_t additive_blue = pixels[center + 0];
	EXPECT_GT(static_cast<uint32_t>(additive_blue), static_cast<uint32_t>(alpha_blue) + 30u);
	//深度写入开关（切换触发重建，绘制仍可用）
	ASSERT_TRUE(renderer.particle_depth_write_set(true));
	EXPECT_TRUE(renderer.particle_depth_write_get());
	ASSERT_TRUE(renderer.particle_depth_write_set(false));
	EXPECT_FALSE(renderer.particle_depth_write_get());
	ASSERT_TRUE(renderer.particle_draw(0.6f));
	renderer.render_target_set(0);
	renderer.render_target_destroy(target);
}

/*
粒子深度分桶近似排序（第三百一十七轮）
盒发射 16 个粒子（深度分散）：4 桶下连续两帧像素逐字节一致（结果稳定、无闪烁）；
桶数钳制到 0..16，关闭分桶后单次绘制仍可用。
*/
TEST(Renderer_Particle_Test, 粒子深度分桶稳定性)
{
	engine::Renderer_Config config;
	config.width = 64;
	config.height = 64;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	const float clear[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	renderer.clear_color_set(clear);
	renderer.camera_position_set(0.0f, 0.0f, 4.0f);
	renderer.camera_target_set(0.0f, 0.0f, 0.0f);
	ASSERT_TRUE(renderer.particle_enable_set(true));
	ASSERT_TRUE(renderer.particle_capacity_set(32));
	engine::Particle_Emitter emitter{};
	emitter.direction[2] = -1.0f;
	emitter.speed_min = 0.0f;
	emitter.speed_max = 0.0f;
	emitter.lifetime_min = 1.0f;
	emitter.lifetime_max = 1.0f;
	emitter.extents[0] = 0.6f;
	emitter.extents[1] = 0.6f;
	emitter.extents[2] = 2.0f;
	emitter.shape = static_cast<uint32_t>(engine::Emit_Shape::Box);
	emitter.count = 16;
	engine::Particle_Params params{};
	params.damping = 1.0f;
	ASSERT_TRUE(renderer.particle_emitter_set(emitter));
	ASSERT_TRUE(renderer.particle_params_set(params));
	ASSERT_EQ(renderer.particle_emit(16), 16u);
	//默认关闭分桶
	EXPECT_EQ(renderer.particle_sort_buckets_get(), 0u);
	ASSERT_TRUE(renderer.particle_sort_buckets_set(4));
	EXPECT_EQ(renderer.particle_sort_buckets_get(), 4u);
	uint64_t target = 0;
	ASSERT_TRUE(renderer.render_target_create(target, 64, 64));
	renderer.render_target_set(target);
	std::vector<uint8_t> first(64 * 64 * 4, 0);
	std::vector<uint8_t> second(64 * 64 * 4, 0);
	uint32_t width = 0;
	uint32_t height = 0;
	//第一帧
	ASSERT_TRUE(renderer.particle_draw(0.25f));
	ASSERT_TRUE(renderer.frame_render());
	ASSERT_TRUE(renderer.render_target_readback(target, first.data(), static_cast<uint32_t>(first.size()),
		&width, &height));
	//第二帧（同状态）
	ASSERT_TRUE(renderer.particle_draw(0.25f));
	ASSERT_TRUE(renderer.frame_render());
	ASSERT_TRUE(renderer.render_target_readback(target, second.data(), static_cast<uint32_t>(second.size()),
		&width, &height));
	EXPECT_EQ(first, second);
	//桶数钳制与关闭
	ASSERT_TRUE(renderer.particle_sort_buckets_set(100));
	EXPECT_EQ(renderer.particle_sort_buckets_get(), 16u);
	ASSERT_TRUE(renderer.particle_sort_buckets_set(0));
	EXPECT_EQ(renderer.particle_sort_buckets_get(), 0u);
	ASSERT_TRUE(renderer.particle_draw(0.25f));
	renderer.render_target_set(0);
	renderer.render_target_destroy(target);
}

/*
力场（第三百一十八轮）
校验：力场数与类型 / 方向非法拒绝；风场解析解（vx = 强度 × 时间、位移 = ½ 加速度 × 时间²）；
GPU 对照：风场 ＋ 引力点 ＋ 涡流十步逐粒子 1e-5。
*/
TEST(Renderer_Particle_Test, 力场校验与解析)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	ASSERT_TRUE(renderer.particle_enable_set(true));
	ASSERT_TRUE(renderer.particle_capacity_set(8));
	engine::Particle_Emitter emitter = point_emitter();
	emitter.speed_min = 0.0f;
	emitter.speed_max = 0.0f;
	emitter.direction[0] = 1.0f;
	emitter.direction[1] = 0.0f;
	emitter.direction[2] = 0.0f;
	emitter.lifetime_min = 5.0f;
	emitter.lifetime_max = 5.0f;
	emitter.count = 1;
	engine::Particle_Params params{};
	params.damping = 1.0f;
	//力场数越界
	params.force_count = 9;
	EXPECT_FALSE(renderer.particle_params_set(params));
	//涡流方向为零
	params.force_count = 1;
	params.forces[0].type = static_cast<uint32_t>(engine::Force_Type::Vortex);
	EXPECT_FALSE(renderer.particle_params_set(params));
	//风场（+X、强度 2）
	params.forces[0].type = static_cast<uint32_t>(engine::Force_Type::Wind);
	params.forces[0].strength = 2.0f;
	params.forces[0].direction[0] = 1.0f;
	ASSERT_TRUE(renderer.particle_params_set(params));
	ASSERT_TRUE(renderer.particle_emitter_set(emitter));
	ASSERT_EQ(renderer.particle_emit(1), 1u);
	//十步 × 0.1 s：vx = 2.0、px = 0.5 × 2.0 × 1.0 = 1.0（离散积分与解析差 ≤ 1e-3）
	for (uint32_t step = 0; step < 10; ++step)
		renderer.particle_step(0.1f);
	engine::Particle_State state{};
	uint32_t readable = 0;
	for (uint32_t index = 0; index < 8; ++index)
		if (renderer.particle_state_get(index, state))
			++readable;
	ASSERT_EQ(readable, 1u);
	EXPECT_NEAR(state.velocity[0], 2.0f, 1e-4f);
	EXPECT_NEAR(state.position[0], 1.1f, 1e-4f);
	EXPECT_NEAR(state.velocity[1], 0.0f, 1e-5f);
	EXPECT_NEAR(state.velocity[2], 0.0f, 1e-5f);
}

/*
力场 GPU 对照（第三百一十八轮）
风场 ＋ 引力点 ＋ 涡流三场同时生效：CPU 参考与 GPU 推进十步，逐活跃粒子 1e-5。
*/
TEST(Renderer_Particle_Test, 力场GPU对照)
{
	engine::Particle_Params params{};
	params.damping = 0.95f;
	params.force_count = 3;
	//风场：+X 方向强度 1
	params.forces[0].type = static_cast<uint32_t>(engine::Force_Type::Wind);
	params.forces[0].strength = 1.0f;
	params.forces[0].direction[0] = 1.0f;
	//引力点：原点、半径 2、强度 1
	params.forces[1].type = static_cast<uint32_t>(engine::Force_Type::Attractor);
	params.forces[1].radius = 2.0f;
	params.forces[1].strength = 1.0f;
	//涡流：绕 +Y 轴、半径 2、强度 1
	params.forces[2].type = static_cast<uint32_t>(engine::Force_Type::Vortex);
	params.forces[2].radius = 2.0f;
	params.forces[2].strength = 1.0f;
	params.forces[2].direction[1] = 1.0f;
	engine::Particle_Emitter emitter = point_emitter();
	emitter.position[0] = 1.0f;
	emitter.position[1] = 0.0f;
	emitter.position[2] = 0.0f;
	emitter.speed_min = 0.0f;
	emitter.speed_max = 0.0f;
	emitter.lifetime_min = 5.0f;
	emitter.lifetime_max = 5.0f;
	emitter.count = 4;
	engine::Renderer cpu;
	engine::Renderer gpu;
	ASSERT_TRUE(cpu.init(hidden_config()));
	ASSERT_TRUE(gpu.init(hidden_config()));
	ASSERT_TRUE(cpu.particle_enable_set(true));
	ASSERT_TRUE(gpu.particle_enable_set(true));
	ASSERT_TRUE(cpu.particle_capacity_set(8));
	ASSERT_TRUE(gpu.particle_capacity_set(8));
	ASSERT_TRUE(cpu.particle_emitter_set(emitter));
	ASSERT_TRUE(gpu.particle_emitter_set(emitter));
	ASSERT_TRUE(cpu.particle_params_set(params));
	ASSERT_TRUE(gpu.particle_params_set(params));
	ASSERT_EQ(cpu.particle_emit(4), 4u);
	ASSERT_EQ(gpu.particle_emit(4), 4u);
	ASSERT_TRUE(gpu.particle_gpu_available());
	//十步推进
	for (uint32_t step = 0; step < 10; ++step)
	{
		cpu.particle_step(0.05f);
		ASSERT_TRUE(gpu.particle_gpu_step(0.05f));
	}
	//逐活跃索引比对
	uint32_t compared = 0;
	for (uint32_t index = 0; index < 8; ++index)
	{
		engine::Particle_State expected{};
		engine::Particle_State actual{};
		if (!cpu.particle_state_get(index, expected))
			continue;
		ASSERT_TRUE(gpu.particle_state_get(index, actual));
		for (uint32_t axis = 0; axis < 3; ++axis)
		{
			EXPECT_NEAR(expected.position[axis], actual.position[axis], 1e-5f);
			EXPECT_NEAR(expected.velocity[axis], actual.velocity[axis], 1e-5f);
		}
		++compared;
	}
	EXPECT_EQ(compared, 4u);
}

/*
碰撞：地面平面（第三百一十九轮）
重力下落 30 步：粒子始终不穿透地面（最低 y ≥ 地面 − 1e-4），且出现反弹（法向速度由负转正）。
*/
TEST(Renderer_Particle_Test, 碰撞地面)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	ASSERT_TRUE(renderer.particle_enable_set(true));
	ASSERT_TRUE(renderer.particle_capacity_set(8));
	engine::Particle_Emitter emitter{};
	emitter.position[1] = 1.0f;
	emitter.direction[1] = 1.0f;
	emitter.speed_min = 0.0f;
	emitter.speed_max = 0.0f;
	emitter.lifetime_min = 10.0f;
	emitter.lifetime_max = 10.0f;
	emitter.shape = static_cast<uint32_t>(engine::Emit_Shape::Point);
	emitter.count = 1;
	engine::Particle_Params params{};
	params.gravity[1] = -2.0f;
	params.damping = 1.0f;
	params.restitution = 0.5f;
	params.friction = 0.2f;
	params.ground_enabled = 1;
	ASSERT_TRUE(renderer.particle_emitter_set(emitter));
	ASSERT_TRUE(renderer.particle_params_set(params));
	ASSERT_EQ(renderer.particle_emit(1), 1u);
	//逐步推进并记录最低点与反弹次数
	float lowest = 1e9f;
	uint32_t bounces = 0;
	float previous_velocity_y = 0.0f;
	for (uint32_t step = 0; step < 30; ++step)
	{
		renderer.particle_step(0.1f);
		engine::Particle_State state{};
		uint32_t readable = 0;
		for (uint32_t index = 0; index < 8; ++index)
			if (renderer.particle_state_get(index, state))
				++readable;
		ASSERT_EQ(readable, 1u);
		if (state.position[1] < lowest)
			lowest = state.position[1];
		//法向速度由负转正即记为一次反弹
		if (previous_velocity_y < 0.0f && state.velocity[1] > 0.0f)
			++bounces;
		previous_velocity_y = state.velocity[1];
	}
	EXPECT_GE(lowest, -1e-4f);
	EXPECT_GT(bounces, 0u);
}

/*
碰撞：球体（第三百一十九轮）
风场把粒子沿 −Z 加速撞向原点半径 0.5 的球（速度严格沿轴，避免发射扰动导致掠过球面）：
全程不穿透球面，撞后法向速度反向。
*/
TEST(Renderer_Particle_Test, 碰撞球体)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	ASSERT_TRUE(renderer.particle_enable_set(true));
	ASSERT_TRUE(renderer.particle_capacity_set(8));
	engine::Particle_Emitter emitter{};
	emitter.position[2] = 1.0f;
	emitter.direction[2] = -1.0f;
	emitter.speed_min = 0.0f;
	emitter.speed_max = 0.0f;
	emitter.lifetime_min = 10.0f;
	emitter.lifetime_max = 10.0f;
	emitter.shape = static_cast<uint32_t>(engine::Emit_Shape::Point);
	emitter.count = 1;
	engine::Particle_Params params{};
	params.damping = 1.0f;
	params.restitution = 0.5f;
	params.friction = 0.2f;
	params.sphere_radius = 0.5f;
	//风场：−Z 方向强度 20（每步 0.1 s 增加 2 m/s）
	params.force_count = 1;
	params.forces[0].type = static_cast<uint32_t>(engine::Force_Type::Wind);
	params.forces[0].strength = 20.0f;
	params.forces[0].direction[2] = -1.0f;
	ASSERT_TRUE(renderer.particle_emitter_set(emitter));
	ASSERT_TRUE(renderer.particle_params_set(params));
	ASSERT_EQ(renderer.particle_emit(1), 1u);
	//逐步推进并记录最小球心距与撞后法向速度
	float minimum_distance = 1e9f;
	bool bounced = false;
	for (uint32_t step = 0; step < 20; ++step)
	{
		renderer.particle_step(0.1f);
		engine::Particle_State state{};
		uint32_t readable = 0;
		for (uint32_t index = 0; index < 8; ++index)
			if (renderer.particle_state_get(index, state))
				++readable;
		ASSERT_EQ(readable, 1u);
		float distance = std::sqrt(state.position[0] * state.position[0]
			+ state.position[1] * state.position[1] + state.position[2] * state.position[2]);
		if (distance < minimum_distance)
			minimum_distance = distance;
		//撞球后 z 方向速度反转为正
		if (state.velocity[2] > 0.0f)
			bounced = true;
	}
	EXPECT_GE(minimum_distance, 0.5f - 1e-4f);
	EXPECT_TRUE(bounced);
}

/*
碰撞 GPU 对照（第三百一十九轮）
地面 ＋ 球体 ＋ 重力同时生效：CPU 参考与 GPU 推进二十步逐粒子位置 / 速度 1e-5。
*/
TEST(Renderer_Particle_Test, 碰撞GPU对照)
{
	engine::Particle_Params params{};
	params.gravity[1] = -2.0f;
	params.damping = 0.98f;
	params.restitution = 0.5f;
	params.friction = 0.2f;
	params.sphere_radius = 0.4f;
	params.ground_enabled = 1;
	engine::Particle_Emitter emitter = point_emitter();
	emitter.position[1] = 1.2f;
	emitter.extents[0] = 0.5f;
	emitter.extents[1] = 0.3f;
	emitter.extents[2] = 0.5f;
	emitter.lifetime_min = 10.0f;
	emitter.lifetime_max = 10.0f;
	emitter.count = 4;
	engine::Renderer cpu;
	engine::Renderer gpu;
	ASSERT_TRUE(cpu.init(hidden_config()));
	ASSERT_TRUE(gpu.init(hidden_config()));
	ASSERT_TRUE(cpu.particle_enable_set(true));
	ASSERT_TRUE(gpu.particle_enable_set(true));
	ASSERT_TRUE(cpu.particle_capacity_set(8));
	ASSERT_TRUE(gpu.particle_capacity_set(8));
	ASSERT_TRUE(cpu.particle_emitter_set(emitter));
	ASSERT_TRUE(gpu.particle_emitter_set(emitter));
	ASSERT_TRUE(cpu.particle_params_set(params));
	ASSERT_TRUE(gpu.particle_params_set(params));
	ASSERT_EQ(cpu.particle_emit(4), 4u);
	ASSERT_EQ(gpu.particle_emit(4), 4u);
	ASSERT_TRUE(gpu.particle_gpu_available());
	for (uint32_t step = 0; step < 20; ++step)
	{
		cpu.particle_step(0.05f);
		ASSERT_TRUE(gpu.particle_gpu_step(0.05f));
	}
	uint32_t compared = 0;
	for (uint32_t index = 0; index < 8; ++index)
	{
		engine::Particle_State expected{};
		engine::Particle_State actual{};
		if (!cpu.particle_state_get(index, expected))
			continue;
		ASSERT_TRUE(gpu.particle_state_get(index, actual));
		for (uint32_t axis = 0; axis < 3; ++axis)
		{
			EXPECT_NEAR(expected.position[axis], actual.position[axis], 1e-5f);
			EXPECT_NEAR(expected.velocity[axis], actual.velocity[axis], 1e-5f);
		}
		++compared;
	}
	EXPECT_EQ(compared, 4u);
}

/*
超大容量与重建（第三百二十轮）
容量上限 1 048 576（双缓冲 64 MB ＋ 力场 320 B）：资源可用、零步自检通过、观测字节正确；
同值重复设置不重建；收缩到 64 后按新容量重建且可用。
*/
TEST(Renderer_Particle_Test, 超大容量与重建)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	ASSERT_TRUE(renderer.particle_enable_set(true));
	//未创建资源时为 0
	EXPECT_EQ(renderer.particle_gpu_bytes(), 0u);
	ASSERT_TRUE(renderer.particle_capacity_set(engine::PARTICLE_CAPACITY_MAX));
	EXPECT_EQ(renderer.particle_capacity_get(), engine::PARTICLE_CAPACITY_MAX);
	ASSERT_TRUE(renderer.particle_gpu_available());
	const uint64_t expected_maximum = static_cast<uint64_t>(engine::PARTICLE_CAPACITY_MAX)
		* engine::PARTICLE_STRIDE * 2u + static_cast<uint64_t>(engine::PARTICLE_FORCE_MAX) * engine::PARTICLE_FORCE_STRIDE
		+ (8u + (static_cast<uint64_t>(engine::PARTICLE_CAPACITY_MAX) + 31u) / 32u * 4u);
	EXPECT_EQ(renderer.particle_gpu_bytes(), expected_maximum);
	EXPECT_TRUE(renderer.particle_gpu_probe());
	//同值重复设置：不重建、字节不变
	ASSERT_TRUE(renderer.particle_capacity_set(engine::PARTICLE_CAPACITY_MAX));
	EXPECT_EQ(renderer.particle_capacity_get(), engine::PARTICLE_CAPACITY_MAX);
	EXPECT_EQ(renderer.particle_gpu_bytes(), expected_maximum);
	//超上限钳到上限
	ASSERT_TRUE(renderer.particle_capacity_set(engine::PARTICLE_CAPACITY_MAX + 1u));
	EXPECT_EQ(renderer.particle_capacity_get(), engine::PARTICLE_CAPACITY_MAX);
	//收缩重建
	ASSERT_TRUE(renderer.particle_capacity_set(64));
	EXPECT_EQ(renderer.particle_capacity_get(), 64u);
	ASSERT_TRUE(renderer.particle_gpu_available());
	const uint64_t expected_small = static_cast<uint64_t>(64) * engine::PARTICLE_STRIDE * 2u
		+ static_cast<uint64_t>(engine::PARTICLE_FORCE_MAX) * engine::PARTICLE_FORCE_STRIDE
		+ (8u + (64u + 31u) / 32u * 4u);
	EXPECT_EQ(renderer.particle_gpu_bytes(), expected_small);
	EXPECT_TRUE(renderer.particle_gpu_probe());
}

/*
粒子模拟统计（第三百二十二轮）
CPU 参考路径：出生累计 16、寿命到期死亡 16、活跃 0（出生 − 死亡 = 活跃）；主机侧模拟耗时 ≥ 0。
GPU 路径：十步后统计自洽；设备支持时间戳时 particle_gpu_ms ≥ 0，否则为 −1。
*/
TEST(Renderer_Particle_Test, 粒子模拟统计)
{
	engine::Particle_Emitter emitter = point_emitter();
	emitter.lifetime_min = 0.5f;
	emitter.lifetime_max = 0.6f;
	emitter.count = 16;
	engine::Particle_Params params{};
	params.damping = 1.0f;
	//CPU 参考路径
	engine::Renderer cpu;
	ASSERT_TRUE(cpu.init(hidden_config()));
	ASSERT_TRUE(cpu.particle_enable_set(true));
	ASSERT_TRUE(cpu.particle_capacity_set(64));
	ASSERT_TRUE(cpu.particle_emitter_set(emitter));
	ASSERT_TRUE(cpu.particle_params_set(params));
	engine::Frame_Stats stats = cpu.frame_stats_get();
	EXPECT_EQ(stats.particle_active, 0u);
	EXPECT_EQ(stats.particle_born_total, 0u);
	EXPECT_EQ(stats.particle_died_total, 0u);
	ASSERT_EQ(cpu.particle_emit(16), 16u);
	stats = cpu.frame_stats_get();
	EXPECT_EQ(stats.particle_active, 16u);
	EXPECT_EQ(stats.particle_born_total, 16u);
	//十二步 × 0.05 s = 0.6 s，寿命 ≤ 0.6 s 全部回收
	for (uint32_t step = 0; step < 12; ++step)
		cpu.particle_step(0.05f);
	stats = cpu.frame_stats_get();
	EXPECT_EQ(stats.particle_active, 0u);
	EXPECT_EQ(stats.particle_born_total, 16u);
	EXPECT_EQ(stats.particle_died_total, 16u);
	EXPECT_EQ(stats.particle_born_total - stats.particle_died_total, stats.particle_active);
	EXPECT_GE(stats.particle_sim_ms, 0.0);
	//GPU 路径
	engine::Renderer gpu;
	ASSERT_TRUE(gpu.init(hidden_config()));
	ASSERT_TRUE(gpu.particle_enable_set(true));
	ASSERT_TRUE(gpu.particle_capacity_set(64));
	ASSERT_TRUE(gpu.particle_emitter_set(emitter));
	ASSERT_TRUE(gpu.particle_params_set(params));
	ASSERT_EQ(gpu.particle_emit(16), 16u);
	ASSERT_TRUE(gpu.particle_gpu_available());
	for (uint32_t step = 0; step < 12; ++step)
		ASSERT_TRUE(gpu.particle_gpu_step(0.05f));
	stats = gpu.frame_stats_get();
	EXPECT_EQ(stats.particle_active, 0u);
	EXPECT_EQ(stats.particle_born_total, 16u);
	EXPECT_EQ(stats.particle_died_total, 16u);
	EXPECT_EQ(stats.particle_born_total - stats.particle_died_total, stats.particle_active);
	EXPECT_GE(stats.particle_sim_ms, 0.0);
	if (gpu.compute_timestamp_supported())
		EXPECT_GE(stats.particle_gpu_ms, 0.0);
	else
		EXPECT_EQ(stats.particle_gpu_ms, -1.0);
}

/*
粒子降级矩阵（第三百二十三轮）
四组环境（计算关闭 / 计算开启 × 粒子关闭 / 粒子开启）：
(i) 粒子关闭与计算开关均不改变既有画面（逐字节一致）；
(ii) 计算关闭时 CPU 镜像路径仍可绘制（与计算开启时的 CPU 绘制逐字节一致）；
(iii) 开启粒子的绘制与关闭态画面不同（确实生效）。
*/
TEST(Renderer_Particle_Test, 粒子降级矩阵)
{
	//离屏渲染到 64×64 目标并读回（BGRA）
	auto render_once = [](engine::Renderer& renderer, engine::Mesh_Handle plane, uint64_t target,
		bool with_particle, std::vector<uint8_t>& out)
	{
		renderer.render_target_set(target);
		float transform[16] = { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f };
		renderer.mesh_draw(plane, transform);
		//可选的粒子绘制
		if (with_particle)
			renderer.particle_draw(0.4f);
		EXPECT_TRUE(renderer.frame_render());
		out.assign(64 * 64 * 4, 0);
		uint32_t width = 0;
		uint32_t height = 0;
		EXPECT_TRUE(renderer.render_target_readback(target, out.data(), static_cast<uint32_t>(out.size()),
			&width, &height));
	};
	//初始化配置（可切换计算路径）
	auto make_config = [](bool compute_enable)
	{
		engine::Renderer_Config config;
		config.width = 64;
		config.height = 64;
		config.hidden = true;
		config.compute_enable = compute_enable;
		return config;
	};
	engine::Renderer_Config config_off = make_config(false);
	engine::Renderer_Config config_on = make_config(true);
	std::vector<uint8_t> plain_off;
	std::vector<uint8_t> plain_off_particles_enabled;
	std::vector<uint8_t> plain_on;
	//(1) 计算关闭 ＋ 粒子关闭
	{
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(config_off));
		engine::Mesh_Handle plane = 0;
		ASSERT_TRUE(renderer.mesh_plane_load(plane, 1.0f, 1.0f));
		uint64_t target = 0;
		ASSERT_TRUE(renderer.render_target_create(target, 64, 64));
		render_once(renderer, plane, target, false, plain_off);
		renderer.render_target_destroy(target);
		renderer.mesh_unload(plane);
	}
	//(2) 计算关闭 ＋ 粒子开启（不绘制）
	{
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(config_off));
		ASSERT_TRUE(renderer.particle_enable_set(true));
		engine::Mesh_Handle plane = 0;
		ASSERT_TRUE(renderer.mesh_plane_load(plane, 1.0f, 1.0f));
		uint64_t target = 0;
		ASSERT_TRUE(renderer.render_target_create(target, 64, 64));
		render_once(renderer, plane, target, false, plain_off_particles_enabled);
		renderer.render_target_destroy(target);
		renderer.mesh_unload(plane);
	}
	EXPECT_EQ(plain_off, plain_off_particles_enabled);
	//(3) 计算开启 ＋ 粒子关闭
	{
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(config_on));
		engine::Mesh_Handle plane = 0;
		ASSERT_TRUE(renderer.mesh_plane_load(plane, 1.0f, 1.0f));
		uint64_t target = 0;
		ASSERT_TRUE(renderer.render_target_create(target, 64, 64));
		render_once(renderer, plane, target, false, plain_on);
		renderer.render_target_destroy(target);
		renderer.mesh_unload(plane);
	}
	EXPECT_EQ(plain_off, plain_on);
	//参数：单粒子（原点、速度 0、寿命 1）
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
	std::vector<uint8_t> particle_off;
	std::vector<uint8_t> particle_on;
	//(4) 计算关闭 ＋ 粒子开启 ＋ 绘制
	{
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(config_off));
		ASSERT_TRUE(renderer.particle_enable_set(true));
		ASSERT_TRUE(renderer.particle_capacity_set(8));
		ASSERT_TRUE(renderer.particle_emitter_set(emitter));
		ASSERT_TRUE(renderer.particle_params_set(params));
		ASSERT_EQ(renderer.particle_emit(1), 1u);
		//计算不可用：GPU 路径关闭，CPU 镜像仍可用
		EXPECT_FALSE(renderer.particle_gpu_available());
		EXPECT_FALSE(renderer.particle_gpu_step(0.1f));
		EXPECT_FALSE(renderer.particle_supported());
		renderer.camera_position_set(0.0f, 0.0f, 3.0f);
		renderer.camera_target_set(0.0f, 0.0f, 0.0f);
		engine::Mesh_Handle plane = 0;
		ASSERT_TRUE(renderer.mesh_plane_load(plane, 4.0f, 4.0f));
		uint64_t target = 0;
		ASSERT_TRUE(renderer.render_target_create(target, 64, 64));
		render_once(renderer, plane, target, true, particle_off);
		renderer.render_target_destroy(target);
		renderer.mesh_unload(plane);
	}
	//(5) 计算开启 ＋ 粒子开启 ＋ 绘制（同一 CPU 状态）
	{
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(config_on));
		ASSERT_TRUE(renderer.particle_enable_set(true));
		ASSERT_TRUE(renderer.particle_capacity_set(8));
		ASSERT_TRUE(renderer.particle_emitter_set(emitter));
		ASSERT_TRUE(renderer.particle_params_set(params));
		ASSERT_EQ(renderer.particle_emit(1), 1u);
		EXPECT_TRUE(renderer.particle_supported());
		renderer.camera_position_set(0.0f, 0.0f, 3.0f);
		renderer.camera_target_set(0.0f, 0.0f, 0.0f);
		engine::Mesh_Handle plane = 0;
		ASSERT_TRUE(renderer.mesh_plane_load(plane, 4.0f, 4.0f));
		uint64_t target = 0;
		ASSERT_TRUE(renderer.render_target_create(target, 64, 64));
		render_once(renderer, plane, target, true, particle_on);
		renderer.render_target_destroy(target);
		renderer.mesh_unload(plane);
	}
	//(6) 计算开关不影响 CPU 绘制结果；粒子确实改变画面
	EXPECT_EQ(particle_off, particle_on);
	uint32_t changed = 0;
	for (std::size_t index = 0; index < plain_off.size(); ++index)
		if (plain_off[index] != particle_off[index])
			++changed;
	EXPECT_GT(changed, 20u);
}

/*
展廊第九区接入（第三百二十四轮）
隐藏窗口初始化能力展示场景：粒子容量预置 4096、默认关闭；
手动开启后发射 64 个并逐帧绘制出帧，出生累计与绘制项均增长。
*/
TEST(Renderer_Particle_Test, 展廊粒子喷泉接入)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::能力展示场景 场景;
	ASSERT_TRUE(场景.init(renderer));
	//预配置：容量 4096、默认关闭
	EXPECT_EQ(renderer.particle_capacity_get(), 4096u);
	EXPECT_FALSE(renderer.particle_enable_get());
	//手动开启并模拟区内一帧
	ASSERT_TRUE(renderer.particle_enable_set(true));
	ASSERT_EQ(renderer.particle_emit(64), 64u);
	(void)renderer.particle_step(0.08f);
	场景.frame_draw(renderer);
	engine::Frame_Stats stats = renderer.frame_stats_get();
	EXPECT_EQ(stats.particle_born_total, 64u);
	EXPECT_EQ(stats.particle_active, 64u);
	//出帧并确认有绘制项
	ASSERT_TRUE(renderer.frame_render());
	EXPECT_GT(renderer.frame_stats_get().drawn_items, 0u);
}

/*
粒子性能基线（第三百二十五轮）
口径：1920×1080（MSAA 4×，设备不支持时自动降级）、1 万活跃粒子、容量 65 536；
记录 CPU 参考单步、GPU 同步链单步（上传 / 调度 / 回读）与整帧绘制的主机 / GPU 耗时；
另测 1M 容量资源分配与零步自检。
*/
TEST(Renderer_Particle_Test, 粒子性能基线)
{
	engine::Renderer_Config config;
	config.width = 1920;
	config.height = 1080;
	config.hidden = true;
	config.msaa_samples = 4;
	//1 万粒子一次实例化提交：实例容量需覆盖
	config.instance_capacity = 16384;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	ASSERT_TRUE(renderer.particle_enable_set(true));
	ASSERT_TRUE(renderer.particle_capacity_set(65536));
	engine::Particle_Emitter emitter = point_emitter();
	emitter.count = 10000;
	emitter.speed_min = 0.6f;
	emitter.speed_max = 1.4f;
	emitter.lifetime_min = 4.0f;
	emitter.lifetime_max = 6.0f;
	engine::Particle_Params params{};
	params.gravity[1] = -0.8f;
	params.damping = 0.99f;
	ASSERT_TRUE(renderer.particle_emitter_set(emitter));
	ASSERT_TRUE(renderer.particle_params_set(params));
	ASSERT_EQ(renderer.particle_emit(10000), 10000u);
	//CPU 参考单步（十步平均）
	const auto cpu_begin = std::chrono::high_resolution_clock::now();
	for (uint32_t step = 0; step < 10; ++step)
		renderer.particle_step(1.0f / 60.0f);
	const auto cpu_end = std::chrono::high_resolution_clock::now();
	double cpu_step_ms = std::chrono::duration<double, std::milli>(cpu_end - cpu_begin).count() / 10.0;
	//GPU 同步链单步（十步平均）
	ASSERT_TRUE(renderer.particle_gpu_available());
	const auto gpu_begin = std::chrono::high_resolution_clock::now();
	for (uint32_t step = 0; step < 10; ++step)
		ASSERT_TRUE(renderer.particle_gpu_step(1.0f / 60.0f));
	const auto gpu_end = std::chrono::high_resolution_clock::now();
	double gpu_step_ms = std::chrono::duration<double, std::milli>(gpu_end - gpu_begin).count() / 10.0;
	//整帧绘制（1 万 billboard）
	ASSERT_TRUE(renderer.particle_draw(0.06f));
	ASSERT_TRUE(renderer.frame_render());
	const engine::Frame_Stats draw_stats = renderer.frame_stats_get();
	//1M 容量资源分配与零步自检
	const auto alloc_begin = std::chrono::high_resolution_clock::now();
	ASSERT_TRUE(renderer.particle_capacity_set(engine::PARTICLE_CAPACITY_MAX));
	ASSERT_TRUE(renderer.particle_gpu_available());
	const auto alloc_end = std::chrono::high_resolution_clock::now();
	double alloc_ms = std::chrono::duration<double, std::milli>(alloc_end - alloc_begin).count();
	EXPECT_EQ(renderer.particle_gpu_bytes(),
		static_cast<uint64_t>(engine::PARTICLE_CAPACITY_MAX) * engine::PARTICLE_STRIDE * 2u
			+ static_cast<uint64_t>(engine::PARTICLE_FORCE_MAX) * engine::PARTICLE_FORCE_STRIDE
			+ (8u + (static_cast<uint64_t>(engine::PARTICLE_CAPACITY_MAX) + 31u) / 32u * 4u));
	const auto probe_begin = std::chrono::high_resolution_clock::now();
	EXPECT_TRUE(renderer.particle_gpu_probe());
	const auto probe_end = std::chrono::high_resolution_clock::now();
	double probe_ms = std::chrono::duration<double, std::milli>(probe_end - probe_begin).count();
	std::cout << "[粒子基线] CPU步进=" << cpu_step_ms << " ms  GPU同步步进=" << gpu_step_ms
		<< " ms  录制=" << draw_stats.host_record_ms << " ms  GPU帧=" << draw_stats.gpu_frame_ms
		<< " ms  1M分配=" << alloc_ms << " ms  1M零步自检=" << probe_ms << " ms" << std::endl;
	//合理上限（防止性能退化）
	EXPECT_LT(cpu_step_ms, 10.0);
	EXPECT_LT(gpu_step_ms, 200.0);
	EXPECT_LT(draw_stats.host_record_ms, 10.0);
	EXPECT_LT(alloc_ms + probe_ms, 2000.0);
}

/*
常驻 GPU 发射（第三百二十七轮）
环形写指针 ＋ 活跃位图 ＋ 活跃计数：三次发射后活跃数守恒、池满截断、出生统计同步；
关闭常驻回到 CPU 镜像活跃数；计算不可用时拒绝常驻与 GPU 发射。
*/
TEST(Renderer_Particle_Test, 常驻GPU发射)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	ASSERT_TRUE(renderer.particle_enable_set(true));
	ASSERT_TRUE(renderer.particle_capacity_set(64));
	engine::Particle_Emitter emitter{};
	emitter.direction[1] = 1.0f;
	emitter.speed_min = 0.0f;
	emitter.speed_max = 0.0f;
	emitter.lifetime_min = 1.0f;
	emitter.lifetime_max = 1.0f;
	emitter.shape = static_cast<uint32_t>(engine::Emit_Shape::Point);
	emitter.count = 10;
	engine::Particle_Params params{};
	params.damping = 1.0f;
	ASSERT_TRUE(renderer.particle_emitter_set(emitter));
	ASSERT_TRUE(renderer.particle_params_set(params));
	//常驻关闭：GPU 发射不生效
	EXPECT_FALSE(renderer.particle_resident_get());
	EXPECT_EQ(renderer.particle_gpu_emit(5), 0u);
	//开启常驻
	ASSERT_TRUE(renderer.particle_resident_set(true));
	EXPECT_TRUE(renderer.particle_resident_get());
	EXPECT_EQ(renderer.particle_active_count(), 0u);
	//三次发射
	EXPECT_EQ(renderer.particle_gpu_emit(10), 10u);
	EXPECT_EQ(renderer.particle_active_count(), 10u);
	EXPECT_EQ(renderer.particle_gpu_emit(10), 10u);
	EXPECT_EQ(renderer.particle_active_count(), 20u);
	//池满截断（64 容量）
	EXPECT_EQ(renderer.particle_gpu_emit(64), 44u);
	EXPECT_EQ(renderer.particle_active_count(), 64u);
	//已满
	EXPECT_EQ(renderer.particle_gpu_emit(8), 0u);
	EXPECT_EQ(renderer.frame_stats_get().particle_born_total, 64u);
	//关闭常驻：回到 CPU 镜像（未发射 → 0）
	ASSERT_TRUE(renderer.particle_resident_set(false));
	EXPECT_EQ(renderer.particle_active_count(), 0u);
	//计算不可用：拒绝常驻与 GPU 发射
	engine::Renderer_Config config_off;
	config_off.width = 64;
	config_off.height = 64;
	config_off.hidden = true;
	config_off.compute_enable = false;
	engine::Renderer renderer_off;
	ASSERT_TRUE(renderer_off.init(config_off));
	ASSERT_TRUE(renderer_off.particle_enable_set(true));
	EXPECT_FALSE(renderer_off.particle_resident_set(true));
	EXPECT_FALSE(renderer_off.particle_resident_get());
	EXPECT_EQ(renderer_off.particle_gpu_emit(4), 0u);
}

/*
常驻推进与死亡压缩（第三百二十八轮）
常驻发射 32 个（寿命 0.5..0.6）：十二步 0.05 s 后活跃归零、死亡累计 32、无幽灵；
再次发射 8 个沿用环形指针；常驻下同步链推进 / 自检被拒，关闭常驻后恢复。
*/
TEST(Renderer_Particle_Test, 常驻推进与死亡压缩)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	ASSERT_TRUE(renderer.particle_enable_set(true));
	ASSERT_TRUE(renderer.particle_capacity_set(64));
	engine::Particle_Emitter emitter{};
	emitter.direction[1] = 1.0f;
	emitter.speed_min = 0.0f;
	emitter.speed_max = 0.0f;
	emitter.lifetime_min = 0.5f;
	emitter.lifetime_max = 0.6f;
	emitter.shape = static_cast<uint32_t>(engine::Emit_Shape::Point);
	emitter.count = 32;
	engine::Particle_Params params{};
	params.damping = 1.0f;
	ASSERT_TRUE(renderer.particle_emitter_set(emitter));
	ASSERT_TRUE(renderer.particle_params_set(params));
	ASSERT_TRUE(renderer.particle_resident_set(true));
	//常驻发射 32
	EXPECT_EQ(renderer.particle_gpu_emit(32), 32u);
	EXPECT_EQ(renderer.particle_active_count(), 32u);
	//十二步 0.05 s → 全部死亡
	const auto begin = std::chrono::high_resolution_clock::now();
	uint32_t died_total = 0;
	for (uint32_t step = 0; step < 12; ++step)
	{
		ASSERT_TRUE(renderer.particle_gpu_step_resident(0.05f));
		died_total += renderer.particle_gpu_died_last();
	}
	const auto end = std::chrono::high_resolution_clock::now();
	double per_step_ms = std::chrono::duration<double, std::milli>(end - begin).count() / 12.0;
	EXPECT_EQ(renderer.particle_active_count(), 0u);
	EXPECT_EQ(died_total, 32u);
	EXPECT_EQ(renderer.frame_stats_get().particle_died_total, 32u);
	//再次发射沿用环形指针
	EXPECT_EQ(renderer.particle_gpu_emit(8), 8u);
	EXPECT_EQ(renderer.particle_active_count(), 8u);
	//常驻下同步链推进 / 自检被拒
	EXPECT_FALSE(renderer.particle_gpu_step(0.05f));
	EXPECT_FALSE(renderer.particle_gpu_probe());
	//关闭常驻：同步链恢复可用
	ASSERT_TRUE(renderer.particle_resident_set(false));
	EXPECT_TRUE(renderer.particle_gpu_probe());
	//常驻单步主机耗时上限（提交 + 围栏 + 两次 4 B 回读）
	std::cout << "[常驻推进] 单步主机耗时=" << per_step_ms << " ms" << std::endl;
	EXPECT_LT(per_step_ms, 5.0);
}

/*
常驻百万粒子基线（第三百二十九轮；方向二阶段 C 收口）
容量 1 048 576：常驻 GPU 发射 100 万 → 五步常驻推进（寿命 100 不死亡）→ 活跃计数守恒；
记录发射与单步耗时（主机侧），并断言合理上限。
*/
TEST(Renderer_Particle_Test, 常驻百万粒子基线)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	ASSERT_TRUE(renderer.particle_enable_set(true));
	ASSERT_TRUE(renderer.particle_capacity_set(engine::PARTICLE_CAPACITY_MAX));
	engine::Particle_Emitter emitter{};
	emitter.direction[1] = 1.0f;
	emitter.speed_min = 0.5f;
	emitter.speed_max = 1.0f;
	emitter.lifetime_min = 100.0f;
	emitter.lifetime_max = 100.0f;
	emitter.shape = static_cast<uint32_t>(engine::Emit_Shape::Sphere_Shell);
	emitter.extents[0] = 1.0f;
	emitter.count = engine::PARTICLE_EMIT_COUNT_MAX;
	engine::Particle_Params params{};
	params.gravity[1] = -0.5f;
	params.damping = 0.99f;
	ASSERT_TRUE(renderer.particle_emitter_set(emitter));
	ASSERT_TRUE(renderer.particle_params_set(params));
	ASSERT_TRUE(renderer.particle_resident_set(true));
	//一次发射 1 048 576（分 16 批，每批 ≤ 65536 上限）
	const auto emit_begin = std::chrono::high_resolution_clock::now();
	uint32_t total = 0;
	for (uint32_t batch = 0; batch < 16; ++batch)
		total += renderer.particle_gpu_emit(engine::PARTICLE_EMIT_COUNT_MAX);
	const auto emit_end = std::chrono::high_resolution_clock::now();
	double emit_ms = std::chrono::duration<double, std::milli>(emit_end - emit_begin).count();
	EXPECT_EQ(total, engine::PARTICLE_CAPACITY_MAX);
	EXPECT_EQ(renderer.particle_active_count(), engine::PARTICLE_CAPACITY_MAX);
	//五步常驻推进
	const auto step_begin = std::chrono::high_resolution_clock::now();
	for (uint32_t step = 0; step < 5; ++step)
		ASSERT_TRUE(renderer.particle_gpu_step_resident(1.0f / 60.0f));
	const auto step_end = std::chrono::high_resolution_clock::now();
	double per_step_ms = std::chrono::duration<double, std::milli>(step_end - step_begin).count() / 5.0;
	//寿命 100 s 内不死亡 → 活跃守恒
	EXPECT_EQ(renderer.particle_active_count(), engine::PARTICLE_CAPACITY_MAX);
	EXPECT_EQ(renderer.particle_gpu_died_last(), 0u);
	std::cout << "[常驻百万] 发射16批=" << emit_ms << " ms  单步=" << per_step_ms << " ms" << std::endl;
	//百万粒子单步为 GPU 吞吐口径（实测约 100 ms 量级，见性能基线）
	EXPECT_LT(per_step_ms, 1000.0);
	EXPECT_LT(emit_ms, 10000.0);
}


/*
常驻绘制（第三百三十轮）
常驻发射 256 个后回读位图与状态并按活跃位构建实例：出帧有绘制项、中心区域出现粒子像素；
容量超过 65536 时常驻绘制返回假（大容量绘制留待间接绘制）。
*/
TEST(Renderer_Particle_Test, 常驻绘制)
{
	engine::Renderer_Config config;
	config.width = 64;
	config.height = 64;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	ASSERT_TRUE(renderer.particle_enable_set(true));
	ASSERT_TRUE(renderer.particle_capacity_set(4096));
	engine::Particle_Emitter emitter{};
	emitter.direction[1] = 1.0f;
	emitter.speed_min = 0.5f;
	emitter.speed_max = 1.0f;
	emitter.lifetime_min = 1.0f;
	emitter.lifetime_max = 1.0f;
	emitter.shape = static_cast<uint32_t>(engine::Emit_Shape::Sphere_Shell);
	emitter.extents[0] = 0.3f;
	emitter.count = 256;
	engine::Particle_Params params{};
	params.damping = 1.0f;
	ASSERT_TRUE(renderer.particle_emitter_set(emitter));
	ASSERT_TRUE(renderer.particle_params_set(params));
	ASSERT_TRUE(renderer.particle_resident_set(true));
	ASSERT_EQ(renderer.particle_gpu_emit(256), 256u);
	EXPECT_EQ(renderer.particle_active_count(), 256u);
	const float clear[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	renderer.clear_color_set(clear);
	renderer.camera_position_set(0.0f, 0.0f, 4.0f);
	renderer.camera_target_set(0.0f, 0.0f, 0.0f);
	uint64_t target = 0;
	ASSERT_TRUE(renderer.render_target_create(target, 64, 64));
	renderer.render_target_set(target);
	//常驻绘制与出帧
	ASSERT_TRUE(renderer.particle_draw(0.1f));
	ASSERT_TRUE(renderer.frame_render());
	EXPECT_GT(renderer.frame_stats_get().drawn_items, 0u);
	std::vector<uint8_t> pixels(64 * 64 * 4, 0);
	uint32_t width = 0;
	uint32_t height = 0;
	ASSERT_TRUE(renderer.render_target_readback(target, pixels.data(), static_cast<uint32_t>(pixels.size()),
		&width, &height));
	uint32_t visible = 0;
	for (std::size_t index = 0; index + 3 < pixels.size(); index += 4)
	{
		//任意通道明显高于清屏色即视为粒子像素
		if (pixels[index + 0] > 8u || pixels[index + 1] > 8u || pixels[index + 2] > 8u)
			++visible;
	}
	EXPECT_GT(visible, 0u);
	//超过 65536 容量：常驻绘制返回假（留待间接绘制）
	ASSERT_TRUE(renderer.particle_capacity_set(engine::PARTICLE_CAPACITY_MAX));
	EXPECT_FALSE(renderer.particle_draw(0.1f));
	renderer.render_target_set(0);
	renderer.render_target_destroy(target);
}

/*
展廊粒子常驻接入（第三百三十一轮）
场景预配置的发射器 / 参数下走常驻路径：三轮「GPU 发射 64 ＋ 常驻推进」后活跃与出生守恒，
出帧有绘制项；关闭常驻后回到 CPU 参考路径（活跃数取 CPU 镜像）。
*/
TEST(Renderer_Particle_Test, 展廊粒子常驻接入)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::能力展示场景 场景;
	ASSERT_TRUE(场景.init(renderer));
	//预配置沿用：容量 4096、默认关闭
	EXPECT_EQ(renderer.particle_capacity_get(), 4096u);
	EXPECT_FALSE(renderer.particle_enable_get());
	//按场景进入区内的顺序：先开粒子再切常驻
	ASSERT_TRUE(renderer.particle_enable_set(true));
	ASSERT_TRUE(renderer.particle_resident_set(true));
	EXPECT_TRUE(renderer.particle_resident_get());
	//三轮区内发射 ＋ 推进（寿命 1.4 s 起，三步 0.08 s 不死亡）
	for (uint32_t frame = 0; frame < 3; ++frame)
	{
		ASSERT_EQ(renderer.particle_gpu_emit(64), 64u);
		ASSERT_TRUE(renderer.particle_gpu_step_resident(0.08f));
	}
	const engine::Frame_Stats stats = renderer.frame_stats_get();
	EXPECT_EQ(stats.particle_born_total, 192u);
	EXPECT_EQ(renderer.particle_active_count(), 192u);
	//出帧并有绘制项（常驻绘制按位图展开）
	场景.frame_draw(renderer);
	ASSERT_TRUE(renderer.frame_render());
	EXPECT_GT(renderer.frame_stats_get().drawn_items, 0u);
	//关闭常驻：回到 CPU 参考路径（CPU 镜像未发射 → 0）
	ASSERT_TRUE(renderer.particle_resident_set(false));
	EXPECT_FALSE(renderer.particle_resident_get());
	EXPECT_EQ(renderer.particle_active_count(), 0u);
	ASSERT_TRUE(renderer.particle_emit(8));
	EXPECT_EQ(renderer.particle_active_count(), 8u);
}
