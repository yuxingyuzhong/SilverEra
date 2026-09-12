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
		//ID绑定
		void ID_bind(const uint64_t& ID)
		{
			//设置对象ID
			this->object_ID = ID;
		}
		//ID信息获取
		uint64_t ID(void) const
		{
			return this->object_ID;
		}
		//对象有效性设置
		void valid_set(bool valid)
		{
			this->is_valid = valid;
		}
		//对象有效性获取
		bool valid(void) 
		{
			return is_valid;
		}
	};

    //属性槽
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