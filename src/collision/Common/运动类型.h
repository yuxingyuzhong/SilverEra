#pragma once
//预编译头
#include "Engine/EngineFoundation/common/前置头文件包含.h"
//获取自有几何类型
#include "几何类型.h"

namespace engine
{
	/*
	单通道运动量
	表示一个运动周期内的总量（平移 + 旋转），由碰撞空间在生效帧按频率拆分；
	默认构造为零平移 + 单位旋转。
	*/
	struct Motion
	{
		//平移（整体运动按世界轴，部件运动按碰撞体局部轴）
		Vector3 translation{ 0.0f,0.0f,0.0f };
		//旋转增量（绕自身原点）
		Quaternion rotation{};

		//空运动判定
		bool empty(void) const
		{
			return translation.length2() <= 0.0f && rotation.identity_check();
		}

		//按因数拆分
		Motion operator/(Scalar factor) const
		{
			//除数为零时返回空运动
			if (factor == 0.0f)
				return Motion();

			//拆分结果
			Motion result;
			//平移按因数均分
			result.translation = translation / factor;
			//旋转按轴角均分
			Vector3 axis;
			Scalar angle = 0.0f;
			if (rotation.axis_angle_get(axis, angle))
				result.rotation = Quaternion::from_axis_angle(axis, angle / factor);
			return result;
		}
	};

	//部件运动量（part 为子部件编号）
	struct Part_Motion
	{
		//子部件编号
		uint16_t part = 0;
		//该部件的运动量
		Motion motion;
	};

	//一次运动事件（整体运动 + 部件运动）
	struct Motion_Event
	{
		//整体运动
		Motion body;
		//部件运动集合
		std::vector<Part_Motion> parts;
	};
}
