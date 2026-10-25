#pragma once
#include "../函数预声明.h"

//展开命名空间
namespace engine
{
	//扩大可行性分析
	template <typename T>
	void Quadtree<T>::expand_analyse(const Rect2l& target_range, Rect2l& reachable_range, bool read_only)
	{
		for (;;)
		{
			//计算四叉树当前可查询范围
			reachable_range = rect_range_calcu(state.root, state.size);
			//求交并获取越界标记
			Point2d expand_mark = range_intersect(target_range, reachable_range);

			//若无需扩大或为只读操作则结束
			if (expand_mark == state.root || read_only)
				break;

			//若存在回调则交由上级管理对象仲裁
			if (callback)
			{
				//若扩大申请未通过则结束
				if (!callback(state.root, point_to_l(expand_mark)))
					break;
				//若扩大失败则结束
				if (!tree_expand())
					break;
			}
			//若不存在回调则按自身上限直接扩大
			else
			{
				//已达上限则结束
				if (state.size >= state.max_size)
					break;
				//若扩大失败则结束
				if (!tree_expand())
					break;
			}

			//若已达上限则结束
			if (state.size >= state.max_size)
				break;
		}
	}

	//区块构建 —— 单区块重载(可直接挂载数据)
	template <typename T>
	void Quadtree<T>::build(const Point2l& target, const std::shared_ptr<T>& data)
	{
		//参数填充临时对象
		std::vector<Tree_Chunk_Data<T>> temp{};
		//启动节点创建模式
		chunk_dispatch(target, Chunk_Mode::Build, temp, data);
	}

	//区块构建 —— 范围重载(不可直接挂载数据)
	template <typename T>
	void Quadtree<T>::build(const Rect2l& target_range)
	{
		//参数填充临时对象
		std::vector<Tree_Chunk_Data<T>> temp{};
		//启动节点创建模式
		chunk_dispatch(target_range, Chunk_Mode::Build, temp);
	}

	//区块查找 —— 单区块重载
	template <typename T>
	bool Quadtree<T>::seek(const Point2l& target, Tree_Chunk_Data<T>& receiver)
	{
		//查询结果缓冲
		std::vector<Tree_Chunk_Data<T>> buffer{};
		//启动节点查找模式
		chunk_dispatch(target, Chunk_Mode::Seek, buffer);
		//无结果则返回未命中
		if (buffer.empty())
			return false;
		//取出单区块结果
		receiver = std::move(buffer.front());
		return true;
	}

	//区块查找 —— 范围重载
	template <typename T>
	void Quadtree<T>::seek(const Rect2l& target_range, std::vector<Tree_Chunk_Data<T>>& receiver)
	{
		//启动节点查找模式
		chunk_dispatch(target_range, Chunk_Mode::Seek, receiver);
	}

	//区块获取 —— 单区块重载
	template <typename T>
	bool Quadtree<T>::get(const Point2l& target, Tree_Chunk_Data<T>& receiver)
	{
		//查询结果缓冲
		std::vector<Tree_Chunk_Data<T>> buffer{};
		//启动节点获取模式
		chunk_dispatch(target, Chunk_Mode::Get, buffer);
		//无结果则返回未命中
		if (buffer.empty())
			return false;
		//取出单区块结果
		receiver = std::move(buffer.front());
		return true;
	}

	//区块获取 —— 范围重载
	template <typename T>
	void Quadtree<T>::get(const Rect2l& target_range, std::vector<Tree_Chunk_Data<T>>& receiver)
	{
		//启动节点获取模式
		chunk_dispatch(target_range, Chunk_Mode::Get, receiver);
	}

	//区块卸载 —— 单区块重载
	template <typename T>
	void Quadtree<T>::unload(const Point2l& target)
	{
		//参数填充临时对象
		std::vector<Tree_Chunk_Data<T>> temp{};
		//启动节点卸载模式
		chunk_dispatch(target, Chunk_Mode::Unload, temp);
	}

	//区块卸载 —— 范围重载
	template <typename T>
	void Quadtree<T>::unload(const Rect2l& target_range)
	{
		//参数填充临时对象
		std::vector<Tree_Chunk_Data<T>> temp{};
		//启动节点卸载模式
		chunk_dispatch(target_range, Chunk_Mode::Unload, temp);
	}

	//区块递归
	template <typename T>
	void Quadtree<T>::chunk_recur(Node* node, const Rect2l& node_range, const Rect2l& target_range,
		int now_level, const int& max_level, bool read_only,
		const Chunk_Mode& mode, std::vector<Tree_Chunk_Data<T>>& receiver,
		const std::shared_ptr<T>& data)
	{
		//若已到叶子父层则处理四个叶子槽
		if (now_level == max_level)
		{
			leaf_slot_apply(node, node_range, target_range, mode, read_only, receiver, data);
			return;
		}

		//遍历四个方向
		for (int quadrant = Quadrant::NW; quadrant <= Quadrant::SE; quadrant++)
		{
			//计算子节点范围
			Rect2l child_range = child_range_calcu(quadrant, node_range);
			//若子节点与目标范围不相交则略过
			if (!range_overlap_check(target_range, child_range))
				continue;
			//重置子节点指针
			Node* child_node = node;
			//推进至子节点
			if (!child_step(child_node, quadrant, Node_Kind::Middle, read_only))
				continue;
			//进入下一级递归
			chunk_recur(child_node, child_range, target_range, now_level + 1, max_level,
				read_only, mode, receiver, data);
		}
	}

	//叶子槽操作
	template <typename T>
	void Quadtree<T>::leaf_slot_apply(Node* node, const Rect2l& node_range, const Rect2l& target_range,
		const Chunk_Mode& mode, bool read_only,
		std::vector<Tree_Chunk_Data<T>>& receiver,
		const std::shared_ptr<T>& data)
	{
		//获取父节点子节点列表
		std::array<std::shared_ptr<Node>, 4>& slot_list = std::get<0>(node->data);

		//遍历四个方向
		for (int quadrant = Quadrant::NW; quadrant <= Quadrant::SE; quadrant++)
		{
			//计算子节点范围
			Rect2l child_range = child_range_calcu(quadrant, node_range);
			//若子节点与目标范围不相交则略过
			if (!range_overlap_check(target_range, child_range))
				continue;

			//若为卸载模式
			if (mode == Chunk_Mode::Unload)
			{
				//仅移除叶子节点，保留可能存在的子树
				std::shared_ptr<Node>& slot = slot_list[quadrant];
				if (slot != nullptr && slot->data.index() == 1)
					slot.reset();
				continue;
			}

			//重置子节点指针
			Node* child_node = node;
			//推进至叶子节点(缺失时按模式创建)
			if (!child_step(child_node, quadrant, Node_Kind::Leaf, read_only, data))
				continue;

			//若为查找/获取模式则记录结果
			if (mode == Chunk_Mode::Seek || mode == Chunk_Mode::Get)
				receiver.emplace_back(center_calcu(child_range), std::get<1>(child_node->data));
		}
	}

	//区块操作 —— 单区块重载
	template <typename T>
	void Quadtree<T>::chunk_dispatch(const Point2l& target, const Chunk_Mode& mode,
		std::vector<Tree_Chunk_Data<T>>& receiver,
		const std::shared_ptr<T>& data)
	{
		//计算目标点所属区块范围
		Rect2l target_range = chunk_range_calcu(target);
		//转调范围区块操作重载
		chunk_dispatch(target_range, mode, receiver, data);
	}

	//区块操作 —— 范围重载
	template <typename T>
	void Quadtree<T>::chunk_dispatch(const Rect2l& target_range, const Chunk_Mode& mode,
		std::vector<Tree_Chunk_Data<T>>& receiver,
		const std::shared_ptr<T>& data)
	{
		//默认非只读操作
		bool read_only = false;
		//若当前为区块查找/卸载模式
		if (mode == Chunk_Mode::Seek || mode == Chunk_Mode::Unload)
			read_only = true;

		//格式化待查询范围存储
		Rect2l format_range = target_range;
		//格式化待查询范围
		range_align(format_range, state.root, state.block_size);

		//可查询范围存储
		Rect2l reachable_range{};
		//分析获得可查询范围
		expand_analyse(format_range, reachable_range, read_only);

		//寻址总级数计算
		int max_level = depth_calcu();
		//退化树无可寻址叶子
		if (max_level <= 0)
			return;

		//四叉树管理范围计算
		Rect2l tree_range = rect_range_calcu(state.root, state.size);
		//递归处理区块
		chunk_recur(&root, tree_range, reachable_range, 1, max_level,
			read_only, mode, receiver, data);
	}
}
