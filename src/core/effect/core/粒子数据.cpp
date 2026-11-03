#include "src/core/effect/粒子数据.h"

//std::isfinite
#include <cmath>

//引擎命名空间
namespace engine
{
	namespace
	{
		//浮点是否有限
		bool finite(float value)
		{
			return std::isfinite(value);
		}

		//浮点是否落在 [0, 1]
		bool unit_interval(float value)
		{
			return finite(value) && value >= 0.0f && value <= 1.0f;
		}
	}

	/*
	发射器参数是否合法
	形状 / 数量 / 速度区间 / 寿命区间 / 半尺寸 / 方向长度 / 全部字段有限性；
	任一不满足即返回假（调用方拒绝本次配置并保持既有发射器）。
	*/
	bool particle_emitter_valid(const Particle_Emitter& emitter)
	{
		//形状越界（只允许 点 / 盒 / 球壳）
		if (emitter.shape > static_cast<uint32_t>(Emit_Shape::Sphere_Shell))
			return false;
		//发射数量
		if (emitter.count == 0 || emitter.count > PARTICLE_EMIT_COUNT_MAX)
			return false;
		//速度区间（min 非负、max ≥ min）
		if (!finite(emitter.speed_min) || emitter.speed_min < 0.0f)
			return false;
		if (!finite(emitter.speed_max) || emitter.speed_max < emitter.speed_min)
			return false;
		//寿命区间（min 正、max ≥ min）
		if (!finite(emitter.lifetime_min) || emitter.lifetime_min <= 0.0f)
			return false;
		if (!finite(emitter.lifetime_max) || emitter.lifetime_max < emitter.lifetime_min)
			return false;
		//位置 / 半尺寸 / 方向逐分量
		for (uint32_t axis = 0; axis < 3; ++axis)
		{
			if (!finite(emitter.position[axis]) || !finite(emitter.extents[axis]) || !finite(emitter.direction[axis]))
				return false;
			//半尺寸非负
			if (emitter.extents[axis] < 0.0f)
				return false;
		}
		//方向长度非零（点发射亦要求方向合法，便于统一归一化）
		const float length_squared = emitter.direction[0] * emitter.direction[0]
			+ emitter.direction[1] * emitter.direction[1]
			+ emitter.direction[2] * emitter.direction[2];
		return length_squared > 1e-12f;
	}

	/*
	全局模拟参数是否合法
	重力 / 地面高度有限；阻尼 / 反弹 / 摩擦落在 [0, 1]；力场数量与各场类型 / 数值合法；
	颜色覆盖启用时两端颜色各通道落在 [0, 1]；软边宽度落在 [0, 1]。
	*/
	bool particle_params_valid(const Particle_Params& params)
	{
		for (uint32_t axis = 0; axis < 3; ++axis)
			if (!finite(params.gravity[axis]))
				return false;
		if (!finite(params.floor_y))
			return false;
		if (!unit_interval(params.damping))
			return false;
		if (!unit_interval(params.restitution))
			return false;
		if (!unit_interval(params.friction))
			return false;
		//碰撞球：球心有限、半径非负
		for (uint32_t axis = 0; axis < 3; ++axis)
			if (!finite(params.sphere_center[axis]))
				return false;
		if (!finite(params.sphere_radius) || params.sphere_radius < 0.0f)
			return false;
		//颜色覆盖：启用时两端颜色各通道须落在 [0, 1]
		if (params.color_enabled != 0u)
			for (uint32_t axis = 0; axis < 3; ++axis)
				if (!unit_interval(params.color_start[axis]) || !unit_interval(params.color_end[axis]))
					return false;
		//软边宽度：落在 [0, 1]
		if (!unit_interval(params.edge_softness))
			return false;
		//软粒子深度档位：落在 [0, 1]（0 = 关闭）
		if (!unit_interval(params.soft_depth_range))
			return false;
		//力场数量越界
		if (params.force_count > PARTICLE_FORCE_MAX)
			return false;
		for (uint32_t index = 0; index < params.force_count; ++index)
		{
			const Particle_Force_Field& field = params.forces[index];
			//类型越界
			if (field.type > static_cast<uint32_t>(Force_Type::Wind))
				return false;
			//数值有限、半径非负
			for (uint32_t axis = 0; axis < 3; ++axis)
			{
				if (!finite(field.position[axis]) || !finite(field.direction[axis]))
					return false;
			}
			if (!finite(field.radius) || field.radius < 0.0f)
				return false;
			if (!finite(field.strength))
				return false;
			//涡流 / 风场方向长度非零（引力点忽略方向）
			if (field.type != static_cast<uint32_t>(Force_Type::Attractor))
			{
				const float length_squared = field.direction[0] * field.direction[0]
					+ field.direction[1] * field.direction[1]
					+ field.direction[2] * field.direction[2];
				if (!(length_squared > 1e-12f))
					return false;
			}
		}
		return true;
	}
}
