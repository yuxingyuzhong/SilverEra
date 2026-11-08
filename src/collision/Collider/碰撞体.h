#pragma once
//预编译头
#include "Engine/EngineFoundation/common/前置头文件包含.h"
//获取 JSON 解析外部库
#include <nlohmann/json.hpp>
//获取自有几何类型
#include "../Common/几何类型.h"
//获取运动量类型
#include "../Common/运动类型.h"

namespace engine
{
	//向后声明碰撞体后端
	class Collider_Backend;

	//碰撞检测方式
	struct Detection_Mode
	{
		//扫掠检测标记
		bool is_swept_volume = false;
		//离散检测步长
		uint32_t step_length = 0;
	};

	/*
	碰撞体（前端数据）
	只保留编号、运动状态与检测配置；碰撞对象与几何形状全部由 Collider_Backend 持有，
	本结构不再出现任何碰撞库类型。
	*/
	struct Collider
	{
		//编号
		uint64_t ID = 0;
		//碰撞体后端（持有碰撞对象与几何形状）
		std::unique_ptr<Collider_Backend> backend;

		//整体运动量（周期总量）
		Motion motion;
		//部件运动量（周期总量）
		std::vector<Part_Motion> part_motions;
		/*
		运动作用频率
		逻辑帧率的因数，表示一个逻辑帧周期内运动生效的次数；
		为零表示未配置，此时每次检测都按旧行为施加全量运动。
		*/
		uint64_t motion_frequency = 0;
		//运动上次生效的逻辑帧序号
		uint64_t motion_frame = 0;
		/*
		本帧实际施加的运动量
		生效帧为「运动量 ÷ 作用频率」，非生效帧为零运动；
		未配置频率时即等于运动量本身。
		*/
		Motion motion_step;
		//本帧实际施加的部件运动量
		std::vector<Part_Motion> part_motion_steps;
		//检测方式
		Detection_Mode detection_mode;
		//豁免标记
		uint64_t exemption_flag = 0;
		//几何配置（供空间间镜像与转移复用）
		nlohmann::json geometry;

		//构造函数（分配碰撞体后端）
		Collider();
		//析构函数
		~Collider();

		//禁止拷贝（后端持有碰撞对象）
		Collider(const Collider&) = delete;
		Collider& operator=(const Collider&) = delete;

		//移动构造/赋值（后端句柄随之搬移，碰撞对象地址不变）
		Collider(Collider&& other) noexcept;
		Collider& operator=(Collider&& other) noexcept;

		//几何挂载状态
		bool geometry_mounted(void) const;
	};
}
