#pragma once
//预编译头
#include "common/前置头文件包含.h"
//获取日志系统
#include "src/tools/Logging/日志系统.h"

namespace engine
{
	//分配方式
	enum class Allocate_Order
	{
		LIFO,
		FIFO
	};

	//数值池 —— 模板参数 T 指定分配数值类型
	template <typename T = uint64_t>
		requires std::is_integral_v<T>
	class Number_Allocator
	{
	private:
		//数值分配方式
		Allocate_Order order = Allocate_Order::LIFO;
		//可分配新数值
		T next_number = (std::numeric_limits<T>::min)();
		//回收数值集合
		std::deque<T> recycle_numbers;
		//回收数值映射
		std::unordered_set<T> mapping;

	public:
		//设置分配起点
		void set(T min_allocate_number)
		{
			next_number = min_allocate_number;
		}
		//设置分配机制
		void set(Allocate_Order order)
		{
			this->order = order;
		}
		//获取可用数值
		T get(void)
		{
			//待返回数值记录
			T number;
			//若回收数值集合不为空
			if (!recycle_numbers.empty())
			{
				//若为后进先出分配机制
				if (order == Allocate_Order::LIFO)
				{
					//弹出末元素
					number = recycle_numbers.back();
					recycle_numbers.pop_back();
				}
				//若为后进后出分配机制
				else
				{
					//弹出首元素
					number = recycle_numbers.front();
					recycle_numbers.pop_front();
				}
			}
			//若回收数值集合为空
			else
			{
				//若可分配数值耗尽
				if (next_number == (std::numeric_limits<T>::max)())
				{
					Log::warn("Number_Allocator::数值分配殆尽\n已进行回绕分配");
					//获取可分配新数值
					number = next_number = (std::numeric_limits<T>::min)());
				}
				else
				    //获取可分配新数值
				    number = next_number++;
			}

			//清除数值映射
			mapping.erase(number);

			return number;
		}
		//回收数值 —— 单数值重载
		bool recycle(T recycle_number)
		{
			//若待回收数值已回收
			if (mapping.count(recycle_number))
			{
				Log::warn("Number_Pool::待回收数值已被回收!!!");
				return false;
			}
			else
			{
				//回收数值
				recycle_numbers.push_back(recycle_number);
				//建立映射
				mapping.insert(recycle_number);
				return true;
			}
		}
		//回收数值 —— 多数值重载
		void recycle(const std::vector<T>& numbers)
		{
			//循环调用单数值重载
			for (auto number : numbers)
				recycle(number);
		}
		//重置分配器
		void reset(void)
		{
			//重置数值分配策略
			order = Allocate_Order::LIFO;
			//重置可分配新数值
			next_number = (std::numeric_limits<T>::min)();
			//重置回收数值集合
			recycle_numbers.clear();
			//重置回收数值映射
			mapping.clear();
		}
	};
}