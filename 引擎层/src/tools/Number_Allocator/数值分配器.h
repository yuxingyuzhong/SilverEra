#pragma once
//预编译头
#include "common/前置头文件包含.h"
//获取二分查找算法
#include "src/tools/Auxi_Algorithm/二分查找.h"
//获取日志系统
#include "src/tools/Logging/日志系统.h"

namespace engine
{
	//分配方式
	enum Allocate_Order
	{
		LIFO,
		FIFO
	};

	//数值池
	class Number_Allocator
	{
	private:
		//数值分配方式
		Allocate_Order order = Allocate_Order::LIFO;
		//可分配新数值
		uint64_t next_number = 0;
		//回收数值集合
		std::vector<uint64_t> recycle_numbers;
	public:
		//设置分配起点
		void set(const uint64_t& min_allocate_number)
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
				//若为后进后出分配机制
				if(order == Allocate_Order::LIFO)
				{
					//弹出末元素
					index = recycle_numbers.back();
					recycle_numbers.pop_back();
				}
				//若为后进先出分配机制
				else
				{
					//弹出首元素
					index = recycle_numbers.front();
					recycle_numbers.erase(recycle_numbers.begin());
				}
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
		//数值分配方式设置
		void allocate_order_set(Allocate_Order order = Allocate_Order::LIFO)
		{
			this->order = order;
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