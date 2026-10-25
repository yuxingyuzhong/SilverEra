#pragma once
#include "../函数预声明.h"

//展开命名空间
namespace engine
{
	//递归级数计算
	template <typename T>
	int Quadtree<T>::depth_calcu(void) const
	{
		//最小区块非正时无法继续细分
		if (state.block_size == 0)
			return 0;

		//寻址总级数计算
		int depth = 0;
		for (uint64_t tree_size = state.size; (tree_size /= 2) >= state.block_size;)
			depth++;
		return depth;
	}

	//点属区块范围计算
	template <typename T>
	Rect2l Quadtree<T>::chunk_range_calcu(const Point2l& target) const
	{
		//把点折叠为零尺寸范围后按区块网格对齐
		Rect2l chunk_range{ target.X, target.X, target.Y, target.Y };
		range_align(chunk_range, state.root, state.block_size);
		return chunk_range;
	}

	//子节点范围计算
	template <typename T>
	Rect2l Quadtree<T>::child_range_calcu(const int& quadrant, const Rect2l& parent_range) const
	{
		//子节点范围存储
		Rect2l child_range{};
		//X轴中间点
		int64_t mid_x = parent_range.left + (parent_range.right - parent_range.left) / 2;
		//Y轴中间点
		int64_t mid_y = parent_range.down + (parent_range.up - parent_range.down) / 2;

		//若子节点向西北递归
		if (quadrant == Quadrant::NW)
		{
			child_range.left = parent_range.left;
			child_range.right = mid_x;
			child_range.up = parent_range.up;
			child_range.down = mid_y + 1;
		}
		//若子节点向东北递归
		else if (quadrant == Quadrant::NE)
		{
			child_range.left = mid_x + 1;
			child_range.right = parent_range.right;
			child_range.up = parent_range.up;
			child_range.down = mid_y + 1;
		}
		//若子节点向西南递归
		else if (quadrant == Quadrant::SW)
		{
			child_range.left = parent_range.left;
			child_range.right = mid_x;
			child_range.up = mid_y;
			child_range.down = parent_range.down;
		}
		//若子节点向东南递归
		else if (quadrant == Quadrant::SE)
		{
			child_range.left = mid_x + 1;
			child_range.right = parent_range.right;
			child_range.up = mid_y;
			child_range.down = parent_range.down;
		}

		return child_range;
	}

	//范围中心计算
	template <typename T>
	Point2d Quadtree<T>::center_calcu(const Rect2l& range) const
	{
		//范围中心坐标存储
		Point2d center{};
		//以差值求中点，避免左右边界相加溢出
		center.X = range.left + (range.right - range.left) / 2.0;
		center.Y = range.down + (range.up - range.down) / 2.0;
		return center;
	}

	//四叉树管理范围计算
	template <typename T>
	Rect2l Quadtree<T>::rect_range_calcu(const Point2d& root, const uint64_t& size) const
	{
		//管理范围存储
		Rect2l rect_range{};
		//四叉树中心均为浮点坐标
		//存在初始0.5偏移需先行将其抵消
		//半跨度按 64 位整数参与运算，避免边长超过 INT_MAX 时溢出
		int64_t half_span = static_cast<int64_t>(size / 2 - 1);
		rect_range.left = static_cast<int64_t>(root.X - 0.5) - half_span;
		rect_range.right = static_cast<int64_t>(root.X + 0.5) + half_span;
		rect_range.down = static_cast<int64_t>(root.Y - 0.5) - half_span;
		rect_range.up = static_cast<int64_t>(root.Y + 0.5) + half_span;
		return rect_range;
	}

	//待查询范围格式化(对齐到区块网格)
	template <typename T>
	void Quadtree<T>::range_align(Rect2l& target_range, const Point2d& root, const uint64_t& block_size) const
	{
		//格式化目的:保证待查询范围严格包含内部区块

		//区块尺寸非正防御
		//区块尺寸为零时下方取模会抛整数除零异常
		if (block_size == 0)
			return;

		//安全取模（处理负数）
		auto mod = [](int64_t a, int64_t b) { int64_t r = a % b; return r < 0 ? r + b : r; };
		//区块尺寸按 64 位整数参与取模
		int64_t size_unit = static_cast<int64_t>(block_size);

		//格式化左边界（向下对齐）
		int64_t left_rem = mod(target_range.left - static_cast<int64_t>(root.X + 0.5), size_unit);
		target_range.left -= left_rem;

		//格式化右边界（向上对齐）
		int64_t right_rem = mod(target_range.right - static_cast<int64_t>(root.X - 0.5), size_unit);
		target_range.right += (right_rem == 0 ? 0 : size_unit - right_rem);

		//格式化上边界（向上对齐）
		int64_t up_rem = mod(target_range.up - static_cast<int64_t>(root.Y - 0.5), size_unit);
		target_range.up += (up_rem == 0 ? 0 : size_unit - up_rem);

		//格式化下边界（向下对齐）
		int64_t down_rem = mod(target_range.down - static_cast<int64_t>(root.Y + 0.5), size_unit);
		target_range.down -= down_rem;
	}

	//范围相交判定
	template <typename T>
	bool Quadtree<T>::range_overlap_check(const Rect2l& target_range, const Rect2l& node_range) const
	{
		//无交集判断：目标整体在节点某一侧（左、右、下、上）
		if (target_range.right < node_range.left || target_range.left > node_range.right ||
			target_range.up < node_range.down || target_range.down > node_range.up)
			return false;
		//剩余情况即为相交
		return true;
	}

	//可查询范围求交(返回越界标记)
	template <typename T>
	Point2d Quadtree<T>::range_intersect(const Rect2l& target_range, Rect2l& reachable_range) const
	{
		//是否申请扩大标记
		Point2d coord_register = state.root;

		//左边界比较
		if (target_range.left < reachable_range.left)
			//重置标记
			coord_register.X = reachable_range.left;
		else
			reachable_range.left = target_range.left;
		//右边界比较
		if (target_range.right > reachable_range.right)
			//重置标记
			coord_register.X = reachable_range.right;
		else
			reachable_range.right = target_range.right;
		//上边界比较
		if (target_range.up > reachable_range.up)
			//重置标记
			coord_register.Y = reachable_range.up;
		else
			reachable_range.up = target_range.up;
		//下边界比较
		if (target_range.down < reachable_range.down)
			//重置标记
			coord_register.Y = reachable_range.down;
		else
			reachable_range.down = target_range.down;

		//返回是否申请扩大标记
		return coord_register;
	}
}
