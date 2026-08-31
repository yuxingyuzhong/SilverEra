#pragma once
//预编译头
#include "common/前置头文件包含.h"
//获取预定义记录类型
#include "common/types/记录类型.h"
//获取数值分配器
#include "src/tools/Non_GUI/Number_Allocator/数值分配器.h"

namespace engine
{
	//对象池 —— 模板需从对象记录中派生
	template <typename T, typename = std::enable_if_t<std::is_base_of<object_record, T>::value>>
	class Object_Pool
	{
	private:
		//ID分配器
		Number_Allocator ID_allocator;
		//索引分配器
		Number_Allocator index_allocator;
		//对象记录集合
		std::vector<T> object_records;
		//对象索引映射
		std::unordered_map<uint64_t, uint64_t> object_index_map;

	public:
		//对象构建 —— 单对象重载
		uint64_t build(void)
		{
			//分配新对象索引
			uint64_t index = index_allocator.get();
			//若索引越界
			if (index >= object_records.size())
				object_records.resize(index + 1);

			//获取新对象记录
			auto& record = object_records[index];
			//分配对象ID
			record.ID = ID_allocator.get();
			//设置记录有效
			record.valid = true;
			//记录索引映射
			object_index_map.insert({ record.ID, index });

			//返回对象ID
			return record.ID;
		}
		//对象构建 —— 多对象重载
		std::vector<uint64_t> build(const uint64_t& counts)
		{
			//对象ID记录
			std::vector<uint64_t> IDs;
			//构建对象
			for (int allocate_times = 0;allocate_times < counts;allocate_times++)
				IDs.push_back(build());
				
			//返回构建对象ID集合
			return IDs;
		}
		//对象卸载 —— 单对象重载
		void unload(const uint64_t ID)
		{
			//获取目标对象迭代器
			auto it = object_index_map.find(ID);
			//若目标对象不存在
			if (it == object_index_map.end())
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
			object_records[index].valid = false;
			//取消目标对象索引映射
			object_index_map.erase(ID);
		}
		//对象卸载 —— 多对象重载
		void unload(const std::vector<uint64_t>& IDs)
		{
			//调用单对象卸载重载
			for (const auto& ID : IDs)
				unload(ID);
		}
		//对象清除
		void clear(void)
		{
			//清空所有对象
			object_records.clear();
			//清空所有ID记录
			ID_allocator.reset();      
			//清空所有索引记录
			index_allocator.reset();
			//清空所有索引映射
			object_index_map.clear();
		}
		//对象存在性确认
		bool count(const uint64_t& ID)
		{
			return object_index_map.count(ID);
		}
		//对象获取 —— 全量返回重载
		std::vector<T>& get(void)
		{
			//返回所有对象
			return object_records;
		}
		//对象获取 —— 多对象引用重载
		void get(std::vector<T*> receiver,const std::vector<uint64_t>& IDs)
		{
			//获取对象记录引用
			for (const auto& ID : IDs)
				receiver.push_back(get(ID));
		}
		//对象获取 —— 单对象返回重载
		T* get(const uint64_t& ID)
		{
			//获取目标对象迭代器
			auto it = object_index_map.find(ID);
			//若目标对象不存在
			if (it == object_index_map.end())
			{
				Log::warn("Object_Pool::编号{}对象不存在", ID);
				return nullptr;
			}

			//返回目标对象
			return &object_records[it->second];
		}
	};
}