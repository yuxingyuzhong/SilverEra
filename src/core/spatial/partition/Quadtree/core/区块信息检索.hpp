#pragma once
#include "../函数预声明.h"

//展开命名空间
namespace engine
{
	//范围查询可行性分析
	template <typename T>
	void Quadtree<T>::seekable_analyse(const Rect2l& format_range, Rect2l& seekable_range, bool read_only)
	{
		for (;;)
		{
			//计算四叉树当前查询范围
			manage_range_calcu(seekable_range, state.root, state.size);
			//获取是否扩大标记
			Point2d expand_register = seekable_range_calcu(format_range, seekable_range);

			//若返回坐标非树根节点坐标且非只读查询模式
			//则进行扩大(若存在管理层则进行申请)
			if (expand_register != state.root && !read_only)
			{
				//若存在回调则进行扩大申请
				if (callback)
				{
					//若扩大申请通过通过则扩大
					//扩大标记取自可查询范围边界，可能超出 32 位整数范围
					//故按 64 位整数通报
					if (callback(state.root, point_to_l(expand_register)))
					{
						//若扩大失败则直接结束计算
						if (!tree_expand())
							break;
					}
					//若扩大申请未通过则直接结束循环
					else
						break;
				}
				//若不存在回调则直接进行扩大
				else if (state.size < state.max_size)
				{
					//若扩大失败则直接结束计算
					if (!tree_expand())
						break;
				}

				//若当前四叉树大小以及大于等于上限大小则直接结束计算
				if (state.size >= state.max_size)
					break;
			}
			//反之则直接退出
			else
				break;
		}
	}

	//区块构建 —— 单区块重载(可直接挂载数据)
	template <typename T>
	void Quadtree<T>::build(const Point2l& target, const std::shared_ptr<T>& data)
	{
		//参数填充临时对象
		std::shared_ptr<Tree_Chunk_Data<T>> temp;
		//启动节点创建模式
		chunk_operate(target,Operate_Mode::Build,temp,data);
	}

	//区块构建 —— 范围重载(不可直接挂载数据)
	template <typename T>
	void Quadtree<T>::build(const Rect2l& target_range)
	{
		//参数填充临时对象
		std::vector<std::shared_ptr<Tree_Chunk_Data<T>>> temp;
		//启动节点创建模式
		chunk_operate(target_range,Operate_Mode::Build,temp);
	}

	//区块查找 —— 单区块重载
	template <typename T>
	void Quadtree<T>::seek(const Point2l& target, std::shared_ptr<Tree_Chunk_Data<T>>& receiver)
	{
		//启动节点查找模式
		chunk_operate(target, Operate_Mode::Seek,receiver);
	}

	//区块查找 —— 范围重载
	template <typename T>
	void Quadtree<T>::seek(const Rect2l& target_range,
		std::vector<std::shared_ptr<Tree_Chunk_Data<T>>>& receiver)
	{
		//启动节点查找模式
		chunk_operate(target_range, Operate_Mode::Seek,receiver);
	}

	//区块获取 —— 单区块重载
	template <typename T>
	void Quadtree<T>::get(const Point2l& target, std::shared_ptr<Tree_Chunk_Data<T>>& receiver)
	{
		//启动节点创建模式
		chunk_operate(target, Operate_Mode::Get, receiver);
	}

	//区块获取 —— 范围重载
	template <typename T>
	void Quadtree<T>::get(const Rect2l& target_range,
		std::vector<std::shared_ptr<Tree_Chunk_Data<T>>>& receiver)
	{
		//启动节点查找模式
		chunk_operate(target_range, Operate_Mode::Get, receiver);
	}

	//区块卸载 —— 单区块重载
	template <typename T>
	void Quadtree<T>::unload(const Point2l& target)
	{
		//参数填充临时对象
		std::shared_ptr<Tree_Chunk_Data<T>> temp;
		//启动节点卸载模式
		chunk_operate(target, Operate_Mode::Unload,temp);
	}

	//区块卸载 —— 范围重载
	template <typename T>
	void Quadtree<T>::unload(const Rect2l& target_range)
	{
		//参数填充临时对象
		std::vector<std::shared_ptr<Tree_Chunk_Data<T>>> temp;
		//启动节点卸载模式
		chunk_operate(target_range, Operate_Mode::Unload,temp);
	}

	//递归叶子
	template <typename T>
	void Quadtree<T>::leaf_recur(const Rect2l& target_range, const Rect2l& parent_range, Operate_Mode mode,
		int now_level, const int& max_level, Node* parent_node,
		std::vector<std::shared_ptr<Tree_Chunk_Data<T>>>& receiver)
	{
		//默认非只读操作
		bool read_only = false;
		//若当前为区块查找/卸载模式
		if (mode == Operate_Mode::Seek || mode == Operate_Mode::Unload)
			read_only = true;

		//子节点指针存储
		Node* child_node = parent_node;
		//子节点范围存储
		Rect2l child_range{};

		//若当前为最后一级递归
		if (now_level == max_level)
		{
			//一次性取出当前节点下辖所有待取出叶子节点
			for (int recur_direct = Recur_Direct::NW; recur_direct <= Recur_Direct::SE; recur_direct++)
			{
				//计算子节点范围
				child_node_range_calcu(recur_direct, child_range, parent_range);

				//若当前叶子节点未在可查询范围内则略过
				if (!range_relation_get(target_range, child_range))
					continue;

				//若为区块卸载模式
				if (mode == Operate_Mode::Unload)
				{
					//卸载叶子节点本身(子节点槽置空即删除该叶子)
					std::get<0>(parent_node->data)[recur_direct].reset();
					continue;
				}
				//若为其余模式
				else 
				{
					//重置子节点指针
					child_node = parent_node;
					//递归子节点
					//若递归失败则查找下一节点
					if (!child_node_recur(child_node, recur_direct, Node_Type::LEAF, read_only))
						continue;

					//若为区块查找/获取模式
					if(mode == Operate_Mode::Seek || mode == Operate_Mode::Get)
					{
						//记录查询结果
						//区块中心坐标由 64 位范围求得
						//先以双精度求中点再落单精度，尽量减少精度损失
						//结果对象与叶子区块数据共享所有权
						std::shared_ptr<Tree_Chunk_Data<T>> new_data(new(std::nothrow) Tree_Chunk_Data<T>
							(static_cast<float>((child_range.left + child_range.right) / 2.0),
								static_cast<float>((child_range.up + child_range.down) / 2.0),
								std::get<1>(child_node->data)));
						//若内存分配失败则直接返回
						if (new_data == nullptr)
							return;
						//记录查询结果
						receiver.push_back(new_data);
					}
				}
			}
		}
		//若当前非最后一级递归
		else
		{
			//寻找可查找子节点
			for (int now_direct = Recur_Direct::NW; now_direct <= Recur_Direct::SE; now_direct++)
			{
				//重置子节点指针
				child_node = parent_node;
				//计算子节点管理范围
				child_node_range_calcu(now_direct, child_range, parent_range);
				//若子节点包含待查找范围
				//无论全包含或者部分包含
				if (range_relation_get(target_range, child_range))
				{
					//若子节点递归失败则放弃该方向递归
					if (!child_node_recur(child_node, now_direct, Node_Type::MIDDLE, read_only))
						continue;
					//若子节点递归成功则进入下一级递归函数
					else
						leaf_recur(target_range, child_range, mode,
							now_level + 1, max_level, child_node, receiver);
				}
			}
		}
	}

	//区块操作 —— 单区块重载
	template <typename T>
	void Quadtree<T>::chunk_operate(const Point2l& target, Operate_Mode mode,
		std::shared_ptr<Tree_Chunk_Data<T>>& receiver,
		const std::shared_ptr<T>& data)
	{
		//计算目标点所属区块范围
		Rect2l target_range{};
		inclusion_range_calcu(target_range,target);
		//子节点数据缓冲
		std::vector<std::shared_ptr<Tree_Chunk_Data<T>>> buffer{};
		//调用范围区块操作重载
		chunk_operate(target_range,mode,buffer);
		//接收数据
		receiver = buffer.front();
	}

	//区块操作 —— 范围区块重载
	template <typename T>
	void Quadtree<T>::chunk_operate(const Rect2l& target_range, Operate_Mode mode,
		std::vector<std::shared_ptr<Tree_Chunk_Data<T>>>& receiver)
	{
		//默认非只读操作
		bool read_only = false;
		//若当前为区块查找/卸载模式
		if (mode == Operate_Mode::Seek || mode == Operate_Mode::Unload)
			read_only = true;

		//可查询范围存储
		Rect2l seekable_range{};
		//格式化待查询范围存储
		Rect2l format_range = target_range;
		//格式化待查询范围
		target_range_format(format_range, state.root, state.block_size);
		//分析获得可查询范围
		seekable_analyse(format_range, seekable_range, read_only);

		//根节点寻址总级数声明
		int recur_level_max = 0;
		//寻址总级数计算
		recur_level_calcu(recur_level_max);
		//父节点指针存储
		Node* parent_node = &root;
		//父节点管理范围存储
		Rect2l parent_range{};
		//父节点初始化为四叉树管理范围
		manage_range_calcu(parent_range, state.root, state.size);

		//递归收集叶子节点
		leaf_recur(seekable_range,parent_range,mode,
			1, recur_level_max,parent_node,receiver);
	}
}
