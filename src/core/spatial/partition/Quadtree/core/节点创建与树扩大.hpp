#pragma once
#include "../函数预声明.h"

//展开命名空间
namespace engine
{
	//子节点递归
	template <typename T>
	bool Quadtree<T>::child_node_recur(Node*& this_node, const int& direct, const Node_Type& type, bool stable)
	{
		//若当前节点持有区块数据(分支 1)却需要子节点
		//则先转换为中间节点，原区块数据交由外部持有者释放
		if (this_node->data.index() == 1)
			this_node->data.template emplace<0>(std::array<Node*, 4>{ nullptr, nullptr, nullptr, nullptr });

		//获取当前节点子节点指针列表
		std::array<Node*, 4>& child_list = std::get<0>(this_node->data);
		//若当前子节点为空且为稳定查询模式
		//则为子节点分配内存
		if (child_list[direct] == nullptr && stable == true)
			child_list[direct] = new(std::nothrow) Node(type);

		//递归指针子节点
		this_node = child_list[direct];

		//若子节点为空则返回false
		if (this_node == nullptr)
			return false;
		//若无异常发生则返回true
		else
			return true;
	}

	//四叉树扩大
	template <typename T>
	bool Quadtree<T>::tree_expand(void)
	{
		//分配中间节点内存
		Node* ptr_NW_new = new(std::nothrow) Node(MIDDLE);
		Node* ptr_NE_new = new(std::nothrow) Node(MIDDLE);
		Node* ptr_SW_new = new(std::nothrow) Node(MIDDLE);
		Node* ptr_SE_new = new(std::nothrow) Node(MIDDLE);
		//若存在内存分配失败
		if (ptr_NW_new == nullptr ||
			ptr_NE_new == nullptr ||
			ptr_SW_new == nullptr ||
			ptr_SE_new == nullptr)
		{
			//释放所有内存
			delete ptr_NW_new;
			delete ptr_NE_new;
			delete ptr_SW_new;
			delete ptr_SE_new;
			//放弃四叉树扩大
			return false;
		}
		//若根节点持有区块数据(分支 1)则先转换为中间节点
		//(边长与区块单元相等的退化树扩大时，原根区块不再由树持有)
		if (root.data.index() == 1)
			root.data.template emplace<0>(std::array<Node*, 4>{ nullptr, nullptr, nullptr, nullptr });

		//获取根节点子节点指针列表
		std::array<Node*, 4>& root_child = std::get<0>((*this).root.data);
		//将树原数据链接进中间节点
		//注：由于四叉树是原地扩大
		//所以原来根节点直接管辖的区块间多了层中间节点
		//而方向则在原来的方向上的反方向
		std::get<0>(ptr_NW_new->data)[SE] = root_child[NW];
		std::get<0>(ptr_NE_new->data)[SW] = root_child[NE];
		std::get<0>(ptr_SW_new->data)[NE] = root_child[SW];
		std::get<0>(ptr_SE_new->data)[NW] = root_child[SE];
		//将中间节点链接进根节点
		root_child[NW] = ptr_NW_new;
		root_child[NE] = ptr_NE_new;
		root_child[SW] = ptr_SW_new;
		root_child[SE] = ptr_SE_new;
		//更新四叉树大小
		state.size *= 2;
		//返回扩大成功
		return true;
	}

}
