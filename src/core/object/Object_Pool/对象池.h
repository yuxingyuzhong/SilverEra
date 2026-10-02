#pragma once
//预编译头
#include "common/前置头文件包含.h"
//获取预定义对象类型
#include "../Object/对象.h"
//获取数值分配器
#include "src/tools/Number_Allocator/数值分配器.h"
//获取二分查找算法
#include "src/tools/Detail/二分查找.h"

namespace engine
{
	//对象池 —— 模板需从对象中派生
	template <typename T, typename Key = uint64_t>
		requires std::is_base_of_v<Object, T>
	class Object_Pool
	{
	private:
		//比较器
		class Comparator
		{
			std::variant<std::ranges::less, std::ranges::greater> v;
		public:
			Comparator() = default;
			Comparator(std::ranges::less l) : v(l) {}
			Comparator(std::ranges::greater g) : v(g) {}

			template <typename A, typename B>
			bool operator()(A&& a, B&& b) const
			{
				return std::visit(
					[&](auto cmp) { return cmp(std::forward<A>(a), std::forward<B>(b)); },
					v);
			}
		};

		//ID分配起点
		uint64_t min_ID = 1;
		//ID分配器
		Number_Allocator<uint64_t> ID_allocator;
		//索引分配器
		Number_Allocator<uint64_t> index_allocator;

		//对象集合
		std::vector<T> objects;
		//对象定位投影字段
		std::function<Key(const T&)> projector;
		//对象定位方式标记
		bool is_sorted = false;

		//对象映射
		std::unordered_map<Key, uint64_t> mapping;

		//比较方式(默认升序)
		Comparator compare;
		//有效索引起点
		std::optional<uint64_t> min_valid_index = std::nullopt;

	public:
		//构造函数
		explicit Object_Pool(std::function<Key(const T&)> proj)
		{
			//设置后进先出机制分配回收ID
			ID_allocator.set(Allocate_Order::LIFO);
			index_allocator.set(Allocate_Order::LIFO);
			//为非法实体预留ID
			ID_allocator.set(min_ID);
			//显示指定起始索引
			index_allocator.set(0);
			//记录定位投影字段
			projector = std::move(proj);
		}
		//析构函数
		~Object_Pool()
		{
		}
		//对象排列方式设置
		template <typename Compare>
			requires std::same_as<Compare, std::ranges::less> ||
		std::same_as<Compare, std::ranges::greater>
			void order_set(Compare cmp)
		{
			//若当前为哈希定位模式
			if (!is_sorted)
			{
				//清空映射
				mapping.clear();
				//标记切换排序模式
				is_sorted = true;
			}

			//记录排序方式
			compare = cmp;

			//若未记录有效索引起点
			if (!min_valid_index.has_value())
			{
				//升序排序使非法记录移动到序列前端
				std::ranges::sort(objects, std::ranges::less(), [](const T& o) { return o.ID(); });
				//获取有效索引起点
				for (uint64_t filter_index = 0; filter_index < objects.size(); filter_index++)
				{
					//若当前对象索引有效
					if (objects[filter_index].valid())
					{
						min_valid_index = filter_index;
						break;
					}
					else
						//回收无效索引
						index_allocator.recycle(filter_index);
				}
			}

			//若未获得有效索引起点
			if (!min_valid_index.has_value())
				return;
			else
			    //重排序对象
			    sort();
		}
		//对象排列方式重置
		void order_reset(void)
		{
			//若当前为哈希定位模式
			if (!is_sorted)
				return;
			else
			{
				//标记回到稳定模式
				is_sorted = false;
				//重建映射
				for (uint64_t index = 0; index < objects.size(); index++)
				{
					//若对象合法则建立映射
					if (objects[index].valid())
						mapping.insert({ projector(objects[index]), index });
				}
				//重置比较方式
				compare = std::ranges::less();
				//重置有效索引起点
				min_valid_index = std::nullopt;
			}
		}
		//对象查找
		typename std::vector<T>::iterator find(const Key& key)
		{
			//若为哈希定位模式
			if (!is_sorted)
			{
				//获取索引映射迭代器
				auto it = mapping.find(key);
				//若迭代器有效
				if (it != mapping.end())
					return objects.begin() + it->second;
				//若不存在目标对象则返回超尾迭代器
				else
					return objects.end();
			}
			//若为排序模式
			else
			{
				//若有效索引起点非有效值
				if (!min_valid_index.has_value())
				{
					Log::warn("Object_Pool::当前排序模式不可用\n请设置排序方式后再调用此方法");
					return objects.end();
				}

				//获取目标对象索引
				std::optional<uint64_t> index = detail::binary_search
				(objects.begin() + min_valid_index.value(), objects.end(),
					key, compare, projector);;
				//若返回索引有效
				if (index.has_value())
					return objects.begin() + min_valid_index.value() + index.value();
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

			//获取新对象
			auto& new_object = objects[index];
			//重置该对象避免数据残留
			new_object = T{};
			//分配对象ID
			new_object.ID_set(ID_allocator.get());
			//设置记录有效
			new_object.valid_set(true);

			//若为哈希定位模式则记录索引映射
			if (!is_sorted)
				mapping.insert({ projector(new_object), index });
			//若为排序模式则重排序对象
			if (is_sorted)
			{
				//简化表示路径
				auto& min_index = min_valid_index.value();
				//若有效区非全容器时
				if(min_index > 0)
				{
					//若新对象位于有效区边界则扩充有效区
					if (index == min_index - 1)
						min_index--;//索引分配采用LIFO机制
				}
				//重排序对象
				sort();
			}

			//返回对象ID
			return new_object.ID();
		}
		//对象排序
		bool sort(void)
		{
			//若有效索引起点非有效值
			if (!min_valid_index.has_value())
			{
				Log::warn("Object_Pool::当前排序模式不可用\n请设置排序方式后再调用此方法");
				return false;
			}

			//若非排序模式则返回排序失败
			if (!is_sorted)
				return false;
			//若为排序模式则进行重排序
			else
			    std::ranges::sort(objects.begin() + min_valid_index.value(), objects.end(),
			        compare, projector);

			return true;
		}
		//对象卸载
		void unload(const Key& key)
		{
			//获取目标对象迭代器
			auto it = find(key);
			//若目标迭代器有效
			if (it != objects.end())
			{
				//记录目标对象索引
				uint64_t target_index = it - objects.begin();
				//获取目标对象ID
				uint64_t ID = it->ID();
				//回收目标对象ID
				ID_allocator.recycle(ID);
				//设置预留无效ID
				it->ID_set(0);
				//设置目标对象记录不合法
				objects[target_index].valid_set(false);
				//若为哈希定位模式
				if(!is_sorted)
				{
					//取消目标对象定位字段映射
					mapping.erase(key);
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
			//恢复后进先出分配机制
			ID_allocator.set(Allocate_Order::LIFO);
			//恢复保留非法ID
			ID_allocator.set(min_ID);

			//清空所有索引记录
			index_allocator.reset();
			//恢复后进先出分配机制
			index_allocator.set(Allocate_Order::LIFO);
			//恢复显式指定起始索引
			index_allocator.set(0);

			//若为哈希定位模式则清空所有索引映射
			if(!is_sorted)
			    mapping.clear();
			//若为排序模式则清空所有排序信息
			else
			{
				//重置为哈希定位模式
				is_sorted = false;
				//重置比较方式(默认升序)
				compare = std::ranges::less();
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
		//超尾迭代器获取 —— 非常量重载
		typename std::vector<T>::iterator end(void)
		{
			return objects.end();
		}
		//超尾迭代器获取 —— 常量重载
		typename std::vector<T>::const_iterator end(void) const
		{
			return objects.end();
		}
	};
}