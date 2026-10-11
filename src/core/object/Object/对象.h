#pragma once
//预编译头
#include "Engine/EngineCore/common/前置头文件包含.h"

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
        //默认构造
        Object() = default;
        //带参构造
        explicit Object(uint64_t id, bool valid = true) noexcept
            : object_ID(id), is_valid(valid){}
        //拷贝构造
        Object(const Object&) = default;
        //拷贝赋值
        Object& operator=(const Object&) = default;
        //移动构造
        Object(Object&& other) noexcept
            : object_ID(other.object_ID), is_valid(other.is_valid)
        {
            //将源对象置为无效状态
            other.object_ID = 0;
            other.is_valid = false;
        }
        //移动赋值
        Object& operator=(Object&& other) noexcept
        {
            //若非自赋值
            if (this != &other)
            {
                //转移对象ID
                object_ID = other.object_ID;
                //转移合法标记
                is_valid = other.is_valid;
                //将源对象置为无效状态
                other.object_ID = 0;
                other.is_valid = false;
            }
            return *this;
        }
        //析构函数
        virtual ~Object() = default;

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
		bool valid(void) const
		{
			return is_valid;
		}
	};
}