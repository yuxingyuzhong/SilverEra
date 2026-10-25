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

	//最小区块单元大小设置
	template <typename T>
	bool Quadtree<T>::set_block_size(const uint64_t& size)
	{
		//区块单元须为正的 2 的幂
		if (size == 0 || (size & (size - 1)) != 0)
			return false;
		//树边长须为正的 2 的幂，才能保证逐级对半细分
		if (state.size == 0 || (state.size & (state.size - 1)) != 0)
			return false;
		//区块单元须小于树边长
		if (size >= state.size)
			return false;
		//树边长须为区块单元的整数倍
		if (state.size % size != 0)
			return false;
		//倍率须为 2 的幂
		uint64_t ratio = state.size / size;
		if ((ratio & (ratio - 1)) != 0)
			return false;

		state.block_size = size;
		return true;
	}

	//四叉树边长上限设置
	template <typename T>
	bool Quadtree<T>::set_max_size(const uint64_t& size)
	{
		//上限须不小于当前边长
		if (size < state.size)
			return false;
		//上限与当前边长均须为正的 2 的幂
		if (size == 0 || (size & (size - 1)) != 0)
			return false;
		if (state.size == 0 || (state.size & (state.size - 1)) != 0)
			return false;
		//上限须为当前边长的整数倍
		if (size % state.size != 0)
			return false;
		//倍率须为 2 的幂
		uint64_t ratio = size / state.size;
		if ((ratio & (ratio - 1)) != 0)
			return false;

		state.max_size = size;
		return true;
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
