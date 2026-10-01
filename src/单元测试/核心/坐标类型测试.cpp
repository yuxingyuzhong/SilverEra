//坐标类型测试：覆盖二维点与二维矩形的构造、精度比较与精度转换
#include <gtest/gtest.h>

//四舍五入与浮点分类
#include <cmath>
//浮点边界值
#include <limits>

//获取坐标类型
#include "src/core/spatial/common/core/坐标类型.h"

//坐标类型测试夹具
class Coord_Type_Test : public ::testing::Test
{
};

// ———— 二维点：构造与读写 ————

//整数点：默认构造为零点
TEST_F(Coord_Type_Test, 整数点默认构造为零点)
{
	//默认构造的整数点
	engine::Point2i point;
	//两个分量都应为零
	EXPECT_EQ(point.X, 0);
	EXPECT_EQ(point.Y, 0);
}

//整数点：带参构造按顺序写入分量
TEST_F(Coord_Type_Test, 整数点带参构造按顺序写入分量)
{
	//带参构造的整数点
	engine::Point2i point(3, -7);
	//横坐标为第一个参数
	EXPECT_EQ(point.X, 3);
	//纵坐标为第二个参数
	EXPECT_EQ(point.Y, -7);
}

//整数点：相同分量精确相等、任一不同即不等
TEST_F(Coord_Type_Test, 整数点精确相等)
{
	//分量完全相同的两个点
	EXPECT_TRUE((engine::Point2i(3, -7) == engine::Point2i(3, -7)));
	//横坐标不同
	EXPECT_TRUE((engine::Point2i(3, -7) != engine::Point2i(4, -7)));
	//纵坐标不同
	EXPECT_TRUE((engine::Point2i(3, -7) != engine::Point2i(3, -6)));
}

// ———— 二维点：浮点精度比较 ————

//浮点点：完全相同的值视为相等
TEST_F(Coord_Type_Test, 浮点点完全相同视为相等)
{
	//同一个浮点点
	engine::Point2d point(1.5, -2.25);
	//按分量重建后应判相等
	EXPECT_TRUE(point == engine::Point2d(1.5, -2.25));
}

//浮点点：四个ULP以内视为相等
TEST_F(Coord_Type_Test, 浮点点四个ULP以内视为相等)
{
	//基准值
	const double base = 1.0;
	//向正方向步进四个可表示值
	double stepped = base;
	for (int step = 0; step < 4; step++)
		stepped = std::nextafter(stepped, 1e308);
	//四个ULP以内应判相等
	EXPECT_TRUE((engine::Point2d(base, base) == engine::Point2d(stepped, stepped)));
}

//浮点点：超过四个ULP即不相等
TEST_F(Coord_Type_Test, 浮点点超过四个ULP即不相等)
{
	//基准值
	const double base = 1.0;
	//向正方向步进五个可表示值
	double stepped = base;
	for (int step = 0; step < 5; step++)
		stepped = std::nextafter(stepped, 1e308);
	//超出四个ULP应判不等
	EXPECT_TRUE((engine::Point2d(base, base) != engine::Point2d(stepped, stepped)));
}

//浮点点：正零与负零视为相等
TEST_F(Coord_Type_Test, 浮点点正零与负零相等)
{
	//零与负零数值相同，应判相等
	EXPECT_TRUE((engine::Point2d(0.0, 0.0) == engine::Point2d(-0.0, -0.0)));
}

//浮点点：符号不同即不相等
TEST_F(Coord_Type_Test, 浮点点符号不同即不相等)
{
	//横坐标符号相反
	EXPECT_TRUE((engine::Point2d(1.0, 1.0) != engine::Point2d(-1.0, 1.0)));
	//纵坐标符号相反
	EXPECT_TRUE((engine::Point2d(1.0, 1.0) != engine::Point2d(1.0, -1.0)));
}

//浮点点：含NaN时一律不相等
TEST_F(Coord_Type_Test, 浮点点含NaN时一律不相等)
{
	//非法浮点值
	const double nan_value = std::numeric_limits<double>::quiet_NaN();
	//两侧都是NaN也不相等
	EXPECT_FALSE((engine::Point2d(nan_value, nan_value) == engine::Point2d(nan_value, nan_value)));
	//仅纵坐标为NaN
	EXPECT_FALSE((engine::Point2d(1.0, nan_value) == engine::Point2d(1.0, 1.0)));
}

//浮点点：无穷按精确比较
TEST_F(Coord_Type_Test, 浮点点无穷按精确比较)
{
	//正无穷
	const double inf_value = std::numeric_limits<double>::infinity();
	//同位无穷应相等
	EXPECT_TRUE((engine::Point2d(inf_value, 0.0) == engine::Point2d(inf_value, 0.0)));
	//分量不同即不等
	EXPECT_TRUE((engine::Point2d(inf_value, 0.0) != engine::Point2d(inf_value, 1.0)));
	//正负无穷不等
	EXPECT_TRUE((engine::Point2d(inf_value, 0.0) != engine::Point2d(-inf_value, 0.0)));
}

// ———— 二维矩形：四条边界语义 ————

//整数矩形：默认构造四条边界均为零
TEST_F(Coord_Type_Test, 整数矩形默认构造四条边界均为零)
{
	//默认构造的整数矩形
	engine::Rect2i range;
	//四条边界都应为零
	EXPECT_EQ(range.left, 0);
	EXPECT_EQ(range.right, 0);
	EXPECT_EQ(range.up, 0);
	EXPECT_EQ(range.down, 0);
}

//整数矩形：带参构造依次写入左、右、上、下
TEST_F(Coord_Type_Test, 整数矩形带参构造依次写入四条边界)
{
	//按 左、右、上、下 的顺序构造
	engine::Rect2i range(1, 2, 3, 4);
	//左边界为第一个参数
	EXPECT_EQ(range.left, 1);
	//右边界为第二个参数
	EXPECT_EQ(range.right, 2);
	//上边界为第三个参数
	EXPECT_EQ(range.up, 3);
	//下边界为第四个参数
	EXPECT_EQ(range.down, 4);
}

//整数矩形：四条边界全部相同才算相等
TEST_F(Coord_Type_Test, 整数矩形四条边界全部相同才算相等)
{
	//基准矩形
	const engine::Rect2i base(1, 2, 3, 4);
	//四条边界全部相同
	EXPECT_TRUE(base == engine::Rect2i(1, 2, 3, 4));
	//依次改变其中一条边界，都应判不等
	EXPECT_TRUE(base != engine::Rect2i(9, 2, 3, 4));
	EXPECT_TRUE(base != engine::Rect2i(1, 9, 3, 4));
	EXPECT_TRUE(base != engine::Rect2i(1, 2, 9, 4));
	EXPECT_TRUE(base != engine::Rect2i(1, 2, 3, 9));
}

//整数矩形：四条边界可独立改写
TEST_F(Coord_Type_Test, 整数矩形边界可独立改写)
{
	//默认构造的整数矩形
	engine::Rect2i range;
	//逐条改写边界
	range.left = -5;
	range.right = 5;
	range.up = 7;
	range.down = -7;
	//改写后应逐条读回
	EXPECT_EQ(range.left, -5);
	EXPECT_EQ(range.right, 5);
	EXPECT_EQ(range.up, 7);
	EXPECT_EQ(range.down, -7);
}

// ———— 64 位坐标：Point2l / Rect2l ————

//64 位点：默认构造为零点、带参构造按顺序写入
TEST_F(Coord_Type_Test, 六十四位点构造与读写)
{
	//默认构造的 64 位点
	engine::Point2l zero;
	//两个分量都应为零
	EXPECT_EQ(zero.X, 0);
	EXPECT_EQ(zero.Y, 0);
	//带参构造的 64 位点
	engine::Point2l point(3, -7);
	//横坐标为第一个参数
	EXPECT_EQ(point.X, 3);
	EXPECT_EQ(point.Y, -7);
	//改写后应能逐条读回
	point.X = -9;
	EXPECT_EQ(point.X, -9);
}

//64 位点：可承载超出 int 表示范围的分量
TEST_F(Coord_Type_Test, 六十四位点承载超int边界值)
{
	//2^33 已超出 int 的表示范围
	const int64_t beyond = 1ll << 33;
	//带参构造写入超 int 分量
	engine::Point2l point(beyond, -beyond);
	//分量应按 64 位原值读回，不被截断
	EXPECT_EQ(point.X, beyond);
	EXPECT_EQ(point.Y, -beyond);
	//int 最大值与它之外的下一个整数
	const int64_t int_max = (1ll << 31) - 1;
	//超出 int 表示范围的值同样可承载
	EXPECT_EQ((engine::Point2l(int_max + 1, 0).X), int_max + 1);
}

//64 位矩形：默认构造为零、四边可承载超 int 边界
TEST_F(Coord_Type_Test, 六十四位矩形承载超int边界值)
{
	//默认构造的 64 位矩形
	engine::Rect2l zero;
	//四条边界都应为零
	EXPECT_EQ(zero.left, 0);
	EXPECT_EQ(zero.right, 0);
	EXPECT_EQ(zero.up, 0);
	EXPECT_EQ(zero.down, 0);
	//按 左、右、上、下 的顺序构造超大范围
	const int64_t beyond = 1ll << 33;
	engine::Rect2l range(-beyond, beyond - 1, beyond - 1, -beyond);
	//四条边界都应按 64 位原值读回
	EXPECT_EQ(range.left, -beyond);
	EXPECT_EQ(range.right, beyond - 1);
	EXPECT_EQ(range.up, beyond - 1);
	EXPECT_EQ(range.down, -beyond);
	//宽度应按 64 位计算，不截断
	EXPECT_EQ(range.right - range.left + 1, beyond * 2);
}

// ———— 精度转换 ————

//浮点转整数：按四舍五入取整
TEST_F(Coord_Type_Test, 浮点转整数按四舍五入取整)
{
	//小数部分不足一半向下取整
	EXPECT_EQ(engine::point_to_int(engine::Point2d(1.4, 2.4)), (engine::Point2i(1, 2)));
	//小数部分超过一半向上取整
	EXPECT_EQ(engine::point_to_int(engine::Point2d(1.6, 2.6)), (engine::Point2i(2, 3)));
}

//浮点转整数：半数远离零
TEST_F(Coord_Type_Test, 浮点转整数半数远离零)
{
	//正半数向正方向进位
	EXPECT_EQ(engine::point_to_int(engine::Point2d(2.5, 0.5)), (engine::Point2i(3, 1)));
	//负半数向负方向进位
	EXPECT_EQ(engine::point_to_int(engine::Point2d(-2.5, -0.5)), (engine::Point2i(-3, -1)));
}

//整数转浮点：分量精确提升
TEST_F(Coord_Type_Test, 整数转浮点分量精确提升)
{
	//整数点提升为浮点点
	EXPECT_EQ(engine::point_to_double(engine::Point2i(3, -4)), (engine::Point2d(3.0, -4.0)));
}

//精度转换：整数经浮点往返保持原值
TEST_F(Coord_Type_Test, 精度转换往返保持原值)
{
	//原始整数点
	const engine::Point2i origin(7, -9);
	//先转浮点再转回整数
	EXPECT_EQ(engine::point_to_int(engine::point_to_double(origin)), origin);
}

//64 位浮点转整数：与 point_to_int 同口径取整，且不丢超 int 分量
TEST_F(Coord_Type_Test, 六十四位浮点转整数按四舍五入取整)
{
	//小数部分不足一半向下取整
	EXPECT_EQ(engine::point_to_l(engine::Point2d(1.4, 2.4)), (engine::Point2l(1, 2)));
	//小数部分超过一半向上取整
	EXPECT_EQ(engine::point_to_l(engine::Point2d(1.6, 2.6)), (engine::Point2l(2, 3)));
	//半数远离零
	EXPECT_EQ(engine::point_to_l(engine::Point2d(2.5, 0.5)), (engine::Point2l(3, 1)));
	EXPECT_EQ(engine::point_to_l(engine::Point2d(-2.5, -0.5)), (engine::Point2l(-3, -1)));
	//超出 int 的整数值应完整落到 64 位分量
	EXPECT_EQ(engine::point_to_l(engine::Point2d(4294967296.0, -4294967296.0)),
		(engine::Point2l(1ll << 32, -(1ll << 32))));
}

//64 位精度转换：经浮点往返保持原值
TEST_F(Coord_Type_Test, 六十四位精度转换往返保持原值)
{
	//原始 64 位点（分量超出 int 表示范围）
	const engine::Point2l origin(1ll << 33, -(1ll << 33));
	//先转浮点再转回 64 位整数
	EXPECT_EQ(engine::point_to_l(engine::point_to_double(origin)), origin);
	//未超 int 的分量，往返结果应与原有 32 位路径一致
	const engine::Point2i small_int(7, -9);
	const engine::Point2l small_long = engine::point_to_l(engine::point_to_double(engine::Point2l(7, -9)));
	const engine::Point2i small_round = engine::point_to_int(engine::point_to_double(small_int));
	//逐分量比对两条路径
	EXPECT_EQ(small_long.X, static_cast<int64_t>(small_round.X));
	EXPECT_EQ(small_long.Y, static_cast<int64_t>(small_round.Y));
}

// ———— 精度比较工具 ————

//ULP距离：相同值距离为零
TEST_F(Coord_Type_Test, ULP距离相同值距离为零)
{
	//同一个值
	EXPECT_EQ(engine::detail::ulp_distance(1.25, 1.25), 0u);
	//正零与负零数值相同
	EXPECT_EQ(engine::detail::ulp_distance(0.0, -0.0), 0u);
}

//ULP距离：相邻可表示值距离为一
TEST_F(Coord_Type_Test, ULP距离相邻可表示值距离为一)
{
	//基准值与其下一个可表示值之间恰好相差一个ULP
	EXPECT_EQ(engine::detail::ulp_distance(1.0, std::nextafter(1.0, 1e308)), 1u);
}

//ULP距离：符号不同返回极大值
TEST_F(Coord_Type_Test, ULP距离符号不同返回极大值)
{
	//符号相反的两个值视为差值极大
	EXPECT_EQ(engine::detail::ulp_distance(1.0, -1.0), UINT64_MAX);
}

// ———— 二维矩形：浮点相等语义 ————

//浮点矩形：相等判定按逐分量精确比较，不设ULP容差
TEST_F(Coord_Type_Test, 浮点矩形相等不采用ULP容差)
{
	//基准浮点矩形
	const engine::Rect2d base(1.0, 2.0, 3.0, 4.0);
	//分量完全相同的矩形应判相等
	EXPECT_TRUE(base == engine::Rect2d(1.0, 2.0, 3.0, 4.0));
	//将上边界步进一个ULP
	const double stepped = std::nextafter(3.0, 1e308);
	//仅差一个ULP也应判不等
	EXPECT_TRUE(base != engine::Rect2d(1.0, 2.0, stepped, 4.0));
}

//浮点矩形：任一边界不同即判不等
TEST_F(Coord_Type_Test, 矩形浮点不等运算)
{
	//基准浮点矩形
	const engine::Rect2d base(1.5, 2.5, 3.5, 4.5);
	//分量完全相同的矩形应判相等
	EXPECT_TRUE(base == engine::Rect2d(1.5, 2.5, 3.5, 4.5));
	//依次改变其中一条边界，都应判不等
	EXPECT_TRUE(base != engine::Rect2d(9.5, 2.5, 3.5, 4.5));
	EXPECT_TRUE(base != engine::Rect2d(1.5, 9.5, 3.5, 4.5));
	EXPECT_TRUE(base != engine::Rect2d(1.5, 2.5, 9.5, 4.5));
	EXPECT_TRUE(base != engine::Rect2d(1.5, 2.5, 3.5, 9.5));
}

// ———— 二维点：赋值与输出 ————

//整数点：拷贝赋值后与原对象相等
TEST_F(Coord_Type_Test, 整数点赋值后相等)
{
	//源点
	const engine::Point2i source(5, -6);
	//默认构造的接收点
	engine::Point2i copy;
	//执行拷贝赋值
	copy = source;
	//赋值后两对象应判相等
	EXPECT_TRUE(copy == source);
	//分量应按原值拷贝
	EXPECT_EQ(copy.X, 5);
	EXPECT_EQ(copy.Y, -6);
}

//整数点：输出流可写出且不崩溃
TEST_F(Coord_Type_Test, 整数点输出流可写出)
{
	//待输出的整数点
	const engine::Point2i point(3, -7);
	//输出目标流
	std::ostringstream stream;
	//写出操作不应抛异常
	EXPECT_NO_THROW(stream << point);
	//写出结果不应为空
	EXPECT_FALSE(stream.str().empty());
}

//整数矩形：输出流可写出且不崩溃
TEST_F(Coord_Type_Test, 整数矩形输出流可写出)
{
	//待输出的整数矩形
	const engine::Rect2i range(1, 2, 3, 4);
	//输出目标流
	std::ostringstream stream;
	//写出操作不应抛异常
	EXPECT_NO_THROW(stream << range);
	//写出结果不应为空
	EXPECT_FALSE(stream.str().empty());
}

// ———— 64 位坐标：构造与默认值 ————

//64 位矩形：带参构造按 左、右、上、下 顺序写入
TEST_F(Coord_Type_Test, 六十四位矩形带参构造按顺序写入)
{
	//按 左、右、上、下 的顺序构造
	engine::Rect2l range(11, 22, 33, 44);
	//四条边界应依次读回
	EXPECT_EQ(range.left, 11);
	EXPECT_EQ(range.right, 22);
	EXPECT_EQ(range.up, 33);
	EXPECT_EQ(range.down, 44);
	//与原值重建的矩形应判相等
	EXPECT_TRUE(range == engine::Rect2l(11, 22, 33, 44));
}

//浮点点：默认构造为零点
TEST_F(Coord_Type_Test, 浮点点默认构造为零点)
{
	//默认构造的浮点点
	engine::Point2d point;
	//两个分量都应为零
	EXPECT_EQ(point.X, 0.0);
	EXPECT_EQ(point.Y, 0.0);
	//应与显式零点判相等
	EXPECT_TRUE(point == engine::Point2d(0.0, 0.0));
}

// ———— 精度转换：负数与边界 ————

//浮点转整数：负数按四舍五入取整
TEST_F(Coord_Type_Test, 浮点转整数负数按四舍五入取整)
{
	//小数部分不足一半，向零方向取整
	EXPECT_EQ(engine::point_to_int(engine::Point2d(-1.4, -2.4)), (engine::Point2i(-1, -2)));
	//小数部分超过一半，远离零方向取整
	EXPECT_EQ(engine::point_to_int(engine::Point2d(-1.6, -2.6)), (engine::Point2i(-2, -3)));
}

//浮点转整数：正负半数一律远离零取整
TEST_F(Coord_Type_Test, 浮点转整数正负半数远离零)
{
	//正半数向正方向进位
	EXPECT_EQ(engine::point_to_int(engine::Point2d(0.5, 1.5)), (engine::Point2i(1, 2)));
	//负半数向负方向进位
	EXPECT_EQ(engine::point_to_int(engine::Point2d(-0.5, -1.5)), (engine::Point2i(-1, -2)));
}

//64 位浮点转整数：可承载超出 int 表示范围的整数值
TEST_F(Coord_Type_Test, 六十四位浮点转整数承载超int边界)
{
	//int 最大值加一，已超出 int 表示范围
	const double beyond_int = 2147483648.0;
	//正负两侧都应完整落到 64 位分量
	EXPECT_EQ(engine::point_to_l(engine::Point2d(beyond_int, -beyond_int)),
		(engine::Point2l(2147483648ll, -2147483648ll)));
}

//64 位整数转浮点：先转浮点再转回应保持原值
TEST_F(Coord_Type_Test, 六十四位整数转浮点往返保持原值)
{
	//单值精确转换
	EXPECT_EQ(engine::point_to_double(engine::Point2l(1024, -2048)), (engine::Point2d(1024.0, -2048.0)));
	//2^40 仍在浮点可精确表示范围内
	const engine::Point2l origin(1ll << 40, -(1ll << 40));
	//先转浮点再转回 64 位整数应保持原值
	EXPECT_EQ(engine::point_to_l(engine::point_to_double(origin)), origin);
}

// ———— 精度比较工具：相邻负值与零点、无穷 ————

//ULP距离：相邻负可表示值距离为一
TEST_F(Coord_Type_Test, ULP距离相邻负值距离为一)
{
	//负基准值与其下一个更负的可表示值之间恰好相差一个ULP
	EXPECT_EQ(engine::detail::ulp_distance(-1.0, std::nextafter(-1.0, -1e308)), 1u);
	//另一负值同样满足相邻距离为一
	EXPECT_EQ(engine::detail::ulp_distance(-2.0, std::nextafter(-2.0, -1e308)), 1u);
}

//ULP距离：跨越零点的两侧值返回极大值
TEST_F(Coord_Type_Test, ULP距离跨越零点返回极大值)
{
	//最小正非规格数及其相反数分处零点两侧
	const double tiny = std::numeric_limits<double>::denorm_min();
	//跨越零点的最小正负值仍返回极大值
	EXPECT_EQ(engine::detail::ulp_distance(-tiny, tiny), UINT64_MAX);
	//正负零数值相同，距离为零
	EXPECT_EQ(engine::detail::ulp_distance(-0.0, 0.0), 0u);
}

//ULP距离：相同的无穷之间距离为零
TEST_F(Coord_Type_Test, ULP距离相同无穷距离为零)
{
	//正无穷
	const double inf_value = std::numeric_limits<double>::infinity();
	//相同正无穷之间距离为零
	EXPECT_EQ(engine::detail::ulp_distance(inf_value, inf_value), 0u);
	//相同负无穷之间距离为零
	EXPECT_EQ(engine::detail::ulp_distance(-inf_value, -inf_value), 0u);
}
