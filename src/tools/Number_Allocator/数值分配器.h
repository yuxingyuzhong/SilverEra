#pragma once
//预编译头
#include "common/前置头文件包含.h"
//获取二分查找算法
#include "src/tools/Auxi_Algorithm/二分查找.h"
//获取日志系统
#include "src/tools/Logging/日志系统.h"

namespace engine
{
	//数值池
	class Number_Allocator
	{
	private:
		//可分配新数值
		uint64_t next_number = 0;
		//回收数值集合
		std::vector<uint64_t> recycle_numbers;
	public:
		//设置分配起点
		uint64_t set(const uint64_t& min_allocate_number)
		{
			next_number = min_allocate_number;
		}
		//获取可用数值
		uint64_t get(void)
		{
			//待返回数值记录
			uint64_t index;
			//若回收数值集合不为空
			if (!recycle_numbers.empty())
			{
				//获取回收数值集合末尾元素
				index = recycle_numbers.back();
				//弹出该元素
				recycle_numbers.pop_back();
			}
			else
				//获取可分配新数值
				index = next_number++;

			return index;
		}
		//回收数值 —— 单数值重载
		bool recycle(const uint64_t& recycle_number)
		{
			//查找待回收数值是否已回收
			int index = binary_search(recycle_numbers,recycle_number,std::ranges::less());
			//若待回收数值已回收
			if(index >= 0)
			{
				Log::warn("Number_Pool::待回收数值已被回收!!!");
				return false;
			}
			else
			{
				//回收数值
				recycle_numbers.push_back(recycle_number);
				//重排序
				std::ranges::sort(recycle_numbers);
				return true;
			}
		}
		//回收数值 —— 多数值重载
		void recycle(const std::vector<uint64_t>& recycle_numbers)
		{
			//循环调用单数值重载
			for (auto number : recycle_numbers)
				recycle(number);
		}
		//重置分配器
		void reset(void)
		{
			//重置可分配新数值
			next_number = 0;
			//重置回收数值集合
			recycle_numbers.clear();
		}
	};
}