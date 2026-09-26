#pragma once
//预编译头
#include "common/前置头文件包含.h"
//获取预定义对象类型
#include "src/core/object/Object/对象.h"

namespace engine
{
	//属性
	class Prop : public Object
	{
	private:
		//通用属性槽
		std::unordered_map<std::string, double> property_slot;
	public:
		//属性槽获取
		std::unordered_map<std::string, double>& prop_get(void)
		{
			return property_slot;
		}
	};

}
