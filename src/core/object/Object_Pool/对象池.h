#pragma once
//预编译头
#include "Engine/EngineCore/common/前置头文件包含.h"
//获取预定义对象类型
#include "../Object/对象.h"
//获取数值分配器
#include "Engine/EngineCore/src/tools/Number_Allocator/数值分配器.h"
//获取二分查找算法
#include "Engine/EngineCore/src/tools/Detail/二分查找.h"

namespace engine
{
	//对象排列顺序 
	enum class Object_Order
	{
		Unorder,
		Sorted
	};
	//默认对象定位投影器 —— 以对象ID作为定位键
	struct Default_Projector
	{
		template <typename T>
		uint64_t operator()(const T& object) const
		{
			return object.ID();
		}
	};

	//对象池 —— 模板需从对象中派生
	//Projector为对象定位投影器类型，Key由投影器返回类型自动推导
	template <typename T, typename Projector = Default_Projector,
		typename Key = std::invoke_result_t<Projector, const T&>>
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

		//ID分配器
		Number_Allocator<uint64_t> ID_allocator;
		//索引分配器
		Number_Allocator<uint64_t> index_allocator;

		//ID分配起点
		uint64_t min_ID = 1;
		//有效索引起点
		uint64_t min_valid_index = 0;
		//对象集合
		std::vector<T> objects;

		//对象定位投影器(无状态时经空基类优化不占空间)
		[[no_unique_address]] Projector projector;
		//对象定位方式标记
		bool is_sorted = false;
		//对象映射
		std::unordered_map<Key, uint64_t> mapping;
		//比较方式(默认升序)
		Comparator compare;

	public:
		//构造函数
		Object_Pool()
		{
			//设置后进先出机制分配回收ID
			ID_allocator.set(Allocate_Order::LIFO);
			index_allocator.set(Allocate_Order::LIFO);
			//为非法实体预留ID
			ID_allocator.set(min_ID);
			//显示指定起始索引
			index_allocator.set(0);
		}
		//析构函数
		~Object_Pool()
		{}
		//对象排列方式设置
		template <typename Compare = std::ranges::less>
			requires std::same_as<Compare, std::ranges::less> ||
		std::same_as<Compare, std::ranges::greater>
			void mode_set(Object_Order mode, Compare cmp = {})
		{
			//若为哈希定位模式设置
			if (mode == Object_Order::Unorder && is_sorted)
			{
				//标记回到稳定模式
				is_sorted = false;
				//重建映射
				for (uint64_t rebuild_index = 0; rebuild_index < objects.size(); rebuild_index++)
				{
					//若对象合法则建立映射
					if (objects[rebuild_index].valid())
						mapping.insert({ projector(objects[rebuild_index]), rebuild_index });
				}
				//重置比较方式
				compare = std::ranges::less();
			}
			//若为排序模式设置
			else if (mode == Object_Order::Sorted)
			{
				//若当前为哈希定位模式
				if (!is_sorted)
				{
					//标记切换排序模式
					is_sorted = true;
					//清空映射
					mapping.clear();
				}

				//记录排序方式
				compare = cmp;
				//重排序对象
				sort();
			}
			//若为无效模式切换指令
			else
				logger.warn("Object_Pool::当前已为目标模式");
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
				//获取目标对象索引
				std::optional<uint64_t> object_index = detail::binary_search
				(begin(), end(), key, compare, projector);
				//若返回索引有效
				if (object_index.has_value())
					return begin() + object_index.value();
				//若不存在目标对象则返回超尾迭代器
				else
					return end();
			}
		}
		//对象查找 —— 常量重载
		typename std::vector<T>::const_iterator find(const Key& key) const
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
				//获取目标对象索引
				std::optional<uint64_t> object_index = detail::binary_search
				(begin(), end(), key, compare, projector);
				//若返回索引有效
				if (object_index.has_value())
					return begin() + object_index.value();
				//若不存在目标对象则返回超尾迭代器
				else
					return end();
			}
		}
		//对象添加
		uint64_t build(T object = {})
		{
			//分配新对象索引
			uint64_t object_index = index_allocator.get();
			//若槽位不足则直接创建新对象
			if (object_index >= objects.size())
				objects.push_back(std::move(object));
			//若获得旧槽位则销毁旧对象并原地重建新对象
			else
			{
				//析构旧对象
				std::destroy_at(&objects[object_index]);
				//在旧槽位原地构造新对象
				std::construct_at(&objects[object_index], std::move(object));
			}

			//若当前存在无效区间
			if (min_valid_index > 0)
			{
				//将新对象交换至无效区边界并收缩无效区
				std::swap(objects[object_index], objects[--min_valid_index]);
				//更新新对象索引
				object_index = min_valid_index;
			}

			//获取新对象
			auto& new_object = objects[object_index];
			//分配对象ID
			new_object.ID_set(ID_allocator.get());
			//设置记录有效
			new_object.valid_set(true);

			//若为哈希定位模式则记录索引映射
			if (!is_sorted)
				mapping.insert_or_assign(projector(new_object), object_index);
			//若为排序模式则提示调用方重排序
			else
				logger.info("Object_Pool::请自行调用sort()方法重排序对象");

			//返回对象ID
			return new_object.ID();
		}
		//对象排序
		bool sort(void)
		{
			//若非排序模式则返回排序失败
			if (!is_sorted)
				return false;
			//若为排序模式则进行重排序
			else
				std::ranges::sort(begin(), end(), compare, projector);

			return true;
		}
		//对象卸载 —— 单对象重载
		void unload(const Key& key)
		{
			//调用多对象重载
			unload(std::vector<Key>{key});
		}
		//对象卸载 —— 多对象重载
		void unload(const std::vector<Key>& keys)
		{
			//逐个卸载目标对象
			for (const auto& key : keys)
			{
				//获取目标对象迭代器
				auto it = find(key);
				//若目标迭代器无效
				if (it == objects.end())
				{
					logger.warn("Object_Pool::目标对象不存在: {}", key);
					continue;
				}
				//记录目标对象绝对槽位
				uint64_t target_index = it - objects.begin();
				//获取目标对象ID
				uint64_t ID = it->ID();
				//回收目标对象ID
				ID_allocator.recycle(ID);
				//设置预留无效ID
				it->ID_set(0);
				//设置目标对象记录不合法
				it->valid_set(false);

				//若目标不在无效区边界则交换到边界
				if (target_index != min_valid_index)
				{
					std::swap(objects[target_index], objects[min_valid_index]);
					//若为哈希定位模式则同步更新被交换有效对象映射索引
					if (!is_sorted)
						mapping.insert_or_assign(projector(objects[target_index]), target_index);
				}
				//回收目标对象索引
				index_allocator.recycle(min_valid_index);
				//更新有效索引起点
				min_valid_index++;

				//若为哈希定位模式则取消被卸载对象映射
				if (!is_sorted)
					mapping.erase(key);
				//若为排序模式则重排序，保证后续 find 的二分有效
				else
					sort();
			}
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
			//重置有效索引起点
			min_valid_index = 0;

			//若为哈希定位模式则清空所有索引映射
			if (!is_sorted)
				mapping.clear();
			//若为排序模式则清空所有排序信息
			else
			{
				//重置为哈希定位模式
				is_sorted = false;
				//重置比较方式(默认升序)
				compare = std::ranges::less();
			}
		}

		//对象池槽位预分配
		bool reserve(const uint64_t& num)
		{
			//若当前对象池非空
			if (!objects.empty())
				return false;
			else
			{
				//若对象池为空则预创建非法对象
				for (uint64_t time = 0; time < num; time++)
				{
					//构建新对象槽位
					objects.push_back({});
					//设置无效ID
					objects.back().ID_set(min_ID - 1);
					//设置对象非法
					objects.back().valid_set(false);
				}
				//将预留槽位划入无效区
				min_valid_index = num;
				//返回槽位分配成功
				return true;
			}
		}
		//对象池槽位数量获取
		uint64_t capacity(void) const
		{
			return objects.size();
		}
		//合法对象数量获取
		uint64_t size(void) const
		{
			return ID_allocator.counts();
		}

		//有效元素首迭代器获取 —— 非常量重载
		typename std::vector<T>::iterator begin(void)
		{
			return objects.begin() + min_valid_index;
		}
		//有效元素首迭代器获取 —— 常量重载
		typename std::vector<T>::const_iterator begin(void) const
		{
			return objects.begin() + min_valid_index;
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

		//迭代器转换索引 —— 非常量迭代器重载
		uint64_t index(const typename std::vector<T>::iterator& iterator)
		{
			return iterator - begin();
		}
		//迭代器转换索引 —— 常量迭代器重载
		uint64_t index(const typename std::vector<T>::const_iterator& iterator) const
		{
			return iterator - begin();
		}

		//[]运算符重载 —— 定位键重载
		T& operator[](const Key& key)
		{
			//获取目标对象迭代器
			auto it = find(key);
			//调用方须保证目标对象存在
			assert(it != objects.end());
			return *it;
		}
		//[]运算符重载 —— 定位键重载（常量版本）
		const T& operator[](const Key& key) const
		{
			//获取目标对象迭代器
			auto it = find(key);
			//调用方须保证目标对象存在
			assert(it != objects.end());
			return *it;
		}
		//相对偏移访问 —— 非常量重载
		T& object_at(uint64_t offset)
		{
			//调用方须保证偏移在有效区内
			assert(offset < size());
			return *(begin() + offset);
		}
		//相对偏移访问 —— 常量重载
		const T& object_at(uint64_t offset) const
		{
			//调用方须保证偏移在有效区内
			assert(offset < size());
			return *(begin() + offset);
		}
	};
}