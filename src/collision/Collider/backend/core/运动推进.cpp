#include "../../局部命名空间使用.h"
//获取碰撞体后端声明
#include "src/collision/Collider/backend/碰撞体后端.h"
//获取碰撞体后端实现体与访问器
#include "src/collision/Collider/backend/碰撞体后端_内部.h"
//获取几何类型转换
#include "src/collision/Common/数学转换.h"
//获取日志系统
#include "Engine/EngineFoundation/src/log/日志系统运行包.h"

namespace engine
{
	//实现体定义（沿用内部头的通用实现体，供公开句柄持有）
	struct Collider_Backend::Impl : Collider_Backend_Impl
	{
	};

	//文件内部辅助设施（不对外暴露）
	namespace
	{
		//单个子部件世界变换刷新
		void part_refresh(const Collider_Backend_Impl& impl, Geometry_Part& part)
		{
			//无对象或无形状的部件跳过
			if (!part.object || !part.shape)
				return;
			//世界变换（根变换与局部变换合成）
			Transform world = impl.root_transform * part.local_transform;
			//写入世界变换
			part.object->object.setWorldTransform(conversion::to_bt(world));
		}
	}

	//整体运动推进（绕自身原点旋转后按世界轴平移）
	bool Collider_Backend::motion_advance(const Motion& motion)
	{
		//实现体或几何缺失时无法推进
		if (!impl || !geometry_mounted())
			return false;

		//绕自身原点旋转（增量旋转位于物体局部轴）
		impl->root_transform.setRotation(impl->root_transform.getRotation() * motion.rotation);
		//按世界轴平移
		impl->root_transform.setOrigin(impl->root_transform.getOrigin() + motion.translation);
		//逐个刷新子部件世界变换
		for (Geometry_Part& part : impl->parts)
			part_refresh(*impl, part);
		return true;
	}

	//部件运动推进（在碰撞体局部空间内绕部件原点旋转后按局部轴平移）
	bool Collider_Backend::part_motion_advance(uint16_t part_ID, const Motion& motion)
	{
		//实现体或几何缺失时无法推进
		if (!impl || !geometry_mounted())
			return false;

		//子部件必须存在
		auto it = impl->part_index.find(part_ID);
		if (it == impl->part_index.end())
		{
			logger.warn("Collider_Backend::子部件编号({})不存在", part_ID);
			return false;
		}

		//单部件时部件运动退化为整体运动（整体运动作用于根变换）
		if (impl->parts.size() == 1)
			return motion_advance(motion);

		//目标部件
		Geometry_Part& target = impl->parts[it->second];
		//更新局部变换：绕部件原点旋转后按局部轴平移
		target.local_transform.setRotation(
			target.local_transform.getRotation() * motion.rotation);
		target.local_transform.setOrigin(
			target.local_transform.getOrigin() + motion.translation);
		//刷新目标子部件世界变换
		part_refresh(*impl, target);
		return true;
	}
}
