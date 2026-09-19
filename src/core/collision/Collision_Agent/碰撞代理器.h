#pragma once
//预编译头
#include "common/前置头文件包含.h"
//获取预定义坐标类型
#include "common/types/坐标类型.h"
//获取几何体类型
#include "common/types/几何体类型.h"
//获取预定义事件类型
#include "common/types/事件类型.h"
//获取事件终端
#include "src/core/event/Event_Terminal/事件终端.h"
//获取引擎环境
#include "src/tools/Non_GUI/Engine_Env/引擎环境.h"
//获取路径字符串转化方法
#include "src/tools/Non_Gui/Auxi_Algorithm/路径字符串转换.h"

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
		};
	};
}