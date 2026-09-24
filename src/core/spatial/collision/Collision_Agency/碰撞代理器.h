#pragma once
//预编译头
#include "common/前置头文件包含.h"
//获取预定义坐标类型
#include "../../common/坐标类型.h"
//获取预定义几何体类型
#include "../../common/几何体类型.h"
//获取事件系统运行包
#include "src/core/event/事件系统运行包.h"
//获取引擎环境
#include "src/tools/Engine_Env/引擎环境.h"
//获取路径字符串转化方法
#include "src/tools/Auxi_Algorithm/路径字符串转换.h"

namespace engine
{
	//碰撞代理器
	class Collision_Agent
	{
		//碰撞空间
		struct collision_space
		{
			//碰撞空间名称
			std::string space_name;
			//碰撞空间编号
			uint64_t ID;
			//碰撞空间几何体
			Geometry geometry;
			//碰撞空间活跃性
			bool is_active = false;
		};
	};
}