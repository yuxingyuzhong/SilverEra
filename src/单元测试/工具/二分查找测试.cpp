//二分查找算法测试：覆盖命中、未命中、空区间、首尾元素、投影字段与重复区间
#include <gtest/gtest.h>

//获取二分查找算法
#include "src/tools/Auxi_Algorithm/二分查找.h"

//测试用记录（用于验证按投影字段查找）
struct test_item
{
	//对象编号
	uint64_t ID;
	//排序所用的分值
	int score;
};

//二分查找测试夹具
class Binary_Search_Test : public ::testing::Test
{
protected:
	//已升序排列的待查序列
	std::vector<int> ascending_set{ 1, 3, 5, 7, 9, 11 };
	//含重复元素的待查序列
	std::vector<int> duplicate_set{ 1, 2, 2, 2, 3, 5 };
};

//命中：查找中间元素
TEST_F(Binary_Search_Test, 命中中间元素)
{
	//容器重载按升序比较查找
	int index = engine::binary_search(ascending_set, 5, std::ranges::less());
	EXPECT_EQ(index, 2);
}

//命中：查找首元素
TEST_F(Binary_Search_Test, 命中首元素)
{
	//首元素应落在索引 0
	int index = engine::binary_search(ascending_set, 1, std::ranges::less());
	EXPECT_EQ(index, 0);
}

//命中：查找尾元素
TEST_F(Binary_Search_Test, 命中尾元素)
{
	//尾元素应落在末位索引
	int index = engine::binary_search(ascending_set, 11, std::ranges::less());
	EXPECT_EQ(index, static_cast<int>(ascending_set.size()) - 1);
}

//未命中：目标小于序列最小值
TEST_F(Binary_Search_Test, 未命中时返回无效索引)
{
	//小于最小值的查找必然失败
	int index = engine::binary_search(ascending_set, 0, std::ranges::less());
	EXPECT_EQ(index, -1);
}

//未命中：目标落在序列空隙中
TEST_F(Binary_Search_Test, 空隙目标返回无效索引)
{
	//序列中不存在 6
	int index = engine::binary_search(ascending_set, 6, std::ranges::less());
	EXPECT_EQ(index, -1);
}

//未命中：空区间查找
TEST_F(Binary_Search_Test, 空区间返回无效索引)
{
	//空序列
	std::vector<int> empty_set;
	//空区间上查找不应越界访问
	int index = engine::binary_search(empty_set, 1, std::ranges::less());
	EXPECT_EQ(index, -1);
}

//投影查找：按记录的分值字段定位
TEST_F(Binary_Search_Test, 按投影字段命中)
{
	//按分值升序排列的记录序列
	std::vector<test_item> item_set{ {10, 10}, {20, 20}, {30, 30}, {40, 40} };
	//以分值为投影进行查找
	int index = engine::binary_search(item_set, 30, std::ranges::less(),
		[](const test_item& item) { return item.score; });
	EXPECT_EQ(index, 2);
}

//投影查找：投影字段未命中
TEST_F(Binary_Search_Test, 按投影字段未命中)
{
	//按分值升序排列的记录序列
	std::vector<test_item> item_set{ {10, 10}, {20, 20}, {30, 30} };
	//查找不存在的分值
	int index = engine::binary_search(item_set, 25, std::ranges::less(),
		[](const test_item& item) { return item.score; });
	EXPECT_EQ(index, -1);
}

//子区间查找：返回索引以传入的起始迭代器为基准
TEST_F(Binary_Search_Test, 子区间查找索引相对起点)
{
	//从第 3 个元素起查找目标 7
	int index = engine::binary_search(ascending_set.begin() + 2, ascending_set.end(),
		7, std::ranges::less());
	EXPECT_EQ(index, 1);
}

//降序序列查找：比较器与序列顺序一致时同样有效
TEST_F(Binary_Search_Test, 降序序列查找)
{
	//降序排列的序列
	std::vector<int> descending_set{ 11, 9, 7, 5, 3, 1 };
	//以降序比较查找
	int index = engine::binary_search(descending_set, 7, std::ranges::greater());
	EXPECT_EQ(index, 2);
}

//重复区间：闭区间返回全部等价元素
TEST_F(Binary_Search_Test, 重复元素返回闭区间)
{
	//三个 2 位于索引 1..3
	auto range = engine::range_binary_search(duplicate_set, 2, std::ranges::less());
	EXPECT_EQ(range.first, 1);
	EXPECT_EQ(range.second, 3);
}

//重复区间：唯一元素返回退化为单点的闭区间
TEST_F(Binary_Search_Test, 唯一元素返回单点区间)
{
	//索引 4 处的 3 只出现一次
	auto range = engine::range_binary_search(duplicate_set, 3, std::ranges::less());
	EXPECT_EQ(range.first, 4);
	EXPECT_EQ(range.second, 4);
}

//重复区间：首元素
TEST_F(Binary_Search_Test, 重复区间首元素)
{
	//索引 0 处的 1 只出现一次
	auto range = engine::range_binary_search(duplicate_set, 1, std::ranges::less());
	EXPECT_EQ(range.first, 0);
	EXPECT_EQ(range.second, 0);
}

//重复区间：目标不存在时返回无效区间
TEST_F(Binary_Search_Test, 重复区间未命中)
{
	//序列中不存在 4
	auto range = engine::range_binary_search(duplicate_set, 4, std::ranges::less());
	EXPECT_EQ(range.first, -1);
	EXPECT_EQ(range.second, -1);
}

//重复区间：空区间查找
TEST_F(Binary_Search_Test, 重复区间空序列)
{
	//空序列
	std::vector<int> empty_set;
	//空区间上查找不应越界访问
	auto range = engine::range_binary_search(empty_set, 1, std::ranges::less());
	EXPECT_EQ(range.first, -1);
	EXPECT_EQ(range.second, -1);
}

//重复区间：全序列等价
TEST_F(Binary_Search_Test, 重复区间全序列等价)
{
	//全部元素相同的序列
	std::vector<int> same_set{ 7, 7, 7, 7 };
	//等价区间应覆盖整个序列
	auto range = engine::range_binary_search(same_set, 7, std::ranges::less());
	EXPECT_EQ(range.first, 0);
	EXPECT_EQ(range.second, 3);
}
