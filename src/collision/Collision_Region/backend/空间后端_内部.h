#pragma once
//预编译头
#include "Engine/EngineFoundation/common/前置头文件包含.h"
//获取碰撞库类型（仅后端内部使用，直接使用原始类型名）
#include "btBulletCollisionCommon.h"
//获取持久流形（碰撞检测的接触点容器）
#include "BulletCollision/NarrowPhaseCollision/btPersistentManifold.h"
//获取变换类型
#include "LinearMath/btTransform.h"
//获取碰撞体后端声明
#include "../../Collider/backend/碰撞体后端.h"
//获取碰撞体后端实现体（宽阶段过滤需要子部件身份）
#include "../../Collider/backend/碰撞体后端_内部.h"

namespace engine
{
	/*
	宽阶段重叠过滤
	在窄阶段之前完成豁免判定：跨碰撞体按同组对称互免，同一碰撞体按对称子部件忽略对；
	被过滤的配对不进入窄阶段，豁免真正省算力。
	*/
	class Region_Overlap_Filter : public btOverlapFilterCallback
	{
	public:
		//重叠配对判定（返回 false 表示不进入窄阶段）
		bool needBroadphaseCollision(btBroadphaseProxy* proxy0, btBroadphaseProxy* proxy1) const override
		{
			//保留内置过滤组与掩码检查
			if ((proxy0->m_collisionFilterGroup & proxy1->m_collisionFilterMask) == 0 ||
				(proxy1->m_collisionFilterGroup & proxy0->m_collisionFilterMask) == 0)
				return false;

			//客户端对象反查子部件身份
			const btCollisionObject* object_A =
				static_cast<const btCollisionObject*>(proxy0->m_clientObject);
			const btCollisionObject* object_B =
				static_cast<const btCollisionObject*>(proxy1->m_clientObject);
			Geometry_Part_Object* part_A = Collider_Backend_Access::part_recover(object_A);
			Geometry_Part_Object* part_B = Collider_Backend_Access::part_recover(object_B);
			//无法反查身份的对象（空间边界等）不参与豁免
			if (!part_A || !part_B || !part_A->owner || !part_B->owner)
				return true;

			//同一碰撞体：按对称子部件忽略对判定
			if (part_A->owner == part_B->owner)
				return part_A->owner->part_ignored.count(
					part_pair_key(part_A->part_ID, part_B->part_ID)) == 0;

			//不同碰撞体：同组非零即互免
			if (part_A->owner->exemption_flag != 0 &&
				part_A->owner->exemption_flag == part_B->owner->exemption_flag)
				return false;

			return true;
		}
	};

	//碰撞空间后端实现体
	struct Region_Backend_Impl
	{
		//配置
		std::shared_ptr<btDefaultCollisionConfiguration> config;
		//窄阶段调度器
		std::unique_ptr<btCollisionDispatcher> dispatcher;
		//宽阶段接口
		std::unique_ptr<btBroadphaseInterface> broadphase;
		//碰撞世界
		std::unique_ptr<btCollisionWorld> world;
		//宽阶段重叠过滤（豁免判定）
		Region_Overlap_Filter overlap_filter;

		//空间边界（复用碰撞体后端持有对象与网格形状）
		Collider_Backend boundary;
		//已挂载对象集合（析构时统一摘除，避免世界析构解引用悬空对象）
		std::set<Collider_Backend*> attached;
	};
}
