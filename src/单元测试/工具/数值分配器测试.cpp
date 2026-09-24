//数值分配器测试：覆盖初始分配、起点设置、回收复用顺序、重复回收拒绝、批量回收与重置
#include <gtest/gtest.h>

//获取数值分配器
#include "src/tools/Number_Allocator/数值分配器.h"

//数值分配器测试夹具
class Number_Allocator_Test : public ::testing::Test
{
protected:
	//被测分配器
	engine::Number_Allocator allocator;
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