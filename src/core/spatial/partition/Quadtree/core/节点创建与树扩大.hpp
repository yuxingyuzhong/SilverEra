#pragma once
#include "../函数预声明.h"

//展开命名空间
namespace engine
{
	//子节点推进
	template <typename T>
	bool Quadtree<T>::child_step(Node*& this_node, const int& quadrant, const Node_Kind& kind,
		bool read_only, const std::shared_ptr<T>& external_data)
	{
		//当前节点必须是中间节点
		//否则拒绝推进，避免把已有叶子数据丢弃
		if (this_node->data.index() != 0)
			return false;

		//获取当前节点子节点列表
		std::array<std::shared_ptr<Node>, 4>& child_list = std::get<0>(this_node->data);

		//若目标子节点为空
		if (child_list[quadrant] == nullptr)
		{
			//只读操作不分配节点
			if (read_only)
				return false;
			//分配目标子节点
			child_list[quadrant].reset(new(std::nothrow) Node(kind, external_data));
			//若分配失败则推进失败
			if (child_list[quadrant] == nullptr)
				return false;
		}

		//获取目标子节点观察指针
		Node* child_node = child_list[quadrant].get();

		//若目标子节点种类与预期不符则拒绝推进
		//这样只读查询不会把浅层叶子改写成中间节点
		if (kind == Node_Kind::Leaf && child_node->data.index() != 1)
			return false;
		if (kind == Node_Kind::Middle && child_node->data.index() != 0)
			return false;

		//推进至目标子节点
		this_node = child_node;
		return true;
	}

	//四叉树扩大
	template <typename T>
	bool Quadtree<T>::tree_expand(void)
	{
		//已达边长上限则拒绝扩大
		if (state.max_size != 0 && state.size >= state.max_size)
			return false;
		//根节点必须是中间节点，否则无法原地加层
		if (root.data.index() != 0)
			return false;

		//分配四个新中间节点
		std::shared_ptr<Node> ptr_NW_new(new(std::nothrow) Node(Node_Kind::Middle));
		std::shared_ptr<Node> ptr_NE_new(new(std::nothrow) Node(Node_Kind::Middle));
		std::shared_ptr<Node> ptr_SW_new(new(std::nothrow) Node(Node_Kind::Middle));
		std::shared_ptr<Node> ptr_SE_new(new(std::nothrow) Node(Node_Kind::Middle));
		//若存在内存分配失败则放弃扩大
		if (ptr_NW_new == nullptr || ptr_NE_new == nullptr ||
			ptr_SW_new == nullptr || ptr_SE_new == nullptr)
			return false;

		//获取根节点子节点列表
		std::array<std::shared_ptr<Node>, 4>& root_child = std::get<0>(root.data);
		//将树原子树链接进新中间节点
		//注意方向为原方向的相反方向
		std::get<0>(ptr_NW_new->data)[SE] = root_child[NW];
		std::get<0>(ptr_NE_new->data)[SW] = root_child[NE];
		std::get<0>(ptr_SW_new->data)[NE] = root_child[SW];
		std::get<0>(ptr_SE_new->data)[NW] = root_child[SE];
		//将新中间节点链接进根节点
		root_child[NW] = ptr_NW_new;
		root_child[NE] = ptr_NE_new;
		root_child[SW] = ptr_SW_new;
		root_child[SE] = ptr_SE_new;
		//更新四叉树大小
		state.size *= 2;
		return true;
	}
}
