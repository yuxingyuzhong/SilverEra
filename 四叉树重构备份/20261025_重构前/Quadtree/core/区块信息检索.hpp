#pragma once
#include "../函数预声明.h"

//展开命名空间
namespace engine
{
	//范围查询可行性分析
	template <typename T>
	void Quadtree<T>::reachable_range_calcu(const Rect2l& format_range, Rect2l& reachable_range, bool read_only)
	{
		for (;;)
		{
			//计算四叉树当前查询范围
			reachable_range = recta_range_calcu(state.root, state.size);
			//获取是否扩大标记
			Point2d expand_register = reachable_range_calcu(format_range, reachable_range);

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
		Tree_Chunk_Data<T> temp{};
		//启动节点创建模式
		chunk_operate(target, Operate_Mode::Build, temp, data);
	}

	//区块构建 —— 范围重载(不可直接挂载数据)
	template <typename T>
	void Quadtree<T>::build(const Rect2l& target_range)
	{
		//参数填充临时对象
		std::vector<Tree_Chunk_Data<T>> temp{};
		//启动节点创建模式
		chunk_operate(target_range, Operate_Mode::Build, temp);
	}

	//区块查找 —— 单区块重载
	template <typename T>
	void Quadtree<T>::seek(const Point2l& target, Tree_Chunk_Data<T>& receiver)
	{
		//启动节点查找模式
		chunk_operate(target, Operate_Mode::Seek,receiver);
	}

	//区块查找 —— 范围重载
	template <typename T>
	void Quadtree<T>::seek(const Rect2l& target_range,
		std::vector<Tree_Chunk_Data<T>>& receiver)
	{
		//启动节点查找模式
		chunk_operate(target_range, Operate_Mode::Seek,receiver);
	}

	//区块获取 —— 单区块重载
	template <typename T>
	void Quadtree<T>::get(const Point2l& target, Tree_Chunk_Data<T>& receiver)
	{
		//启动节点创建模式
		chunk_operate(target, Operate_Mode::Get, receiver);
	}

	//区块获取 —— 范围重载
	template <typename T>
	void Quadtree<T>::get(const Rect2l& target_range,
		std::vector<Tree_Chunk_Data<T>>& receiver)
	{
		//启动节点查找模式
		chunk_operate(target_range, Operate_Mode::Get, receiver);
	}

	//区块卸载 —— 单区块重载
	template <typename T>
	void Quadtree<T>::unload(const Point2l& target)
	{
		//参数填充临时对象
		Tree_Chunk_Data<T> temp{};
		//启动节点卸载模式
		chunk_operate(target, Operate_Mode::Unload, temp);
	}

	//区块卸载 —— 范围重载
	template <typename T>
	void Quadtree<T>::unload(const Rect2l& target_range)
	{
		//参数填充临时对象
		std::vector<Tree_Chunk_Data<T>> temp{};
		//启动节点卸载模式
		chunk_operate(target_range, Operate_Mode::Unload, temp);
	}

	//递归叶子父节点
	template <typename T>
	void Quadtree<T>::leaf_parent_get(std::vector<std::pair<Node*, Rect2l>>& receiver, Node* parent_node,
		const Rect2l& target_range, const Rect2l& parent_range,
		int now_level, const int& max_level, bool read_only)
	{
		//子节点指针存储
		Node* child_node = parent_node;
		//子节点范围存储
		Rect2l child_range{};

		//若当前为最后一级递归
		if (now_level == max_level)
			//叶子父节点与其范围写入接收容器
			receiver.emplace_back(parent_node, parent_range);
		//若当前非最后一级递归
		else
		{
			//寻找可查找子节点
			for (int now_direct = Recur_Direct::NW; now_direct <= Recur_Direct::SE; now_direct++)
			{
				//重置子节点指针
				child_node = parent_node;
				//计算子节点管理范围
				child_range = child_node_range_calcu(now_direct, parent_range);
				//若子节点包含待查找范围
				//无论全包含或者部分包含
				if (range_relation_get(target_range, child_range))
				{
					//若子节点递归失败则放弃该方向递归
					if (!child_node_recur(child_node, now_direct, Node_Type::MIDDLE, read_only))
						continue;
					//若子节点递归成功则进入下一级递归函数
					else
						leaf_parent_get(receiver, child_node,target_range, child_range,
							now_level + 1, max_level,read_only);
				}
			}
		}
	}

	//区块操作 —— 单区块重载
	template <typename T>
	void Quadtree<T>::chunk_operate(const Point2l& target, Operate_Mode mode,
		Tree_Chunk_Data<T>& receiver,
		const std::shared_ptr<T>& data)
	{
		//计算目标点所属区块范围
		Rect2l target_range = inclusion_range_calcu(target);
		//子节点数据缓冲
		std::vector<Tree_Chunk_Data<T>> buffer{};
		//调用范围区块操作重载
		chunk_operate(target_range,mode,buffer,data);
		//接收数据
		if(!buffer.empty())
		   receiver = buffer.front();
	}

	//区块操作 —— 范围区块重载
	template <typename T>
	void Quadtree<T>::chunk_operate(const Rect2l& target_range, Operate_Mode mode,
		std::vector<Tree_Chunk_Data<T>>& receiver,
		const std::shared_ptr<T>& data)
	{
		//默认非只读操作
		bool read_only = false;
		//若当前为区块查找/卸载模式
		if (mode == Operate_Mode::Seek || mode == Operate_Mode::Unload)
			read_only = true;

		//可查询范围存储
		Rect2l reachable_range{};
		//格式化待查询范围存储
		Rect2l format_range = target_range;
		//格式化待查询范围
		target_range_format(format_range, state.root, state.block_size);
		//分析获得可查询范围
		reachable_range_calcu(format_range, reachable_range, read_only);

		//寻址总级数计算
		int max_recur_level = recur_level_calcu();
		//退化树无可寻址叶子
		if (max_recur_level <= 0)
			return;
		//四叉树管理范围计算
		Rect2l tree_range = recta_range_calcu(state.root, state.size);

		//叶子父节点缓存
		std::vector<std::pair<Node*, Rect2l>> parents{};
		//递归收集叶子父节点
		leaf_parent_get(parents,&root,reachable_range,tree_range,
			1, max_recur_level,read_only);

		for (auto& [parent_node, parent_range] : parents)
		{
			//一次性取出当前节点下辖所有待取出叶子节点
			for (int recur_direct = Recur_Direct::NW; recur_direct <= Recur_Direct::SE; recur_direct++)
			{
				//计算子节点范围
				Rect2l child_range = child_node_range_calcu(recur_direct, parent_range);

				//若当前叶子节点未在可查询范围内则直接略过
				if (!range_relation_get(reachable_range, child_range))
					continue;
				//若为区块卸载模式
				if (mode == Operate_Mode::Unload)
				{
					//卸载叶子节点本身(子节点槽置空即删除该叶子)
					std::get<0>(parent_node->data)[recur_direct].reset();
					continue;
				}
				//若为其余模式
				//重置子节点指针
				Node* child_node = parent_node;
				//递归子节点
				//若递归失败则查找下一节点
				if (!child_node_recur(child_node, recur_direct, Node_Type::LEAF, read_only, data))
					continue;

				//若为区块查找/获取模式
				if (mode == Operate_Mode::Seek || mode == Operate_Mode::Get)
				{
					//记录查询结果
					//区块中心坐标由 64 位范围求得
					//结果对象与叶子区块数据共享所有权
					receiver.push_back(Tree_Chunk_Data<T>
						(range_center_calcu(child_range), std::get<1>(child_node->data)));
				}
			}
		}
	}
}
