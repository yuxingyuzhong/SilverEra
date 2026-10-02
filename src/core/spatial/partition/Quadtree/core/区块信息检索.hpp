#pragma once
#include "../函数预声明.h"

//展开命名空间
namespace engine
{
	//单点查询可行性分析
	template <typename T>
	Quadtree<T>::Analysis_Result Quadtree<T>::seekable_analyse(const Point2l& target, bool read_only)
	{
		//简化表示路径
		auto& root = state.root;
		//四叉树管理范围存储
		Rect2l tree_range{};
		//计算四叉树管理范围
		manage_range_calcu(tree_range, state.root, state.size);

		//判断坐标大小是否超出树
		//若坐标大小超出树则进行下一步检测
		if (target.X < tree_range.left || target.X > tree_range.right
			|| target.Y > tree_range.up || target.Y < tree_range.down)
		{
			//若当前为只读查找模式则查找不可行
			if (read_only)
				return Analysis_Result::INFEASIBLE;

			//若当前四叉树大小以及大于等于上限大小则查找不可行
			if (state.size >= state.max_size)
			{
				//若存在回调管理函数则报告上级
				if (callback)
					callback(root, target);
				//返回分析终止
				return Analysis_Result::INFEASIBLE;
			}
			//若当前四叉树大小小于上限大小则寻址可能可行
			else if (state.size < state.max_size)
			{
				//若存在回调管理函数则请求扩大权限
				if (callback)
					//若扩大申请未通过则返回分析终止
					if (!callback(root, target))
						return Analysis_Result::INFEASIBLE;

				//进行四叉树扩大操作
				//若四叉树扩大成功则进行下一步操作
				if (tree_expand())
				{
					//重新计算四叉树管理范围
					manage_range_calcu(tree_range, state.root, state.size);

					//重新比较四叉树是否已经包含待查找位置
					//若未包含则返回分析持续
					if (target.X < tree_range.left || target.X > tree_range.right
						|| target.Y > tree_range.up || target.Y < tree_range.down)
						return Analysis_Result::IN_PROGRESS;
					//若已包含则返回返回查找可行
					else
						return Analysis_Result::FEASIBLE;
				}
				//若未成功则直接返回分析终止
				else
					return Analysis_Result::INFEASIBLE;
			}
		}
		//若坐标大小未超出树则直接返回查找可行
		else
			return Analysis_Result::FEASIBLE;

	}

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
		node_operate(target,Operate_Mode::Build,temp,data);
	}

	//区块构建 —— 范围重载(不可直接挂载数据)
	template <typename T>
	void Quadtree<T>::build(const Rect2l& target_range)
	{
		//参数填充临时对象
		std::vector<std::shared_ptr<Tree_Chunk_Data<T>>> temp;
		//启动节点创建模式
		node_operate(target_range,Operate_Mode::Build,temp);
	}

	//区块查找 —— 单区块重载
	template <typename T>
	void Quadtree<T>::seek(const Point2l& target, std::shared_ptr<Tree_Chunk_Data<T>>& receiver)
	{
		//启动节点查找模式
		node_operate(target, Operate_Mode::Seek,receiver);
	}

	//区块查找 —— 范围重载
	template <typename T>
	void Quadtree<T>::seek(const Rect2l& target_range,
		std::vector<std::shared_ptr<Tree_Chunk_Data<T>>>& receiver)
	{
		//启动节点查找模式
		node_operate(target_range, Operate_Mode::Seek,receiver);
	}

	//区块获取 —— 单区块重载
	template <typename T>
	void Quadtree<T>::get(const Point2l& target, std::shared_ptr<Tree_Chunk_Data<T>>& receiver)
	{
		//启动节点创建模式
		node_operate(target, Operate_Mode::Get, receiver);
	}

	//区块获取 —— 范围重载
	template <typename T>
	void Quadtree<T>::get(const Rect2l& target_range,
		std::vector<std::shared_ptr<Tree_Chunk_Data<T>>>& receiver)
	{
		//启动节点查找模式
		node_operate(target_range, Operate_Mode::Get, receiver);
	}

	//区块卸载 —— 单区块重载
	template <typename T>
	void Quadtree<T>::unload(const Point2l& target)
	{
		//参数填充临时对象
		std::shared_ptr<Tree_Chunk_Data<T>> temp;
		//启动节点卸载模式
		node_operate(target, Operate_Mode::Unload,temp);
	}

	//区块卸载 —— 范围重载
	template <typename T>
	void Quadtree<T>::unload(const Rect2l& target_range)
	{
		//参数填充临时对象
		std::vector<std::shared_ptr<Tree_Chunk_Data<T>>> temp;
		//启动节点卸载模式
		node_operate(target_range, Operate_Mode::Unload,temp);
	}

	//叶子收集
	template <typename T>
	void Quadtree<T>::leaf_collect(const Rect2l& target_range, const Rect2l& parent_range, Operate_Mode mode,
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
					//获取待卸载叶子节点
					Node* leaf_node = std::get<0>(parent_node->data)[recur_direct];
					//释放叶子节点持有的区块数据
					std::get<1>(leaf_node->data).reset();
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
						leaf_collect(target_range, child_range, mode,
							now_level + 1, max_level, child_node, receiver);
				}
			}
		}
	}

	//节点操作 —— 单区块重载
	template <typename T>
	void Quadtree<T>::node_operate(const Point2l& target, Operate_Mode mode,
		std::shared_ptr<Tree_Chunk_Data<T>>& receiver,
		const std::shared_ptr<T>& data)
	{
		//默认非只读操作
		bool read_only = false;
		//若当前为区块查找/卸载模式
		if (mode == Operate_Mode::Seek ||mode == Operate_Mode::Unload)
			read_only = true;

		//最大检测次数存储
		int check_time_max = 1;
		//计算最大检测次数
		for (uint64_t max_size = state.max_size; (max_size /= 2) / state.size > 1;)
			check_time_max++;
		//循环检测查找是否可行
		//循环次数保证理想情况下四叉树可扩大到最大
		//额外次数保证可能存在的管理层知晓查询失败信息
		for (int check_time = 0; check_time < check_time_max; check_time++)
		{
			//获取下一步分析方案
			Analysis_Result next_step = seekable_analyse(target, read_only);
			//若查找可行则直接结束检测
			if (next_step == Analysis_Result::FEASIBLE)
				break;
			//若查找可能可行则继续
			else if (next_step == Analysis_Result::IN_PROGRESS)
				continue;
			//若查找不可行则直接返回
			else if (next_step == Analysis_Result::INFEASIBLE)
				//返回给上层调用者
				return;
		}

		//根节点寻址总级数声明
		int recur_level_max = 0;
		//递归总级数计算
		recur_level_calcu(recur_level_max);

		//获取根节点指针
		Node* child_node = &root;
		//节点管理范围存储
		Rect2l node_range{};
		//初始化为四叉树管理范围
		manage_range_calcu(node_range, state.root, state.size);
		//路径递归方向标记存储
		int recur_direct = 0;

		//开始区块检索
		for (int recur_level_now = 0; recur_level_now < recur_level_max; recur_level_now++)
		{
			//计算递归方向
			recur_direct_calcu(target, node_range, recur_direct);
			//递归子节点
			//若当前不为最后一级则创建中间节点
			if (recur_level_now < recur_level_max - 1)
				child_node_recur(child_node, recur_direct, Node_Type::MIDDLE, read_only);
			//若当前为最后一级递归则创建叶子节点
			else
			{
				//若为区块构建模式
				if (mode == Operate_Mode::Build)
				{
					//获取当前节点子节点指针列表
					std::array<Node*, 4>& child_list = std::get<0>(child_node->data);
					//挂载外部数据
					child_list[recur_direct] = new(std::nothrow) Node(Node_Type::LEAF, data);
					return;
				}
				//若为区块卸载模式
				else if (mode == Operate_Mode::Unload)
				{
					//获取待卸载叶子节点
					Node* leaf_node = std::get<0>(child_node->data)[recur_direct];
					//释放叶子节点持有的区块数据
					std::get<1>(leaf_node->data).reset();
					return;
				}
				//若为区块查找/获取模式
				else
					child_node_recur(child_node, recur_direct, Node_Type::LEAF, read_only);
			}

			//若内存分配失败则直接返回
			if (child_node == nullptr)
				return;

			//存储旧范围值
			Rect2l old_range = node_range;
			//计算新范围值
			child_node_range_calcu(recur_direct, node_range, old_range);
		}

		//若为区块查找/获取模式
		if (mode == Operate_Mode::Seek || mode == Operate_Mode::Get)
		{
			//若接收器为空则分配结果对象
			if (receiver == nullptr)
				receiver.reset(new(std::nothrow) Tree_Chunk_Data<T>);
			//若内存分配失败则返回
			if (receiver == nullptr)
				return;

			//记录查询结果
			//结果对象与叶子区块数据共享所有权
			receiver->ptr_data = std::get<1>(child_node->data);
			//范围边界为 64 位整数，故以双精度求中点避免精度损失
			receiver->node.X = static_cast<double>(node_range.left + node_range.right) / 2.0;
			receiver->node.Y = static_cast<double>(node_range.down + node_range.up) / 2.0;
		}
	}

	//节点操作 —— 范围区块重载
	template <typename T>
	void Quadtree<T>::node_operate(const Rect2l& target_range, Operate_Mode mode,
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
		leaf_collect(seekable_range,parent_range,mode,
			1, recur_level_max,parent_node,receiver);
	}
}
