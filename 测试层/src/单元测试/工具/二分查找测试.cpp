//二分查找算法测试：覆盖命中、未命中、空区间、首尾元素、投影字段与重复区间
#include <gtest/gtest.h>

//获取二分查找算法
#include "src/tools/Detail/二分查找.h"

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
	std::optional<uint64_t> index = engine::detail::binary_search(ascending_set, 5, std::ranges::less());
	EXPECT_EQ(index, 2u);
}

//命中：查找首元素
TEST_F(Binary_Search_Test, 命中首元素)
{
	//首元素应落在索引 0
	std::optional<uint64_t> index = engine::detail::binary_search(ascending_set, 1, std::ranges::less());
	EXPECT_EQ(index, 0u);
}

//命中：查找尾元素
TEST_F(Binary_Search_Test, 命中尾元素)
{
	//尾元素应落在末位索引
	std::optional<uint64_t> index = engine::detail::binary_search(ascending_set, 11, std::ranges::less());
	EXPECT_EQ(index, static_cast<uint64_t>(ascending_set.size()) - 1);
}

//未命中：目标小于序列最小值
TEST_F(Binary_Search_Test, 未命中时返回空值)
{
	//小于最小值的查找必然失败
	std::optional<uint64_t> index = engine::detail::binary_search(ascending_set, 0, std::ranges::less());
	EXPECT_EQ(index, std::nullopt);
}

//未命中：目标落在序列空隙中
TEST_F(Binary_Search_Test, 空隙目标返回空值)
{
	//序列中不存在 6
	std::optional<uint64_t> index = engine::detail::binary_search(ascending_set, 6, std::ranges::less());
	EXPECT_EQ(index, std::nullopt);
}

//未命中：空区间查找
TEST_F(Binary_Search_Test, 空区间返回空值)
{
	//空序列
	std::vector<int> empty_set;
	//空区间上查找不应越界访问
	std::optional<uint64_t> index = engine::detail::binary_search(empty_set, 1, std::ranges::less());
	EXPECT_EQ(index, std::nullopt);
}

//投影查找：按记录的分值字段定位
TEST_F(Binary_Search_Test, 按投影字段命中)
{
	//按分值升序排列的记录序列
	std::vector<test_item> item_set{ {10, 10}, {20, 20}, {30, 30}, {40, 40} };
	//以分值为投影进行查找
	std::optional<uint64_t> index = engine::detail::binary_search(item_set, 30, std::ranges::less(),
		[](const test_item& item) { return item.score; });
	EXPECT_EQ(index, 2u);
}

//投影查找：投影字段未命中
TEST_F(Binary_Search_Test, 按投影字段未命中)
{
	//按分值升序排列的记录序列
	std::vector<test_item> item_set{ {10, 10}, {20, 20}, {30, 30} };
	//查找不存在的分值
	std::optional<uint64_t> index = engine::detail::binary_search(item_set, 25, std::ranges::less(),
		[](const test_item& item) { return item.score; });
	EXPECT_EQ(index, std::nullopt);
}

//子区间查找：返回索引以传入的起始迭代器为基准
TEST_F(Binary_Search_Test, 子区间查找索引相对起点)
{
	//从第 3 个元素起查找目标 7
	std::optional<uint64_t> index = engine::detail::binary_search(ascending_set.begin() + 2, ascending_set.end(),
		7, std::ranges::less());
	EXPECT_EQ(index, 1u);
}

//降序序列查找：比较器与序列顺序一致时同样有效
TEST_F(Binary_Search_Test, 降序序列查找)
{
	//降序排列的序列
	std::vector<int> descending_set{ 11, 9, 7, 5, 3, 1 };
	//以降序比较查找
	std::optional<uint64_t> index = engine::detail::binary_search(descending_set, 7, std::ranges::greater());
	EXPECT_EQ(index, 2u);
}

//重复区间：闭区间返回全部等价元素
TEST_F(Binary_Search_Test, 重复元素返回闭区间)
{
	//三个 2 位于索引 1..3
	std::optional<std::pair<uint64_t, uint64_t>> range =
		engine::detail::range_binary_search(duplicate_set, 2, std::ranges::less());
	ASSERT_TRUE(range.has_value());
	EXPECT_EQ(range->first, 1u);
	EXPECT_EQ(range->second, 3u);
}

//重复区间：唯一元素返回退化为单点的闭区间
TEST_F(Binary_Search_Test, 唯一元素返回单点区间)
{
	//索引 4 处的 3 只出现一次
	std::optional<std::pair<uint64_t, uint64_t>> range =
		engine::detail::range_binary_search(duplicate_set, 3, std::ranges::less());
	ASSERT_TRUE(range.has_value());
	EXPECT_EQ(range->first, 4u);
	EXPECT_EQ(range->second, 4u);
}

//重复区间：首元素
TEST_F(Binary_Search_Test, 重复区间首元素)
{
	//索引 0 处的 1 只出现一次
	std::optional<std::pair<uint64_t, uint64_t>> range =
		engine::detail::range_binary_search(duplicate_set, 1, std::ranges::less());
	ASSERT_TRUE(range.has_value());
	EXPECT_EQ(range->first, 0u);
	EXPECT_EQ(range->second, 0u);
}

//重复区间：目标不存在时返回空值
TEST_F(Binary_Search_Test, 重复区间未命中)
{
	//序列中不存在 4
	std::optional<std::pair<uint64_t, uint64_t>> range =
		engine::detail::range_binary_search(duplicate_set, 4, std::ranges::less());
	EXPECT_FALSE(range.has_value());
}

//重复区间：空区间查找
TEST_F(Binary_Search_Test, 重复区间空序列)
{
	//空序列
	std::vector<int> empty_set;
	//空区间上查找不应越界访问
	std::optional<std::pair<uint64_t, uint64_t>> range =
		engine::detail::range_binary_search(empty_set, 1, std::ranges::less());
	EXPECT_FALSE(range.has_value());
}

//重复区间：全序列等价
TEST_F(Binary_Search_Test, 重复区间全序列等价)
{
	//全部元素相同的序列
	std::vector<int> same_set{ 7, 7, 7, 7 };
	//等价区间应覆盖整个序列
	std::optional<std::pair<uint64_t, uint64_t>> range =
		engine::detail::range_binary_search(same_set, 7, std::ranges::less());
	ASSERT_TRUE(range.has_value());
	EXPECT_EQ(range->first, 0u);
	EXPECT_EQ(range->second, 3u);
}

//未命中：显式空容器迭代器区间查找
TEST_F(Binary_Search_Test, 空容器迭代器重载返回空值)
{
	//空序列
	std::vector<int> empty_set;
	//以迭代器重载在空区间上查找
	std::optional<uint64_t> index = engine::detail::binary_search(empty_set.begin(), empty_set.end(),
		0, std::ranges::less());
	//空区间查找应返回空值
	EXPECT_EQ(index, std::nullopt);
}

//命中：单元素容器查找唯一元素
TEST_F(Binary_Search_Test, 单元素容器命中)
{
	//仅含一个元素的序列
	std::vector<int> single_set{ 42 };
	//查找该唯一元素
	std::optional<uint64_t> index = engine::detail::binary_search(single_set, 42, std::ranges::less());
	//应命中索引 0
	EXPECT_EQ(index, 0u);
}

//未命中：单元素容器查找不存在的元素
TEST_F(Binary_Search_Test, 单元素容器未命中)
{
	//仅含一个元素的序列
	std::vector<int> single_set{ 42 };
	//查找比唯一元素大的目标
	std::optional<uint64_t> index = engine::detail::binary_search(single_set, 43, std::ranges::less());
	//应返回空值
	EXPECT_EQ(index, std::nullopt);
}

//未命中：目标小于序列首元素
TEST_F(Binary_Search_Test, 目标小于首元素返回空值)
{
	//升序序列
	std::vector<int> price_set{ 10, 20, 30, 40 };
	//查找比首元素还小的目标
	std::optional<uint64_t> index = engine::detail::binary_search(price_set, 5, std::ranges::less());
	//应返回空值
	EXPECT_EQ(index, std::nullopt);
}

//未命中：目标大于序列尾元素
TEST_F(Binary_Search_Test, 目标大于尾元素返回空值)
{
	//查找比尾元素还大的目标
	std::optional<uint64_t> index = engine::detail::binary_search(ascending_set, 100, std::ranges::less());
	//应返回空值
	EXPECT_EQ(index, std::nullopt);
}

//未命中：目标位于两相邻元素之间
TEST_F(Binary_Search_Test, 目标位于相邻元素之间)
{
	//目标 4 落在 3 与 5 之间
	std::optional<uint64_t> index = engine::detail::binary_search(ascending_set, 4, std::ranges::less());
	//相邻元素之间无对应元素，应返回空值
	EXPECT_EQ(index, std::nullopt);
}

//命中：重复元素查找返回某个等价下标
TEST_F(Binary_Search_Test, 重复元素返回等价下标)
{
	//在含三个 2 的序列中查找 2
	std::optional<uint64_t> index = engine::detail::binary_search(duplicate_set, 2, std::ranges::less());
	//应命中且下标处元素恰为 2
	ASSERT_TRUE(index.has_value());
	EXPECT_EQ(duplicate_set[*index], 2);
}

//重复区间：重复值位于序列首部的闭区间
TEST_F(Binary_Search_Test, 重复区间首部连续重复值)
{
	//首部含三个 2 的序列
	std::vector<int> head_set{ 2, 2, 2, 5, 6 };
	//查找 2 的等价闭区间
	std::optional<std::pair<uint64_t, uint64_t>> range =
		engine::detail::range_binary_search(head_set, 2, std::ranges::less());
	//闭区间应从索引 0 到索引 2
	ASSERT_TRUE(range.has_value());
	EXPECT_EQ(range->first, 0u);
	EXPECT_EQ(range->second, 2u);
}

//重复区间：重复值位于序列尾部的闭区间
TEST_F(Binary_Search_Test, 重复区间尾部连续重复值)
{
	//尾部含三个 5 的序列
	std::vector<int> tail_set{ 1, 3, 5, 5, 5 };
	//查找 5 的等价闭区间
	std::optional<std::pair<uint64_t, uint64_t>> range =
		engine::detail::range_binary_search(tail_set, 5, std::ranges::less());
	//闭区间应从索引 2 到索引 4
	ASSERT_TRUE(range.has_value());
	EXPECT_EQ(range->first, 2u);
	EXPECT_EQ(range->second, 4u);
}

//重复区间：仅含一个元素的序列等价区间退化为单点
TEST_F(Binary_Search_Test, 单元素序列等价区间)
{
	//仅含一个元素的序列
	std::vector<int> single_set{ 8 };
	//查找该唯一元素
	std::optional<std::pair<uint64_t, uint64_t>> range =
		engine::detail::range_binary_search(single_set, 8, std::ranges::less());
	//区间应退化为索引 0 的单点
	ASSERT_TRUE(range.has_value());
	EXPECT_EQ(range->first, 0u);
	EXPECT_EQ(range->second, 0u);
}

//重复区间：重复值位于序列中部的等价闭区间
TEST_F(Binary_Search_Test, 重复区间中部连续重复值)
{
	//中部含三个 2 的序列
	std::vector<int> middle_set{ 1, 1, 2, 2, 2, 3, 3 };
	//查找 2 的等价闭区间
	std::optional<std::pair<uint64_t, uint64_t>> range =
		engine::detail::range_binary_search(middle_set, 2, std::ranges::less());
	//闭区间应从索引 2 到索引 4
	ASSERT_TRUE(range.has_value());
	EXPECT_EQ(range->first, 2u);
	EXPECT_EQ(range->second, 4u);
}

//重载一致：迭代器重载与容器重载返回相同下标
TEST_F(Binary_Search_Test, 迭代器与容器重载结果一致)
{
	//容器重载查找 9
	std::optional<uint64_t> container_index = engine::detail::binary_search(ascending_set, 9, std::ranges::less());
	//迭代器重载查找 9
	std::optional<uint64_t> iterator_index = engine::detail::binary_search(ascending_set.begin(), ascending_set.end(),
		9, std::ranges::less());
	//两种重载应返回相同结果
	ASSERT_TRUE(container_index.has_value());
	ASSERT_TRUE(iterator_index.has_value());
	EXPECT_EQ(container_index, iterator_index);
}

//重复区间：降序序列的等价闭区间
TEST_F(Binary_Search_Test, 降序序列重复区间)
{
	//降序排列且含两个 7 的序列
	std::vector<int> descending_set{ 9, 9, 7, 7, 2, 1 };
	//以降序比较查找 7 的等价闭区间
	std::optional<std::pair<uint64_t, uint64_t>> range =
		engine::detail::range_binary_search(descending_set, 7, std::ranges::greater());
	//闭区间应从索引 2 到索引 3
	ASSERT_TRUE(range.has_value());
	EXPECT_EQ(range->first, 2u);
	EXPECT_EQ(range->second, 3u);
}

//投影查找：投影字段配合降序比较器
TEST_F(Binary_Search_Test, 投影字段降序命中)
{
	//按分值降序排列的记录序列
	std::vector<test_item> item_set{ {40, 40}, {30, 30}, {20, 20}, {10, 10} };
	//以分值投影和降序比较器查找分值 20
	std::optional<uint64_t> index = engine::detail::binary_search(item_set, 20, std::ranges::greater(),
		[](const test_item& item) { return item.score; });
	//应命中索引 2
	EXPECT_EQ(index, 2u);
}
