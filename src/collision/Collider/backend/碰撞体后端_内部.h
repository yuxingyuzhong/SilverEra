#pragma once
//预编译头
#include "Engine/EngineFoundation/common/前置头文件包含.h"
//无序集合（子部件忽略对）
#include <unordered_set>
//获取碰撞库类型（仅后端内部使用，直接使用原始类型名）
#include "btBulletCollisionCommon.h"
//获取变换类型
#include "LinearMath/btTransform.h"
//获取自有几何类型
#include "../../Common/几何类型.h"

namespace engine
{
	//向后声明碰撞空间后端（用于挂载归属的反向指针）
	class Region_Backend;
	//向后声明碰撞体后端实现体
	struct Collider_Backend_Impl;

	/*
	子部件碰撞对象
	每个子部件独占一个碰撞对象，使同一碰撞体内部的子部件能够互相检测（自碰）；
	对象地址由堆分配保证稳定，自指针恒指向本结构。
	*/
	struct Geometry_Part_Object
	{
		//碰撞对象
		btCollisionObject object;
		//所属碰撞体编号
		uint64_t collider_ID = 0;
		//子部件编号（0xFFFF 表示未配置）
		uint16_t part_ID = 0xFFFF;
		//所属碰撞体实现体
		Collider_Backend_Impl* owner = nullptr;

		//构造函数
		Geometry_Part_Object()
		{
			//挂载实现体自指针，供碰撞库回调反查
			object.setUserPointer(this);
		}
	};

	//几何体部件（碰撞体内一个带相对变换的几何体）
	struct Geometry_Part
	{
		/*
		网格接口（网格形状的数据源）
		注意：BVH三角形网格形状只持有网格指针而不接管所有权，
		      故网格数据必须与形状同生共死；此处按成员声明顺序的逆序析构，
		      网格先于形状声明，因此析构时形状先释放、网格后释放。
		*/
		std::unique_ptr<btTriangleMesh> mesh;
		//几何形状（由部件持有，子部件对象只挂载指针）
		std::unique_ptr<btCollisionShape> shape;
		//相对变换（描述该几何体在碰撞体内的空间位置关系）
		Transform local_transform = Transform::getIdentity();
		//子部件编号
		uint16_t part_ID = 0xFFFF;
		//子部件碰撞对象（堆分配，自指针稳定）
		std::unique_ptr<Geometry_Part_Object> object;
	};

	//子部件对称忽略对键（较小编号在高 16 位，较大编号在低 16 位）
	inline uint64_t part_pair_key(const uint16_t part_A, const uint16_t part_B)
	{
		//较小与较大编号
		uint16_t low = (part_A < part_B) ? part_A : part_B;
		uint16_t high = (part_A < part_B) ? part_B : part_A;
		//拼接键
		return (static_cast<uint64_t>(low) << 16) | static_cast<uint64_t>(high);
	}

	//碰撞体后端实现体（堆上分配，地址在移动后端句柄时保持不变）
	struct Collider_Backend_Impl
	{
		//根变换（整体运动作用于根变换，子部件世界变换由根变换与局部变换合成）
		Transform root_transform = Transform::getIdentity();
		//当前部件集合
		std::vector<Geometry_Part> parts;
		//待挂载部件集合（几何构建先落到暂存区，成功后才替换当前几何）
		std::vector<Geometry_Part> pending_parts;
		//待挂载根变换
		Transform pending_transform = Transform::getIdentity();
		//待挂载子部件忽略对
		std::unordered_set<uint64_t> pending_part_ignored;
		//待挂载内容是否有效
		bool pending_valid = false;

		//子部件编号到部件下标的映射
		std::unordered_map<uint16_t, uint32_t> part_index;
		//子部件对称忽略对集合
		std::unordered_set<uint64_t> part_ignored;
		//豁免标记（供宽阶段过滤读取）
		uint64_t exemption_flag = 0;

		//身份标签（碰撞体编号与是否空间边界）
		uint64_t collider_ID = 0;
		//是否空间边界
		bool is_boundary = false;
		//所属碰撞空间后端（空表示未挂载；析构时据此自动摘除）
		Region_Backend* owner = nullptr;
	};

	/*
	碰撞体后端内部访问器
	公共头只 friend 本结构，使外部后端能在不暴露实现体的前提下取得对象与部件；
	本头文件仅允许后端源码包含。
	*/
	struct Collider_Backend_Access
	{
		//实现体获取
		static Collider_Backend_Impl& impl_get(class Collider_Backend& backend);
		//实现体获取（只读）
		static const Collider_Backend_Impl& impl_get(const class Collider_Backend& backend);
		//反查子部件对象（无法反查时返回空指针）
		static Geometry_Part_Object* part_recover(const btCollisionObject* object)
		{
			return static_cast<Geometry_Part_Object*>(object->getUserPointer());
		}
		//碰撞对象获取（返回首个部件对象，空几何时无意义）
		static btCollisionObject& object_get(class Collider_Backend& backend);
		//碰撞对象获取（只读）
		static const btCollisionObject& object_get(const class Collider_Backend& backend);
		//部件集合获取
		static std::vector<Geometry_Part>& parts_get(class Collider_Backend& backend);
	};
}
