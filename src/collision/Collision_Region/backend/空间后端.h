#pragma once
//预编译头
#include "Engine/EngineFoundation/common/前置头文件包含.h"
//获取自有几何类型
#include "../../Common/几何类型.h"
//获取运动量类型
#include "../../Common/运动类型.h"
//获取碰撞数据类型
#include "../../Common/碰撞数据类型.h"

namespace engine
{
	//向后声明碰撞体后端
	class Collider_Backend;

	//分段描述（由前端构造，后端据其内部循环推进）
	struct Collider_Move
	{
		//碰撞体编号（用于输出碰撞对）
		uint64_t ID = 0;
		//碰撞体后端句柄
		Collider_Backend* backend = nullptr;
		//本帧整体运动
		Motion body_step;
		//本帧部件运动
		std::vector<Part_Motion> part_steps;
		//分段段数（零视作一段）
		uint32_t step_count = 0;
	};

	/*
	碰撞空间后端
	持有碰撞世界、空间边界与检测能力；分段循环在本类内部完成，
	前端只组织"哪些碰撞体本帧移动、单步位移与总步数"，不接触任何子弹库概念。
	*/
	class Region_Backend
	{
	public:
		//构造函数
		Region_Backend();
		//析构函数（先摘除全部已挂载对象，再释放碰撞世界）
		~Region_Backend();

		//禁止拷贝
		Region_Backend(const Region_Backend&) = delete;
		Region_Backend& operator=(const Region_Backend&) = delete;
		//移动构造/赋值（实现体地址不变）
		Region_Backend(Region_Backend&& other) noexcept;
		Region_Backend& operator=(Region_Backend&& other) noexcept;

		//空间有效性检查
		bool valid(void) const;

		//空间边界构建
		bool boundary_build(const std::string& mesh_path);
		//空间边界卸载
		void boundary_unload(void);
		//空间边界挂载状态
		bool boundary_mounted(void) const;

		//对象挂载（登记所属空间并加入碰撞世界）
		void object_attach(Collider_Backend& backend);
		//对象摘除（移出碰撞世界并清空所属空间）
		void object_detach(Collider_Backend& backend);
		//刷新世界包围盒
		void aabbs_refresh(void);
		//对象是否已进入碰撞世界
		bool object_contains(const Collider_Backend& backend) const;

		//扫掠检测收集（后端内部按段推进，输出碰撞详情与边界穿越编号）
		void swept_collect(const std::vector<Collider_Move>& moves,
			std::vector<Collision_Detail>& details,
			std::vector<uint64_t>& boundary_contacts) const;
		//离散检测执行（后端内部逐段平移与求交，输出碰撞详情与边界接触编号）
		void discrete_run(const std::vector<Collider_Move>& moves,
			std::vector<Collision_Detail>& details, std::vector<uint64_t>& boundary_contacts);
		//空间边界包含性判定（射线奇偶）
		bool point_inside(const Vector3& point) const;
	private:
		//实现体
		struct Impl;
		std::unique_ptr<Impl> impl;
	};
}
