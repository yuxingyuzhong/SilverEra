#pragma once
//获取 ECS 运行包
#include "Engine/EngineSystem/src/world/ecs/ECS系统运行包.h"

//ECS 测试组件命名空间
namespace ecs_test
{
	//位置组件
	struct Position : engine::Object
	{
		//归属实体槽位
		uint32_t owner = 0;
		//横坐标
		float x = 0.0f;
		//纵坐标
		float y = 0.0f;

		//默认构造
		Position(void) = default;
		//带参构造
		Position(float px, float py) : x(px), y(py) {}
	};

	//速度组件
	struct Velocity : engine::Object
	{
		//归属实体槽位
		uint32_t owner = 0;
		//横向速度
		float x = 0.0f;
		//纵向速度
		float y = 0.0f;

		//默认构造
		Velocity(void) = default;
		//带参构造
		Velocity(float vx, float vy) : x(vx), y(vy) {}
	};

	//生命组件
	struct Health : engine::Object
	{
		//归属实体槽位
		uint32_t owner = 0;
		//生命值
		int hp = 100;

		//默认构造
		Health(void) = default;
		//带参构造
		explicit Health(int value) : hp(value) {}
	};
}
