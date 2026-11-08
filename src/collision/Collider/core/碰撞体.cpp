#include "../局部命名空间使用.h"
//获取碰撞体后端声明
#include "src/collision/Collider/backend/碰撞体后端.h"

namespace engine
{
	//构造函数（分配碰撞体后端）
	Collider::Collider()
	{
		//分配后端内存（失败时保留空句柄，由调用方按未挂载处理）
		backend.reset(new(nothrow) Collider_Backend());
	}

	//析构函数
	Collider::~Collider() = default;

	//移动构造
	Collider::Collider(Collider&& other) noexcept
	{
		//搬移编号与数据成员
		ID = other.ID;
		backend = std::move(other.backend);
		motion = other.motion;
		part_motions = std::move(other.part_motions);
		motion_frequency = other.motion_frequency;
		motion_frame = other.motion_frame;
		motion_step = other.motion_step;
		part_motion_steps = std::move(other.part_motion_steps);
		detection_mode = other.detection_mode;
		exemption_flag = other.exemption_flag;
		geometry = std::move(other.geometry);
	}

	//移动赋值
	Collider& Collider::operator=(Collider&& other) noexcept
	{
		//若为自身赋值则直接返回
		if (this == &other)
			return *this;

		//搬移编号与数据成员
		ID = other.ID;
		backend = std::move(other.backend);
		motion = other.motion;
		part_motions = std::move(other.part_motions);
		motion_frequency = other.motion_frequency;
		motion_frame = other.motion_frame;
		motion_step = other.motion_step;
		part_motion_steps = std::move(other.part_motion_steps);
		detection_mode = other.detection_mode;
		exemption_flag = other.exemption_flag;
		geometry = std::move(other.geometry);

		return *this;
	}

	//几何挂载状态
	bool Collider::geometry_mounted(void) const
	{
		return backend && backend->geometry_mounted();
	}
}