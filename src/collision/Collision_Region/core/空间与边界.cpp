#include "../局部命名空间使用.h"
//获取日志系统
#include "Engine/EngineFoundation/src/log/日志系统运行包.h"

namespace engine
{
	//默认构造
	Collision_Region::Collision_Region() : Collision_Region(string())
	{
	}

	//含参构造(指定碰撞空间名称)
	Collision_Region::Collision_Region(const string& region_name)
	{
		//记录碰撞空间名称
		name = region_name;
		//预分配空间边界编号（编号0恒保留给空间边界）
		ID_allocator.get();
	}

	//析构函数
	Collision_Region::~Collision_Region() = default;

	//碰撞空间有效性检查
	bool Collision_Region::valid(void) const
	{
		return backend.valid();
	}

	//碰撞空间状态设置
	void Collision_Region::state_set(bool active)
	{
		//停用时清空跨越记录与待发通知（重新激活后重新建立基准）
		if (!active)
		{
			cross_states.clear();
			cross_dirty.clear();
			cross_notices.clear();
		}
		//写入空间活跃性
		is_active = active;
	}

	//碰撞空间状态查看
	bool Collision_Region::state_check() const
	{
		return is_active;
	}

	//运动事件读取回调注入(由碰撞代理器注入)
	void Collision_Region::motion_reader_set(std::function<bool(uint64_t, Motion_Event&)> reader)
	{
		motion_reader = std::move(reader);
	}

	//边界构建
	bool Collision_Region::boundary_build(const string& mesh_path)
	{
		//空间有效性检查
		if (!valid())
		{
			logger.warn("Collision_Region::碰撞空间后端不可用，空间边界无法构建");
			return false;
		}

		//交由后端构建边界
		if (!backend.boundary_build(mesh_path))
			return false;

		//边界更换后清空跨越记录与待发通知（重新建立基准，不产生跨越通知）
		cross_states.clear();
		cross_dirty.clear();
		cross_notices.clear();
		return true;
	}

	//边界卸载
	void Collision_Region::boundary_unload(void)
	{
		//交由后端卸载边界
		backend.boundary_unload();
	}
}