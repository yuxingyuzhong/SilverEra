#pragma once
//预编译头
#include "common/前置头文件包含.h"

namespace engine
{
	//对象
	class Object
	{
	protected:
		//对象ID
		uint64_t object_ID = 0;
		//合法标记
		bool is_valid = false;
	public:
		//ID设置
		void ID_set(const uint64_t& ID)
		{
			//设置对象ID
			this->object_ID = ID;
		}
		//对象有效性设置
		void valid_set(bool valid)
		{
			this->is_valid = valid;
		}
		//ID获取
		uint64_t ID(void) const
		{
			return this->object_ID;
		}
		//对象有效性获取
		bool valid(void) 
		{
			return is_valid;
		}
	};


}