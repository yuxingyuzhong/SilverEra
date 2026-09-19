#pragma once
//预编译头
#include "common/前置头文件包含.h"
//获取预定义对象类型
#include "common/types/对象类型.h"
//获取数值分配器
#include "src/tools/Non_GUI/Number_Allocator/数值分配器.h"
//获取二分查找算法
#include "src/tools/Non_GUI/Auxi_Algorithm/二分查找.h"

namespace engine
{
	//对象池 —— 模板需从对象中派生
	template <typename T, typename = std::enable_if_t<std::is_base_of<Object, T>::value>>
	class Object_Pool
	{
	private:
		//ID分配器
		Number_Allocator ID_allocator;
		//索引分配器
		Number_Allocator index_allocator;

		//连续存储对象集合
		std::unique_ptr<std::vector<T>> objects;
		//对象索引映射
		std::unique_ptr<std::unordered_map<uint64_t, uint64_t>> object_index_map;
		//对象池排列易变性标志
		bool is_volatile = false;

	public:
		//对象查找 —— 连续存储重载
		template <typename Compare = std::ranges::less>
		typename std::vector<T>::iterator find(const uint64_t& ID, Compare comp = {})
		{
			//若对象池序列稳定
			if (!is_volatile)
			{
				//获取索引映射迭代器
				auto it = object_index_map->find(ID);
				//若迭代器有效
				if (it != object_index_map->end())
					return objects->begin() + it->second;
				//若不存在目标对象则返回超尾迭代器
				else
					return objects->end();
			}
			else
			{
				//获取目标对象索引
				int index = binary_search(*objects, ID, comp, [](const T& object) {return object.ID();});
				//若返回索引有效
				if (index >= 0)
					return objects->begin() + index;
				//若不存在目标对象则返回超尾迭代器
				else
					return objects->end();
			}
		}
		//对象添加
		uint64_t push(void)
		{
			//分配新对象索引
			uint64_t index = index_allocator.get();
			//若索引越界
			if (index >= objects->size())
				objects->push_back({});

			//获取新对象
			auto& new_object = (*objects)[index];
			//重置该对象避免数据残留
			new_object = T{};
			//分配对象ID
			new_object.ID_bind(ID_allocator.get());
			//设置记录有效
			new_object.valid_set(true);
			//若序列稳定则记录索引映射
			if(!is_volatile)
			    object_index_map->insert({ new_object.ID(), index});

			//返回对象ID
			return new_object.ID();
		}
		//对象卸载
		void erase(const uint64_t ID)
		{
			//获取目标对象迭代器
			auto it = object_index_map->find(ID);
			//若目标对象不存在
			if (it == object_index_map->end())
			{
				Log::warn("Object_Pool::编号{}对象不存在", ID);
				return;
			}

			//获取目标对象索引
			uint64_t index = it->second;
			//回收目标对象ID
			ID_allocator.recycle(ID);
			//回收目标对象索引
			index_allocator.recycle(index);
			//设置目标对象记录不合法
			(*objects)[index].valid_set(false);
			//取消目标对象索引映射
			object_index_map->erase(ID);
		}
		//对象卸载 —— 多对象重载
		void erase(const std::vector<uint64_t>& IDs)
		{
			//调用单对象卸载重载
			for (const auto& ID : IDs)
				erase(ID);
		}
		//对象清除
		void clear(void)
		{
			//清空所有对象
			objects->clear();
			//清空所有ID记录
			ID_allocator.reset();      
			//清空所有索引记录
			index_allocator.reset();
			//清空所有索引映射
			object_index_map->clear();
		}
		//全部对象获取
		std::vector<T>& data(void)
		{
			//返回全部对象
			return *objects;
		}
		//超尾迭代器获取
		typename std::vector<T>::iterator end(void)
		{
			return objects->end();
		}
	};
}