#pragma once
#include "../函数预声明.h"

//展开命名空间
namespace engine
{
	//构造函数
	template <typename T>
	Quadtree<T>::Quadtree(const uint64_t& size, const Point2d& root)
	{
		state.size = size;
		state.root = root;
	}

	//析构函数
	template <typename T>
	Quadtree<T>::~Quadtree(void)
	{
		//子节点由根节点持有的 shared_ptr 链自动递归释放，无需手工回收
	}

	//最小区块单元大小设置
	template <typename T>
	bool Quadtree<T>::set_block_size(const uint64_t& size)
	{
		//若最小区块单元小于四叉树当前大小则设置可行
		if (size < state.size)
		{
			state.block_size = size;
			return true;
		}
		else
			return false;
	}

	//四叉树边长上限设置
	template <typename T>
	bool Quadtree<T>::set_max_size(const uint64_t& size)
	{
		//若四叉树边长上限大于四叉树当前大小则设置可行
		if (size > state.size)
		{
			state.max_size = size;
			return true;
		}
		else
			return false;
	}

	//四叉树回调管理方法设置
	template <typename T>
	void Quadtree<T>::set_callback_manage
	(const std::function<bool(Point2d root, Point2l target)>& cb)
	{
		//回调管理函数注册
		callback = cb;
	}

	//四叉树状态获取
	template <typename T>
	const Tree_State& Quadtree<T>::tree_state_get(void) const
	{
		return state;
	}

}

