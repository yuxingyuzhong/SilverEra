#pragma once
//预编译头
#include "common/前置头文件包含.h"

namespace engine
{
	//对象记录
	struct object_record
	{
		//对象ID
		uint64_t ID = 0;
		//记录合法标记
		bool valid = false;
	};

    //属性槽记录
    struct prop_record : public object_record
    {
        //通用属性槽
        std::unordered_map<std::string, double> property_slot;
    };

}