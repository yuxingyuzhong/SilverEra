#pragma once
//预编译头
#include "common/前置头文件包含.h"
//获取数值分配器
#include "src/tools/Non_GUI/Number_Allocator/数值分配器.h"

namespace engine
{
	//对象记录
	struct object_record
	{
		//对象ID
		uint64_t ID = 0;
		//记录合法标记
		bool vaild = false;
	};

	//对象管理器 —— 模板需从对象记录中派生
	template <typename T, typename = std::enable_if_t<std::is_base_of<object_record, T>::value>>
	class Object_Manager
	{
	private:
		//ID分配器
		Number_Allocator ID_allocator;
		//索引分配器
		Number_Allocator index_allocator;
		//对象记录集合
		std::vector<T> object_records;

	public:
		//对象构建
		void build(const uint64_t& counts)
		{
			//构建对象
			for (int allocate_times = 0;allocate_times<counts;allocate_times++)
			{
				//分配新对象索引
				uint64_t index = index_allocator.get();
				//若索引越界
				if (index >= object_records.size())
					object_records.push_back({});

				//获取新对象记录
				auto& record = object_records[index];
				//分配对象ID
				record.ID = ID_allocator.get();
				//设置记录有效
				record.valid = true;
			}
		}
		//对象卸载
		void unload(const std::vector<uint64_t> ID_sets)
		{

		}
		//对象清除
		void erase(void);
		//对象获取 —— 全量返回重载
		std::vector<T>& get(void);
		//对象获取 —— 多对象返回重载
		bool get(std::vector<T>& receiver);
		//对象获取 —— 单对象返回重载
		bool get(T& receiver);
	};
}