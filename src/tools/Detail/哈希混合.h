#pragma once
//预编译头
#include "Engine/EngineCore/common/前置头文件包含.h"

//通用算法模块
namespace engine
{
	//辅助工具命名空间
	namespace detail
	{
		//哈希混合工具
		inline void hash_combine(size_t& seed, size_t val) noexcept 
		{
			seed ^= val + 0x9e3779b9 + (seed << 6) + (seed >> 2);
		}
	}
}
