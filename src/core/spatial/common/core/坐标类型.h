#pragma once
//预编译头
#include "common/前置头文件包含.h"

namespace engine
{
	//精度比较工具
	namespace detail
	{
		//浮点数 ULP 距离计算
		inline uint64_t ulp_distance(double lhs, double rhs) noexcept
		{
			//完全相同直接返回零
			if (lhs == rhs)
				return 0;

			//按位取位模式
			uint64_t lhs_bits = std::bit_cast<uint64_t>(lhs);
			uint64_t rhs_bits = std::bit_cast<uint64_t>(rhs);

			//符号不同视为差值极大
			if ((lhs_bits & 0x8000000000000000ULL) !=
				(rhs_bits & 0x8000000000000000ULL))
				return UINT64_MAX;

			//计算 ULP 数量
			if (lhs_bits > rhs_bits)
				std::swap(lhs_bits, rhs_bits);
			return rhs_bits - lhs_bits;
		}
	}

	//二维点
	template <typename T>
	struct Point2
	{
		//横坐标
		T X = T{};
		//纵坐标
		T Y = T{};

		//默认构造
		Point2() = default;
		//带参构造
		Point2(T x, T y) : X(x), Y(y) {}

		//等于运算符重载
		bool operator==(const Point2& other) const noexcept
		{
			//浮点类型走 ULP 比较
			if constexpr (std::is_floating_point_v<T>)
			{
				//NaN 一律不相等
				if (std::isnan(X) || std::isnan(other.X) ||
					std::isnan(Y) || std::isnan(other.Y))
					return false;
				//无穷按精确比较
				if (std::isinf(X) || std::isinf(other.X) ||
					std::isinf(Y) || std::isinf(other.Y))
					return X == other.X && Y == other.Y;

				//允许四个 ULP 误差
				constexpr uint64_t max_ulp = 4;
				return detail::ulp_distance(X, other.X) <= max_ulp
					&& detail::ulp_distance(Y, other.Y) <= max_ulp;
			}
			//整数类型走精确比较
			else
				return X == other.X && Y == other.Y;
		}
		//不等于运算符重载
		bool operator!=(const Point2& other) const noexcept
		{
			return !(*this == other);
		}

		//输出重载
		friend std::ostream& operator<<(std::ostream& os, const Point2& p)
		{
			os << std::format("X轴坐标: {}\nY轴坐标: {}\n", p.X, p.Y);
			return os;
		}
	};

	//二维点 —— 整数精度
	using Point2i = Point2<int>;
	//二维点 —— 双精度浮点
	using Point2d = Point2<double>;

	//二维矩形范围
	template <typename T>
	struct Rect2
	{
		//左边界
		T left = T{};
		//右边界
		T right = T{};
		//上边界
		T up = T{};
		//下边界
		T down = T{};

		//默认构造
		Rect2() = default;
		//带参构造
		Rect2(T l, T r, T u, T d) : left(l), right(r), up(u), down(d) {}

		//等于运算符重载
		bool operator==(const Rect2& other) const noexcept
		{
			return left == other.left && right == other.right &&
				up == other.up && down == other.down;
		}
		//不等于运算符重载
		bool operator!=(const Rect2& other) const noexcept
		{
			return !(*this == other);
		}

		//输出重载
		friend std::ostream& operator<<(std::ostream& os, const Rect2& r)
		{
			os << std::format("左边界: {}\n右边界: {}\n上边界: {}\n下边界: {}\n",
				r.left, r.right, r.up, r.down);
			return os;
		}
	};

	//二维矩形范围 —— 整数精度
	using Rect2i = Rect2<int>;
	//二维矩形范围 —— 双精度浮点
	using Rect2d = Rect2<double>;

	//精度转换 —— 浮点转整数
	inline Point2i point_to_int(const Point2d& p) noexcept
	{
		return { static_cast<int>(std::lround(p.X)),
			static_cast<int>(std::lround(p.Y)) };
	}

	//精度转换 —— 整数转浮点
	inline Point2d point_to_double(const Point2i& p) noexcept
	{
		return { static_cast<double>(p.X),
			static_cast<double>(p.Y) };
	}
}
