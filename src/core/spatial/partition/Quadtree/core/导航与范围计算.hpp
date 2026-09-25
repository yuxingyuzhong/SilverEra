#pragma once
#include "../函数预声明.h"

//展开命名空间
namespace engine
{
	//递归级数计算
	template <typename T>
	void Quadtree<T>::recur_level_calcu(int& address_series)
	{
		//寻址总级数计算
		for (uint64_t tree_size = state.size; (tree_size /= 2) >= state.block_size;)
			address_series++;

		//PS:从根节点开始向下递归时
		   //若当前指针已指向最小区块单元大小的区域
		   //此时已经为该指针分配内存
		   //递归结束
	}

	//递归方向计算
	template <typename T>
	void Quadtree<T>::recur_direct_calcu(const Point2l& target, const Rect2l& node, int& recur_direct)
	{
		//因节点坐标为浮点坐标
		//而查找坐标为整数坐标
		//因而不存在坐标相等情况

		//计算节点根坐标
		//节点范围边界已达 64 位
		//故以双精度求中点，避免单精度在小数位上的精度损失
		Point2d node_coord = {};
		node_coord.X = (node.left + node.right) / 2.0;
		node_coord.Y = (node.down + node.up) / 2.0;

		//西北方向
		if (target.X < node_coord.X && target.Y > node_coord.Y)
			recur_direct = NW;
		//东北方向
		else if (target.X > node_coord.X && target.Y > node_coord.Y)
			recur_direct = NE;
		//西南方向
		else if (target.X < node_coord.X && target.Y < node_coord.Y)
			recur_direct = SW;
		//东南方向
		else if (target.X > node_coord.X && target.Y < node_coord.Y)
			recur_direct = SE;
	}

	//子节点范围计算
	template <typename T>
	void Quadtree<T>::child_node_range_calcu(const int& recur_direct, Rect2l& child_range,
		const Rect2l& parent_range)
	{
		// 先计算正确的中间分割点（避免负数向零截断问题）
		// X轴中间点 = left + (right - left) / 2  (right - left 恒为正)
		int64_t mid_x = parent_range.left + (parent_range.right - parent_range.left) / 2;
		// Y轴中间点 = down + (up - down) / 2      (up - down 恒为正)
		int64_t mid_y = parent_range.down + (parent_range.up - parent_range.down) / 2;

		//若子节点向西北递归
		if (recur_direct == NW)
		{
			child_range.left = parent_range.left;
			child_range.right = mid_x;
			child_range.up = parent_range.up;
			child_range.down = mid_y + 1;
		}
		//若子节点向东北递归
		else if (recur_direct == NE)
		{
			child_range.left = mid_x + 1;
			child_range.right = parent_range.right;
			child_range.up = parent_range.up;
			child_range.down = mid_y + 1;
		}
		//若子节点向西南递归
		else if (recur_direct == SW)
		{
			child_range.left = parent_range.left;
			child_range.right = mid_x;
			child_range.up = mid_y;
			child_range.down = parent_range.down;
		}
		//若子节点向东南递归
		else if (recur_direct == SE)
		{
			child_range.left = mid_x + 1;
			child_range.right = parent_range.right;
			child_range.up = mid_y;
			child_range.down = parent_range.down;
		}
	}

	//四叉树管理范围计算
	template <typename T>
	void Quadtree<T>::manage_range_calcu(Rect2l& receiver, const Point2d& root, const uint64_t tree_size)
	{
		//四叉树中心均为浮点坐标
		//存在初始0.5偏移需先行将其抵消
		//半跨度按 64 位整数参与运算，避免边长超过 INT_MAX 时溢出
		int64_t half_span = static_cast<int64_t>(tree_size / 2 - 1);
		receiver.left = static_cast<int64_t>(root.X - 0.5) - half_span;
		receiver.right = static_cast<int64_t>(root.X + 0.5) + half_span;
		receiver.down = static_cast<int64_t>(root.Y - 0.5) - half_span;
		receiver.up = static_cast<int64_t>(root.Y + 0.5) + half_span;
	}

	//待查询范围格式化
	template <typename T>
	void Quadtree<T>::target_range_format(Rect2l& target_range, const Point2d& root, const uint64_t block_size)
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

	//待查询范围比较
	template <typename T>
	bool Quadtree<T>::range_relation_get(const Rect2l& target_range, const Rect2l& node_range)
	{
		//简化表示路径
		auto& t_left = target_range.left;
		auto& t_right = target_range.right;
		auto& t_up = target_range.up;
		auto& t_down = target_range.down;
		//简化表示路径
		auto& n_left = node_range.left;
		auto& n_right = node_range.right;
		auto& n_up = node_range.up;
		auto& n_down = node_range.down;

		//无交集判断：目标整体在节点某一侧（左、右、下、上）
		if (t_right < n_left || t_left > n_right ||
			t_up < n_down || t_down > n_up)
			return false;
		//剩余情况即为相交
		else
			return true;
	}

	//可查询范围计算
	template <typename T>
	Point2d Quadtree<T>::seekable_range_calcu(const Rect2l& target_range, Rect2l& seekable_range)
	{
		//是否申请扩大标记
		Point2d coord_register = state.root;

		//左边界比较
		if (target_range.left < seekable_range.left)
			//重置标记
			coord_register.X = seekable_range.left;
		else
			seekable_range.left = target_range.left;
		//右边界比较
		if (target_range.right > seekable_range.right)
			//重置标记
			coord_register.X = seekable_range.right;
		else
			seekable_range.right = target_range.right;
		//上边界比较
		if (target_range.up > seekable_range.up)
			//重置标记
			coord_register.Y = seekable_range.up;
		else
			seekable_range.up = target_range.up;
		//下边界比较
		if (target_range.down < seekable_range.down)
			//重置标记
			coord_register.Y = seekable_range.down;
		else
			seekable_range.down = target_range.down;

		//返回是否申请扩大标记
		return coord_register;
	}

}

