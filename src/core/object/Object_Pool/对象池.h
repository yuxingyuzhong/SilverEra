#pragma once
//预编译头
#include "common/前置头文件包含.h"
//获取预定义对象类型
#include "../Object/对象.h"
//获取数值分配器
#include "src/tools/Number_Allocator/数值分配器.h"
//获取二分查找算法
#include "src/tools/Auxi_Algorithm/二分查找.h"

namespace engine
{
	//对象池 —— 模板需从对象中派生
	template <typename T, typename Key = uint64_t>
		requires std::is_base_of_v<Object, T>
	class Object_Pool
	{
	private:
		//ID分配器
		Number_Allocator ID_allocator;
		//索引分配器
		Number_Allocator index_allocator;

		//对象集合
		std::vector<T> objects;
		//模板类型标记
		constexpr bool is_key_integral = std::is_integral_v<Key>;
		//对象池排序标记
		bool is_sorted = false;
		//对象池管理信息
		union
		{
			//对象索引映射
			std::unordered_map<uint64_t, uint64_t> object_index_map;
			//排序定位信息
			struct
			{
				//投影字段
				std::function<Key(const T&)> projector;  
				//比较方式(默认降序)
				bool is_greater = false;
				//有效索引起点
				std::optional<uint64_t> min_valid_index{};
			};
		};

	public:
		//构造函数
		Object_Pool()
		{
			//默认构造稳定模式
			new (&object_index_map) std::unordered_map<uint64_t, uint64_t>();
			//为非法实体预留ID
			ID_allocator.set(1);
		}
		//析构函数
		~Object_Pool()
		{
			//若序列不稳定则析构投影字段
			if (is_sorted)
				projector.~function();
			else
				object_index_map.~unordered_map();
		}
		//对象排列方式设置
		template <typename Projection>
		void sort_order_set(bool greater, Projection proj)
		{
			//若当前未记录排序方式
			if(!is_sorted)
			{
				//设置对象序列易变
				is_sorted = true;
				//析构对象索引映射
				object_index_map.~unordered_map();	
				//分配内存并记录投影字段
				new (&projector) std::function<Key(const T&)>(proj);
			}
			else
				projector = proj;

			//若未记录有效索引起点
			if (!min_valid_index.has_value())
			{
				//重置索引分配器
				index_allocator.reset();
				//升序排序使非法记录移动到序列前端
				std::ranges::sort(objects, std::ranges::less, [](const T& o) { return o.ID(); });
				//获取有效索引起点
				for (int filter_index = 0; filter_index < objects.size(); filter_index++)
				{
					//若当前对象索引有效
					if (objects[filter_index].ID() > 0)
					{
						min_valid_index = filter_index;
						break;
					}
					else
						//回收无效索引
						index_allocator.recycle(filter_index);
				}
			}

			//记录排序方式
			is_greater = greater;
			//重排序对象
			if (!is_greater)
				std::ranges::sort(objects.begin() + min_valid_index, objects.end(),
					std::ranges::less, proj);
			else
				std::ranges::sort(objects.begin() + min_valid_index, objects.end(),
					std::ranges::greater, proj);
		}
		//对象排列方式重置
		void sort_order_reset(void)
		{
			//若当前为稳定排列模式
			if (!is_sorted)
				return;
			else
			{
				//析构投影字段
				projector.~function();
				//清除有效索引起点
				min_valid_index.~optional();
				//构造对象索引映射
				new (&object_index_map) std::unordered_map<uint64_t, uint64_t>();
				//重建映射
				for (uint64_t index = 0; index < objects.size(); index++)
				{
					if (objects[index].ID() > 0)
						object_index_map.insert({ objects[index].ID(), index });
				}
				//标记回到稳定模式
				is_sorted = false;
			}
		}
		//对象查找 —— 连续存储重载
		typename std::vector<T>::iterator find(const Key& key)
		{
			//若对象池序列稳定且为整数Key
			if (!is_sorted && is_key_integral)
			{
				//获取索引映射迭代器
				auto it = object_index_map.find(key);
				//若迭代器有效
				if (it != object_index_map.end())
					return objects.begin() + it->second;
				//若不存在目标对象则返回超尾迭代器
				else
					return objects.end();
			}
			//若对象池序列稳定且非整数Key
			else if (!is_sorted && !is_key_integral)
			{
				Log::error("Object_Pool::当前对象池未排序\n无法使用ID以外字段查找目标对象");
				return objects.end();
			}
			else
			{
				//目标对象索引存储
				int index;
				//获取目标对象索引
				if(!is_greater)
				    index = binary_search(objects.begin() + min_valid_index.value(), objects.end(),
					   key, std::ranges::less, projector);
				else
					index = binary_search(objects.begin() + min_valid_index.value(), objects.end(),
						key, std::ranges::greater, projector);
				//若返回索引有效
				if (index >= 0)
					return objects.begin() + min_valid_index.value() + index;
				//若不存在目标对象则返回超尾迭代器
				else
					return objects.end();
			}
		}
		//对象添加
		uint64_t build(void)
		{
			//分配新对象索引
			uint64_t index = index_allocator.get();
			//若索引越界
			if (index >= objects.size())
				objects.push_back({});
			//若索引位于有效区边界则拓展有效区
			else if (index == min_valid_index.value() - 1)
				min_valid_index.value()--;

			//获取新对象
			auto& new_object = objects[index];
			//重置该对象避免数据残留
			new_object = T{};
			//分配对象ID
			new_object.ID_set(ID_allocator.get());
			//设置记录有效
			new_object.valid_set(true);
			//若序列稳定则记录索引映射
			if(!is_sorted)
				object_index_map.insert({ new_object.ID(), index });

			//返回对象ID
			return new_object.ID();
		}
		//对象卸载
		void unload(const Key& key)
		{
			//获取目标对象迭代器
			auto it = find(key;
			//若目标迭代器有效
			if (it != objects.end())
			{
				//记录目标对象索引
				uint64_t target_index = it - objects.begin();
				//获取目标对象ID
				uint64_t ID = it->ID();
				//回收目标对象ID
				ID_allocator.recycle(ID);
				//设置目标对象记录不合法
				objects[target_index].valid_set(false);
				//若序列稳定
				if(!is_sorted)
				{
					//取消目标对象索引映射
					object_index_map.erase(ID);
					//回收目标对象索引
					index_allocator.recycle(target_index);
				}
				else
				{
					//将无效对象移动至容器前端
					std::swap(objects[min_valid_index.value()], objects[target_index]);
					//回收目标对象索引
					index_allocator.recycle(min_valid_index.value());
					//更新有效索引起点
					min_valid_index.value()++;
				}
			}
			else
			{
				Log::warn("Object_Pool::目标对象不存在");
				return;
			}
				
		}
		//对象卸载 —— 多对象重载
		void unload(const std::vector<Key>& keys)
		{
			//调用单对象卸载重载
			for (const auto& key : keys)
				unload(key);
		}
		//对象清除
		void clear(void)
		{
			//清空所有对象
			objects.clear();
			//清空所有ID记录
			ID_allocator.reset();      
			//清空所有索引记录
			index_allocator.reset();
			//若为稳定模式清空所有索引映射
			if(is_sorted)
			    object_index_map.clear();
			else
			{
				//重置投影字段
				projector = {};
				//重置比较方式(默认降序)
				is_greater = false;
				//重置有效索引起点
				min_valid_index = std::nullopt;
			}
		}
		//全部对象获取
		std::vector<T>& data(void)
		{
			//返回全部对象
			return objects;
		}
		//超尾迭代器获取
		typename std::vector<T>::iterator end(void)
		{
			return objects.end();
		}
	};
}