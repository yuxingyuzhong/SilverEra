#include "src/core/effect/粒子系统.h"

//std::sqrt
#include <cmath>

//引擎命名空间
namespace engine
{
	/*
	容量设置（钳制到 [1, PARTICLE_CAPACITY_MAX]）
	收缩时丢弃超出容量的粒子；容量变化后回收栈按新容量重建（活跃计数归零）。
	*/
	void Particle_System::capacity_set(uint32_t capacity)
	{
		//钳制下界
		if (capacity == 0)
			capacity = 1;
		//钳制上界
		if (capacity > PARTICLE_CAPACITY_MAX)
			capacity = PARTICLE_CAPACITY_MAX;
		capacity_ = capacity;
		particles_.resize(capacity_);
		alive_.assign(capacity_, 0u);
		//重建空闲栈（升序压栈，弹出顺序为倒序）
		free_indices_.clear();
		free_indices_.reserve(capacity_);
		for (uint32_t index = 0; index < capacity_; ++index)
			free_indices_.push_back(index);
		active_count_ = 0;
	}

	//当前容量
	uint32_t Particle_System::capacity_get(void) const
	{
		return capacity_;
	}

	//当前活跃粒子数
	uint32_t Particle_System::active_count_get(void) const
	{
		return active_count_;
	}

	/*
	分配一个新粒子索引（回收栈优先复用）
	池满返回假；成功时返回池内索引，状态内容保持调用前的值（由调用方填充）。
	*/
	bool Particle_System::spawn(uint32_t& index_out)
	{
		//池满
		if (free_indices_.empty())
			return false;
		index_out = free_indices_.back();
		free_indices_.pop_back();
		alive_[index_out] = 1u;
		++active_count_;
		return true;
	}

	/*
	回收指定索引（LIFO）
	越界或重复回收返回假；成功后该索引立即进入回收栈顶端。
	*/
	bool Particle_System::recycle(uint32_t index)
	{
		//越界
		if (index >= capacity_)
			return false;
		//重复回收
		if (alive_[index] == 0u)
			return false;
		alive_[index] = 0u;
		free_indices_.push_back(index);
		--active_count_;
		return true;
	}

	//清空全部粒子并复位回收栈（保留容量）
	void Particle_System::clear(void)
	{
		alive_.assign(capacity_, 0u);
		free_indices_.clear();
		free_indices_.reserve(capacity_);
		for (uint32_t index = 0; index < capacity_; ++index)
			free_indices_.push_back(index);
		active_count_ = 0;
	}

	/*
	推进一个固定步（CPU 参考）
	顺序固定：速度加重力 → 按步阻尼 → 位置积分 → 年龄累加 → 寿命判定回收；
	与 GPU 推进着色器保持同一运算顺序，便于 1e-5 阈值逐粒子对照。
	*/
	uint32_t Particle_System::step(const Particle_Params& params, float dt)
	{
		uint32_t died = 0;
		for (uint32_t index = 0; index < capacity_; ++index)
		{
			//跳过非活跃索引
			if (alive_[index] == 0u)
				continue;
			Particle_State& state = particles_[index];
			for (uint32_t axis = 0; axis < 3; ++axis)
			{
				state.velocity[axis] += params.gravity[axis] * dt;
			}
			/*
			力场作用（第三百一十八轮；与 GPU 推进同序）
			按配置顺序逐场施加加速度：风场为恒定方向；引力点为半径内线性衰减的径向力；
			涡流为半径内的轴向切向力（半径外 / 退化方向直接跳过）。
			*/
			for (uint32_t field_index = 0; field_index < params.force_count; ++field_index)
			{
				const Particle_Force_Field& field = params.forces[field_index];
				//风场：恒定方向加速度
				if (field.type == static_cast<uint32_t>(Force_Type::Wind))
				{
					float direction_length = std::sqrt(field.direction[0] * field.direction[0]
						+ field.direction[1] * field.direction[1]
						+ field.direction[2] * field.direction[2]);
					//方向退化则跳过
					if (!(direction_length > 1e-6f))
						continue;
					for (uint32_t axis = 0; axis < 3; ++axis)
						state.velocity[axis] += field.direction[axis] / direction_length * field.strength * dt;
					continue;
				}
				//相对位置与距离
				float relative[3] = { state.position[0] - field.position[0],
					state.position[1] - field.position[1], state.position[2] - field.position[2] };
				float distance = std::sqrt(relative[0] * relative[0] + relative[1] * relative[1]
					+ relative[2] * relative[2]);
				//半径外或与力场中心重合则跳过
				if (!(distance > 1e-6f) || !(distance < field.radius))
					continue;
				//线性衰减权重
				float weight = 1.0f - distance / field.radius;
				//引力点：指向中心的径向加速度
				if (field.type == static_cast<uint32_t>(Force_Type::Attractor))
				{
					for (uint32_t axis = 0; axis < 3; ++axis)
						state.velocity[axis] -= relative[axis] / distance * field.strength * weight * dt;
					continue;
				}
				//涡流：绕轴方向的切向加速度
				float axis_length = std::sqrt(field.direction[0] * field.direction[0]
					+ field.direction[1] * field.direction[1]
					+ field.direction[2] * field.direction[2]);
				//轴退化则跳过
				if (!(axis_length > 1e-6f))
					continue;
				float axis_unit[3] = { field.direction[0] / axis_length, field.direction[1] / axis_length,
					field.direction[2] / axis_length };
				float tangent[3] = {
					axis_unit[1] * relative[2] - axis_unit[2] * relative[1],
					axis_unit[2] * relative[0] - axis_unit[0] * relative[2],
					axis_unit[0] * relative[1] - axis_unit[1] * relative[0] };
				float tangent_length = std::sqrt(tangent[0] * tangent[0] + tangent[1] * tangent[1]
					+ tangent[2] * tangent[2]);
				//切向退化则跳过
				if (!(tangent_length > 1e-6f))
					continue;
				for (uint32_t axis = 0; axis < 3; ++axis)
					state.velocity[axis] += tangent[axis] / tangent_length * field.strength * weight * dt;
			}
			for (uint32_t axis = 0; axis < 3; ++axis)
			{
				state.velocity[axis] *= params.damping;
				state.position[axis] += state.velocity[axis] * dt;
			}
			/*
			碰撞（第三百一十九轮；与 GPU 同序）
			先地面平面（位置钳到地面、法向反弹、切向摩擦），再球体（进入球面推出、法向反弹、切向摩擦）。
			*/
			if (params.ground_enabled != 0u && state.position[1] < params.floor_y)
			{
				state.position[1] = params.floor_y;
				if (state.velocity[1] < 0.0f)
					state.velocity[1] = -state.velocity[1] * params.restitution;
				state.velocity[0] *= 1.0f - params.friction;
				state.velocity[2] *= 1.0f - params.friction;
			}
			if (params.sphere_radius > 0.0f)
			{
				float delta[3] = { state.position[0] - params.sphere_center[0],
					state.position[1] - params.sphere_center[1],
					state.position[2] - params.sphere_center[2] };
				float distance = std::sqrt(delta[0] * delta[0] + delta[1] * delta[1] + delta[2] * delta[2]);
				//进入球面（含球心与球面重合的极少数退化）
				if (distance > 1e-6f && distance < params.sphere_radius)
				{
					//法向单位向量
					float normal[3] = { delta[0] / distance, delta[1] / distance, delta[2] / distance };
					//推出到球面
					for (uint32_t axis = 0; axis < 3; ++axis)
						state.position[axis] = params.sphere_center[axis] + normal[axis] * params.sphere_radius;
					float normal_speed = state.velocity[0] * normal[0] + state.velocity[1] * normal[1]
						+ state.velocity[2] * normal[2];
					//仅处理进入球面（法向速度为负）
					if (normal_speed < 0.0f)
					{
						//切向分量（摩擦衰减）＋ 法向回弹
						float normal_after = -normal_speed * params.restitution;
						for (uint32_t axis = 0; axis < 3; ++axis)
						{
							float tangent = state.velocity[axis] - normal_speed * normal[axis];
							state.velocity[axis] = tangent * (1.0f - params.friction) + normal[axis] * normal_after;
						}
					}
				}
			}
			state.age += dt;
			//寿命到期回收
			if (state.age >= state.lifetime)
			{
				recycle(index);
				++died;
			}
		}
		return died;
	}

	/*
	发射（CPU 参考；确定性 LCG）
	位置按形状采样；速度方向 = normalize(direction ＋ 0.3·随机单位向量)，大小在 [speed_min, speed_max]；
	寿命在 [lifetime_min, lifetime_max]；池满即停止并返回实际发射数。
	*/
	uint32_t Particle_System::emit(const Particle_Emitter& emitter, uint32_t budget)
	{
		//发射器非法
		if (!particle_emitter_valid(emitter))
			return 0;
		//本帧发射上限
		const uint32_t limit = emitter.count < budget ? emitter.count : budget;
		//单位随机数（[0, 1)；LCG 与 GPU 端一致）
		auto next_unit = [this](void) -> float
		{
			rng_state_ = rng_state_ * 1664525u + 1013904223u;
			return static_cast<float>((rng_state_ >> 8) & 0x00FFFFFFu) / 16777216.0f;
		};
		//[-1, 1] 随机分量
		auto next_signed = [&next_unit](void) -> float
		{
			return next_unit() * 2.0f - 1.0f;
		};
		//主方向归一化（发射器校验已保证长度非零）
		const float direction_length = std::sqrt(emitter.direction[0] * emitter.direction[0]
			+ emitter.direction[1] * emitter.direction[1]
			+ emitter.direction[2] * emitter.direction[2]);
		const float direction_unit[3] = { emitter.direction[0] / direction_length,
			emitter.direction[1] / direction_length, emitter.direction[2] / direction_length };

		uint32_t spawned = 0;
		for (; spawned < limit; ++spawned)
		{
			uint32_t index = 0;
			//池满
			if (!spawn(index))
				break;
			Particle_State& state = particles_[index];
			//发射位置
			for (uint32_t axis = 0; axis < 3; ++axis)
				state.position[axis] = emitter.position[axis];
			if (emitter.shape == static_cast<uint32_t>(Emit_Shape::Box))
			{
				for (uint32_t axis = 0; axis < 3; ++axis)
					state.position[axis] += next_signed() * emitter.extents[axis];
			}
			else if (emitter.shape == static_cast<uint32_t>(Emit_Shape::Sphere_Shell))
			{
				//球壳方向（长度过小则退化为 +Y）
				float shell[3] = { next_signed(), next_signed(), next_signed() };
				float shell_length = std::sqrt(shell[0] * shell[0] + shell[1] * shell[1] + shell[2] * shell[2]);
				if (!(shell_length > 1e-6f))
				{
					shell[0] = 0.0f;
					shell[1] = 1.0f;
					shell[2] = 0.0f;
					shell_length = 1.0f;
				}
				for (uint32_t axis = 0; axis < 3; ++axis)
					state.position[axis] += shell[axis] / shell_length * emitter.extents[0];
			}
			//速度方向（主方向 ＋ 0.3 随机扰动）
			float velocity[3] = { direction_unit[0] + next_signed() * 0.3f,
				direction_unit[1] + next_signed() * 0.3f,
				direction_unit[2] + next_signed() * 0.3f };
			float velocity_length = std::sqrt(velocity[0] * velocity[0] + velocity[1] * velocity[1]
				+ velocity[2] * velocity[2]);
			if (!(velocity_length > 1e-6f))
			{
				velocity[0] = direction_unit[0];
				velocity[1] = direction_unit[1];
				velocity[2] = direction_unit[2];
				velocity_length = 1.0f;
			}
			const float speed = emitter.speed_min + next_unit() * (emitter.speed_max - emitter.speed_min);
			for (uint32_t axis = 0; axis < 3; ++axis)
				state.velocity[axis] = velocity[axis] / velocity_length * speed;
			//年龄与寿命
			state.age = 0.0f;
			state.lifetime = emitter.lifetime_min
				+ next_unit() * (emitter.lifetime_max - emitter.lifetime_min);
		}
		return spawned;
	}

	//重置随机种子（确定性对照用）
	void Particle_System::seed_set(uint32_t seed)
	{
		rng_state_ = seed;
	}

	//粒子状态数组（长度 = 容量）
	Particle_State* Particle_System::data_get(void)
	{
		return particles_.data();
	}

	const Particle_State* Particle_System::data_get(void) const
	{
		return particles_.data();
	}

	//逐索引活跃标记（长度 = 容量）
	uint8_t* Particle_System::alive_get(void)
	{
		return alive_.data();
	}

	const uint8_t* Particle_System::alive_get(void) const
	{
		return alive_.data();
	}
}
