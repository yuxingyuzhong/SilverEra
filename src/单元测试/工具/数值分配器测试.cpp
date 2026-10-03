//数值分配器测试：覆盖初始分配、起点设置、回收复用顺序、重复回收拒绝、批量回收、分配方式、模板类型与重置
#include <gtest/gtest.h>

//获取数值分配器
#include "Engine/EngineCore/src/tools/Number_Allocator/数值分配器.h"

//数值分配器测试夹具
class Number_Allocator_Test : public ::testing::Test
{
protected:
	//被测分配器
	engine::Number_Allocator<uint64_t> allocator;
};

//默认起点：从零开始逐个递增
TEST_F(Number_Allocator_Test, 默认从零开始递增)
{
	//连续取三个数值
	EXPECT_EQ(allocator.get(), 0u);
	EXPECT_EQ(allocator.get(), 1u);
	EXPECT_EQ(allocator.get(), 2u);
}

//设置起点：取出的数值从起点开始
TEST_F(Number_Allocator_Test, 设置起点后从起点递增)
{
	//把分配起点设为 100
	allocator.set(100);
	//首个数值应为起点本身
	EXPECT_EQ(allocator.get(), 100u);
	//后续数值依次递增
	EXPECT_EQ(allocator.get(), 101u);
}

//起点设置可覆盖：后设置的值生效
TEST_F(Number_Allocator_Test, 起点可被覆盖)
{
	//先设起点为 10
	allocator.set(10);
	//再设起点为 50
	allocator.set(50);
	//取出的数值应基于后设置的起点
	EXPECT_EQ(allocator.get(), 50u);
}

//回收复用：回收后的数值会被优先重新分配
TEST_F(Number_Allocator_Test, 回收数值被优先分配)
{
	//先取出 0、1、2
	allocator.get();
	allocator.get();
	allocator.get();
	//回收数值 1
	EXPECT_TRUE(allocator.recycle(1u));
	//下一次分配应取回 1 而不是新的 3
	EXPECT_EQ(allocator.get(), 1u);
	//回收集合已空，继续分配新的 3
	EXPECT_EQ(allocator.get(), 3u);
}

//复用顺序：多个回收值按后进先出取回
TEST_F(Number_Allocator_Test, 回收值后进先出)
{
	//依次回收 1 与 2
	allocator.recycle(1u);
	allocator.recycle(2u);
	//应先取回后回收的 2
	EXPECT_EQ(allocator.get(), 2u);
	//再取回先回收的 1
	EXPECT_EQ(allocator.get(), 1u);
}

//重复回收：同一数值第二次回收被拒绝
TEST_F(Number_Allocator_Test, 重复回收被拒绝)
{
	//首次回收成功
	EXPECT_TRUE(allocator.recycle(5u));
	//二次回收同一数值失败
	EXPECT_FALSE(allocator.recycle(5u));
	//回收集合中该数值只保留一份
	EXPECT_EQ(allocator.get(), 5u);
	//再次分配只能拿到新数值
	EXPECT_EQ(allocator.get(), 0u);
}

//批量回收：容器重载逐个回收并保持后进先出
TEST_F(Number_Allocator_Test, 批量回收容器重载)
{
	//批量回收三个数值
	allocator.recycle(std::vector<uint64_t>{ 7u, 8u, 9u });
	//应按回收顺序逆序取回
	EXPECT_EQ(allocator.get(), 9u);
	EXPECT_EQ(allocator.get(), 8u);
	EXPECT_EQ(allocator.get(), 7u);
}

//批量回收中的重复项：重复值被忽略且不影响其余数值
TEST_F(Number_Allocator_Test, 批量回收忽略重复项)
{
	//同一数值在批量中重复出现
	allocator.recycle(std::vector<uint64_t>{ 3u, 3u });
	//该数值只保留一份
	EXPECT_EQ(allocator.get(), 3u);
	//回收集合已空
	EXPECT_EQ(allocator.get(), 0u);
}

//未分配过的数值也可回收：分配器不做归属校验
TEST_F(Number_Allocator_Test, 未分配数值也可回收)
{
	//直接回收一个从未取出的数值
	EXPECT_TRUE(allocator.recycle(999u));
	//下一次分配取回该数值
	EXPECT_EQ(allocator.get(), 999u);
}

//重置：起点与回收集合一起复位
TEST_F(Number_Allocator_Test, 重置清空回收集合与起点)
{
	//先取出若干数值
	allocator.get();
	allocator.get();
	//回收一个数值
	allocator.recycle(1u);
	//重置分配器
	allocator.reset();
	//重置后应从零重新开始
	EXPECT_EQ(allocator.get(), 0u);
}

//重置后可重新设起点：顺序不受影响
TEST_F(Number_Allocator_Test, 重置后可重新设起点)
{
	//设起点并取一个数值
	allocator.set(20);
	allocator.get();
	//重置并重设起点
	allocator.reset();
	allocator.set(300);
	//应从新起点开始
	EXPECT_EQ(allocator.get(), 300u);
}

//大量分配：两千次分配互不重复
TEST_F(Number_Allocator_Test, 大量分配互不重复)
{
	//分配次数
	const uint64_t count = 2000;
	//已分配数值集合
	std::set<uint64_t> issued;
	//连续分配并收集
	for (uint64_t i = 0; i < count; ++i)
		issued.insert(allocator.get());
	//集合大小应与分配次数一致
	EXPECT_EQ(issued.size(), static_cast<size_t>(count));
}

//回收后重复分配：数值不会同时被两个持有者拿到
TEST_F(Number_Allocator_Test, 回收复用不产生重复持有)
{
	//取出三个数值
	const uint64_t first = allocator.get();
	const uint64_t second = allocator.get();
	const uint64_t third = allocator.get();
	//归还中间那个
	allocator.recycle(second);
	//重新取出应得到被归还的数值
	const uint64_t reused = allocator.get();
	//且与仍在持有的数值不同
	EXPECT_EQ(reused, second);
	EXPECT_NE(reused, first);
	EXPECT_NE(reused, third);
}

//分配方式：重置后回到默认后进先出
TEST_F(Number_Allocator_Test, 重置后分配方式复位为后进先出)
{
	//先切换为先进先出
	allocator.set(engine::Allocate_Order::FIFO);
	//依次回收两个数值
	allocator.recycle(1u);
	allocator.recycle(2u);
	//重置分配器(分配方式一并复位)
	allocator.reset();
	//重置后回收池已清空，重新回收两个数值
	allocator.recycle(1u);
	allocator.recycle(2u);
	//默认后进先出应先取回后回收的 2
	EXPECT_EQ(allocator.get(), 2u);
	//再取回先回收的 1
	EXPECT_EQ(allocator.get(), 1u);
}

//先进先出：多个回收值按先进先出取回
TEST_F(Number_Allocator_Test, 先进先出回收弹出顺序)
{
	//切换为先进先出分配方式
	allocator.set(engine::Allocate_Order::FIFO);
	//依次回收 1 与 2
	allocator.recycle(1u);
	allocator.recycle(2u);
	//应先取回先回收的 1
	EXPECT_EQ(allocator.get(), 1u);
	//再取回后回收的 2
	EXPECT_EQ(allocator.get(), 2u);
}

//分配方式切换：弹出顺序随当前设置改变
TEST_F(Number_Allocator_Test, 先进先出与后进先出切换)
{
	//连续回收 1、2、3
	allocator.recycle(1u);
	allocator.recycle(2u);
	allocator.recycle(3u);
	//切换为先进先出后应取回最小的 1
	allocator.set(engine::Allocate_Order::FIFO);
	EXPECT_EQ(allocator.get(), 1u);
	//切换为后进先出后应取回最大的 3
	allocator.set(engine::Allocate_Order::LIFO);
	EXPECT_EQ(allocator.get(), 3u);
	//剩余数值按后进先出取回 2
	EXPECT_EQ(allocator.get(), 2u);
}

//设置起点：从起点开始连续递增分配多个数值
TEST_F(Number_Allocator_Test, 设置起点后连续递增分配)
{
	//把分配起点设为 1000
	allocator.set(1000);
	//连续取出四个数值应依次递增
	EXPECT_EQ(allocator.get(), 1000u);
	EXPECT_EQ(allocator.get(), 1001u);
	EXPECT_EQ(allocator.get(), 1002u);
	EXPECT_EQ(allocator.get(), 1003u);
}

//回收复用：取出的数值可被再次回收
TEST_F(Number_Allocator_Test, 回收值取出后可再次回收)
{
	//取出数值
	const uint64_t value = allocator.get();
	//回收刚取出的数值成功
	EXPECT_TRUE(allocator.recycle(value));
	//再次取出后回收集合已空
	EXPECT_EQ(allocator.get(), value);
	//此时该数值已不在回收池，可再次回收
	EXPECT_TRUE(allocator.recycle(value));
}

//批量回收：按调用顺序逐个入池，重复项被忽略
TEST_F(Number_Allocator_Test, 批量回收保持调用顺序并忽略重复项)
{
	//批量回收含重复且乱序的数值
	allocator.recycle(std::vector<uint64_t>{ 5u, 1u, 3u, 1u, 5u });
	//后进先出先取回最后入池的 3
	EXPECT_EQ(allocator.get(), 3u);
	//再取回 1
	EXPECT_EQ(allocator.get(), 1u);
	//最后取回 5
	EXPECT_EQ(allocator.get(), 5u);
	//回收池已空，继续分配新的游标数值 0
	EXPECT_EQ(allocator.get(), 0u);
}

//重置：分配游标归零并清空回收池
TEST_F(Number_Allocator_Test, 重置后计数归零并连续递增)
{
	//连续取出三个数值使游标前进
	allocator.get();
	allocator.get();
	allocator.get();
	//回收一个数值进入回收池
	allocator.recycle(2u);
	//重置分配器
	allocator.reset();
	//重置后应从零重新连续递增
	EXPECT_EQ(allocator.get(), 0u);
	EXPECT_EQ(allocator.get(), 1u);
	EXPECT_EQ(allocator.get(), 2u);
}

//游标交互：回收池优先分配且不影响新数值游标
TEST_F(Number_Allocator_Test, 回收池与数值游标交互)
{
	//把起点设为 10
	allocator.set(10);
	//取出 10、11，游标前进到 12
	EXPECT_EQ(allocator.get(), 10u);
	EXPECT_EQ(allocator.get(), 11u);
	//回收 10
	EXPECT_TRUE(allocator.recycle(10u));
	//回收池优先返回 10
	EXPECT_EQ(allocator.get(), 10u);
	//回收池耗尽后游标未被回收影响，返回 12
	EXPECT_EQ(allocator.get(), 12u);
}

//空池分配：连续分配数值严格递增
TEST_F(Number_Allocator_Test, 空池连续分配严格递增)
{
	//把起点设为 7
	allocator.set(7);
	//记录上一次取值
	uint64_t previous = allocator.get();
	//起点值应为 7
	EXPECT_EQ(previous, 7u);
	//连续分配九次并校验严格递增
	for (uint64_t i = 0; i < 9; ++i)
	{
		//取出下一个数值
		const uint64_t current = allocator.get();
		//后取数值应比前取数值大 1
		EXPECT_EQ(current, previous + 1);
		//更新上一次取值
		previous = current;
	}
}

//回收池耗尽：回收值用尽后回到游标递增分配
TEST_F(Number_Allocator_Test, 回收池耗尽后继续递增分配)
{
	//取出 0、1，游标前进到 2
	allocator.get();
	allocator.get();
	//回收 0 与 1
	allocator.recycle(0u);
	allocator.recycle(1u);
	//后进先出先取回 1
	EXPECT_EQ(allocator.get(), 1u);
	//再取回 0
	EXPECT_EQ(allocator.get(), 0u);
	//回收池耗尽后游标未回退，继续返回 2
	EXPECT_EQ(allocator.get(), 2u);
}

//极值边界：游标即为最大数值时首次分配即判定耗尽并回绕
TEST_F(Number_Allocator_Test, 游标为最大数值时首次分配回绕)
{
	//把起点设为 uint64_t 最大值
	allocator.set(UINT64_MAX);
	//游标与最大值相等即判定耗尽，首次分配直接回绕到最小值
	EXPECT_EQ(allocator.get(), 0u);
}

//极值边界：游标递增到最大值时回绕，最大值本身不外发
TEST_F(Number_Allocator_Test, 游标递增至最大值时回绕)
{
	//把起点设到最大值前一位
	allocator.set(UINT64_MAX - 1);
	//先正常取出起点数值
	EXPECT_EQ(allocator.get(), UINT64_MAX - 1);
	//游标到达最大值后判定耗尽，回绕到最小值
	EXPECT_EQ(allocator.get(), 0u);
}

//极值边界：回收最大数值后可原样取回
TEST_F(Number_Allocator_Test, 回收最大数值后原样取回)
{
	//回收 uint64_t 最大值
	EXPECT_TRUE(allocator.recycle(UINT64_MAX));
	//分配应原样返回该最大值
	EXPECT_EQ(allocator.get(), UINT64_MAX);
}

//模板类型：可按 8 位无符号整数分配
TEST_F(Number_Allocator_Test, 八位无符号类型分配)
{
	//8 位分配器
	engine::Number_Allocator<uint8_t> byte_allocator;
	//连续取出三个数值
	EXPECT_EQ(static_cast<int>(byte_allocator.get()), 0);
	EXPECT_EQ(static_cast<int>(byte_allocator.get()), 1);
	EXPECT_EQ(static_cast<int>(byte_allocator.get()), 2);
}

//模板类型：可按有符号整数分配且起点可为负数
TEST_F(Number_Allocator_Test, 有符号类型负数起点)
{
	//32 位有符号分配器
	engine::Number_Allocator<int32_t> signed_allocator;
	//把起点设为负数
	signed_allocator.set(-5);
	//从负起点开始递增
	EXPECT_EQ(signed_allocator.get(), -5);
	EXPECT_EQ(signed_allocator.get(), -4);
}

//极值边界：8 位类型游标耗尽后回绕到最小值
TEST_F(Number_Allocator_Test, 八位类型游标耗尽回绕)
{
	//8 位分配器
	engine::Number_Allocator<uint8_t> byte_allocator;
	//把游标设到最大值前一位
	byte_allocator.set(static_cast<uint8_t>(254));
	//先正常取出 254
	EXPECT_EQ(static_cast<int>(byte_allocator.get()), 254);
	//游标到达 255 后判定耗尽，回绕到最小值
	EXPECT_EQ(static_cast<int>(byte_allocator.get()), 0);
}

//重置：重复回收检测记录一并清空
TEST_F(Number_Allocator_Test, 重置清空重复回收记录)
{
	//回收数值 4
	EXPECT_TRUE(allocator.recycle(4u));
	//重复回收同一数值被拒绝
	EXPECT_FALSE(allocator.recycle(4u));
	//重置分配器
	allocator.reset();
	//重置后同一数值可再次回收
	EXPECT_TRUE(allocator.recycle(4u));
}

//分配方式设置：切换分配方式不影响数值游标
TEST_F(Number_Allocator_Test, 切换分配方式不影响数值游标)
{
	//取出 0、1，游标前进到 2
	allocator.get();
	allocator.get();
	//切换分配方式
	allocator.set(engine::Allocate_Order::FIFO);
	//回收池为空，游标不受分配方式影响，继续返回 2
	EXPECT_EQ(allocator.get(), 2u);
}