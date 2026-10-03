//哈希混合测试：覆盖确定性、种子改写、输入敏感性、累积效应与极值输入
#include <gtest/gtest.h>

//获取哈希混合工具
#include "Engine/EngineCore/src/tools/Detail/哈希混合.h"

//哈希混合测试夹具
class Hash_Combine_Test : public ::testing::Test
{
public:
	//以同一种子同一种输入执行一次混合
	static size_t combine_once(size_t seed, size_t value)
	{
		engine::detail::hash_combine(seed, value);
		return seed;
	}
};

//确定性：相同种子与相同输入必然得到相同结果
TEST_F(Hash_Combine_Test, 相同输入结果一致)
{
	//两次独立混合（种子与输入完全相同）
	const size_t first = combine_once(12345u, 67890u);
	const size_t second = combine_once(12345u, 67890u);
	//结果应逐位一致
	EXPECT_EQ(first, second);
}

//确定性：反复混合同一输入仍保持稳定
TEST_F(Hash_Combine_Test, 重复混合结果稳定)
{
	//基准结果
	const size_t reference = combine_once(7u, 11u);
	//再执行多次
	for (int round = 0; round < 16; ++round)
		EXPECT_EQ(combine_once(7u, 11u), reference);
}

//改写性：混合会就地改写种子本身
TEST_F(Hash_Combine_Test, 混合就地改写种子)
{
	//初始种子
	size_t seed = 0u;
	//混合一个非零输入
	engine::detail::hash_combine(seed, 1u);
	//种子应已被改写
	EXPECT_NE(seed, 0u);
}

//输入敏感性：同一种子下不同输入得到不同结果
TEST_F(Hash_Combine_Test, 不同输入结果不同)
{
	//仅输入值不同
	EXPECT_NE(combine_once(100u, 1u), combine_once(100u, 2u));
}

//种子敏感性：同一输入下不同种子得到不同结果
TEST_F(Hash_Combine_Test, 不同种子结果不同)
{
	//仅种子不同
	EXPECT_NE(combine_once(1u, 100u), combine_once(2u, 100u));
}

//零输入：种子为零、输入为零仍产生确定结果
TEST_F(Hash_Combine_Test, 零种子零输入确定)
{
	//零种子混入零值
	const size_t result = combine_once(0u, 0u);
	//结果必然等于常量偏移与零位移的组合，且可复现
	EXPECT_EQ(result, combine_once(0u, 0u));
}

//零输入：混入零值仍会改写非零种子
TEST_F(Hash_Combine_Test, 零值输入仍改写种子)
{
	//非零种子
	size_t seed = 42u;
	//记录混合前取值
	const size_t before = seed;
	//混入零值
	engine::detail::hash_combine(seed, 0u);
	//种子被改写
	EXPECT_NE(seed, before);
}

//累积性：连续混合序列具有确定的最终值
TEST_F(Hash_Combine_Test, 连续混合序列确定)
{
	//第一段序列的累积结果
	size_t first = 0u;
	engine::detail::hash_combine(first, 3u);
	engine::detail::hash_combine(first, 5u);
	engine::detail::hash_combine(first, 7u);

	//第二段序列的累积结果（与第一段顺序相同）
	size_t second = 0u;
	engine::detail::hash_combine(second, 3u);
	engine::detail::hash_combine(second, 5u);
	engine::detail::hash_combine(second, 7u);

	//相同序列应得到相同累积值
	EXPECT_EQ(first, second);
}

//顺序敏感性：交换混合顺序应改变最终结果
TEST_F(Hash_Combine_Test, 交换顺序结果不同)
{
	//先混三再混五
	size_t first = 0u;
	engine::detail::hash_combine(first, 3u);
	engine::detail::hash_combine(first, 5u);

	//先混五再混三
	size_t second = 0u;
	engine::detail::hash_combine(second, 5u);
	engine::detail::hash_combine(second, 3u);

	//顺序不同则结果不同
	EXPECT_NE(first, second);
}

//极值输入：最大值输入不越界且可复现
TEST_F(Hash_Combine_Test, 最大值输入可复现)
{
	//以最大值为种子与输入
	const size_t result = combine_once(SIZE_MAX, SIZE_MAX);
	//重复执行结果一致
	EXPECT_EQ(result, combine_once(SIZE_MAX, SIZE_MAX));
}

//极值输入：高位为一位宽输入不崩溃
TEST_F(Hash_Combine_Test, 高位输入不崩溃)
{
	//单独置起最高位
	const size_t high_bit = size_t(1) << (sizeof(size_t) * 8 - 1);
	//混合不产生未定义行为
	EXPECT_NO_THROW({
		size_t seed = 0u;
		engine::detail::hash_combine(seed, high_bit);
	});
}

//异常规格：混合函数声明为不抛出
TEST_F(Hash_Combine_Test, 混合函数不抛出)
{
	//编译期即可判定为 noexcept
	EXPECT_TRUE(noexcept(engine::detail::hash_combine(
		std::declval<size_t&>(), std::declval<size_t>())));
}