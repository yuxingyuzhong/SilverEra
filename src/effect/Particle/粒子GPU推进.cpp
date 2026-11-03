#include "src/effect/Particle/粒子GPU推进.h"
//渲染器私有实现（缓冲 / 管线 / 帧槽字段）
#include "Engine/EngineSystem/src/core/Renderer/src/render/Renderer/core/渲染器实现.h"

//时间与数值
#include <chrono>
#include <cmath>
#include <cstdint>

namespace engine
{
	namespace particle_gpu
	{
	/*
	GPU 发射（常驻模式；第三百二十七轮）
	回读活跃计数 → 计算本次可发射数 → 绑定状态 / 控制 / 力场 / 占位 → 调度发射管线 → 再回读计数。
	*/
	uint32_t emit(Renderer& renderer, uint32_t count)
	{
		//前置条件
		if (renderer.impl == nullptr || !renderer.impl->initialized || !renderer.impl->particle_enabled || !renderer.impl->particle_resident
			|| !renderer.impl->particle_emitter_configured || count == 0)
			return 0;
		if (!renderer.particle_gpu_available())
			return 0;
		uint32_t capacity = renderer.impl->particle_buffer_capacity;
		//当前活跃计数（GPU 侧 [1]）
		uint32_t active = 0;
		if (!renderer.compute_buffer_read(renderer.impl->particle_control_buffer, &active, 4u, 4u))
			return 0;
		renderer.impl->particle_resident_active = active;
		//池满
		if (active >= capacity)
			return 0;
		//本次发射数（不超过剩余容量）
		uint32_t budget = count;
		if (active + budget > capacity)
			budget = capacity - active;
		//绑定：状态（只写新槽）/ 控制 / 力场（占位）/ 状态（占位）
		if (!renderer.compute_buffer_bind(renderer.impl->particle_buffer_front, 0))
			return 0;
		if (!renderer.compute_buffer_bind(renderer.impl->particle_control_buffer, 1))
			return 0;
		if (!renderer.compute_buffer_bind(renderer.impl->particle_force_buffer, 2))
			return 0;
		if (!renderer.compute_buffer_bind(renderer.impl->particle_buffer_front, 3))
			return 0;
		const Particle_Emitter& emitter = renderer.impl->particle_emitter;
		//推送常量：位置与形状 / 速度与寿命 / 容量数量种子半径 / 主方向
		const float push[16] = {
			emitter.position[0], emitter.position[1], emitter.position[2], static_cast<float>(emitter.shape),
			emitter.speed_min, emitter.speed_max, emitter.lifetime_min, emitter.lifetime_max,
			static_cast<float>(capacity), static_cast<float>(budget),
			static_cast<float>(renderer.impl->particle_born_total + 1u), emitter.extents[0],
			emitter.direction[0], emitter.direction[1], emitter.direction[2], 0.0f
		};
		if (!renderer.compute_dispatch(renderer.impl->particle_emit_pipeline, budget, 64u, push, sizeof(push)))
			return 0;
		//回读新计数并结算实际发射数
		uint32_t after = 0;
		if (!renderer.compute_buffer_read(renderer.impl->particle_control_buffer, &after, 4u, 4u))
			return 0;
		renderer.impl->particle_resident_active = after;
		uint32_t spawned = after > active ? after - active : 0u;
		renderer.impl->particle_born_total += spawned;
		return spawned;
	}

	/*
	常驻推进（第三百二十八轮）
	读 front / 写 back → 交换前后缓冲 → 回读活跃计数；死亡数 = 前计数 − 后计数。
	*/
	bool step_resident(Renderer& renderer, float dt)
	{
		//前置条件
		if (renderer.impl == nullptr || !renderer.impl->initialized || !renderer.impl->particle_enabled || !renderer.impl->particle_resident)
			return false;
		if (!std::isfinite(dt) || dt <= 0.0f)
			return false;
		if (!renderer.particle_gpu_available())
			return false;
		//主机侧计时（第三百二十八轮）
		const auto sim_begin = std::chrono::high_resolution_clock::now();
		uint32_t capacity = renderer.impl->particle_buffer_capacity;
		//推进前活跃计数
		uint32_t before = 0;
		if (!renderer.compute_buffer_read(renderer.impl->particle_control_buffer, &before, 4u, 4u))
			return false;
		renderer.impl->particle_resident_active = before;
		//上传力场（仅前 force_count 项）
		if (renderer.impl->particle_params.force_count > 0)
		{
			uint32_t force_bytes = renderer.impl->particle_params.force_count * static_cast<uint32_t>(PARTICLE_FORCE_STRIDE);
			if (!renderer.compute_buffer_write(renderer.impl->particle_force_buffer, renderer.impl->particle_params.forces, force_bytes))
				return false;
		}
		//绑定：front 状态 / back 状态 / 力场 / 控制缓冲
		if (!renderer.compute_buffer_bind(renderer.impl->particle_buffer_front, 0))
			return false;
		if (!renderer.compute_buffer_bind(renderer.impl->particle_buffer_back, 1))
			return false;
		if (!renderer.compute_buffer_bind(renderer.impl->particle_force_buffer, 2))
			return false;
		if (!renderer.compute_buffer_bind(renderer.impl->particle_control_buffer, 3))
			return false;
		//推送常量（与同步链推进同布局）
		const float push[16] = {
			renderer.impl->particle_params.gravity[0], renderer.impl->particle_params.gravity[1], renderer.impl->particle_params.gravity[2], dt,
			renderer.impl->particle_params.damping, static_cast<float>(capacity),
			static_cast<float>(renderer.impl->particle_params.force_count), 0.0f,
			renderer.impl->particle_params.sphere_center[0], renderer.impl->particle_params.sphere_center[1],
			renderer.impl->particle_params.sphere_center[2], renderer.impl->particle_params.sphere_radius,
			renderer.impl->particle_params.floor_y, renderer.impl->particle_params.restitution, renderer.impl->particle_params.friction,
			static_cast<float>(renderer.impl->particle_params.ground_enabled)
		};
		if (!renderer.compute_dispatch(renderer.impl->particle_resident_step_pipeline, capacity, 64u, push, sizeof(push)))
			return false;
		//交换前后缓冲（下一步读取新结果）
		uint64_t swapped = renderer.impl->particle_buffer_front;
		renderer.impl->particle_buffer_front = renderer.impl->particle_buffer_back;
		renderer.impl->particle_buffer_back = swapped;
		//推进后活跃计数与死亡数
		uint32_t after = 0;
		if (!renderer.compute_buffer_read(renderer.impl->particle_control_buffer, &after, 4u, 4u))
			return false;
		renderer.impl->particle_resident_active = after;
		uint32_t died = before > after ? before - after : 0u;
		renderer.impl->particle_gpu_died_last = died;
		renderer.impl->particle_died_total += died;
		//设备耗时（不支持时间戳为 −1）与主机侧耗时
		renderer.impl->particle_gpu_ms_last = renderer.impl->compute.timestamp_supported()
			? renderer.impl->compute.stats_get().elapsed_ms : -1.0;
		const auto sim_end = std::chrono::high_resolution_clock::now();
		renderer.impl->particle_sim_ms_last = std::chrono::duration<double, std::milli>(sim_end - sim_begin).count();
		return true;
	}

	}
}
