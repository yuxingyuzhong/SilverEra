//随机数生成器测试：覆盖同种子可复现性、不同种子差异、闭区间取值、区间交换与范围覆盖
#include <gtest/gtest.h>

//获取随机数生成器
#include "src/tools/Random/随机数生成器.h"

//随机数生成器测试夹具
class Random_Generator_Test : public ::testing::Test
{
};

//同种子可复现：两个同种子生成器序列完全一致
TEST_F(Random_Generator_Test, 同种子序列一致)
{
	//同种子生成器
	engine::Random_Generator first(12345);
	engine::Random_Generator second(12345);
	//逐位比较一百个随机数
	for (int i = 0; i < 100; ++i)
		EXPECT_EQ(first(), second());
}

//不同种子：序列出现差异
TEST_F(Random_Generator_Test, 不同种子序列不同)
{
	//不同种子生成器
	engine::Random_Generator first(1);
	engine::Random_Generator second(2);
	//至少有一位取值不同
	bool different = false;
	for (int i = 0; i < 10; ++i)
	{
		if (first() != second())
			different = true;
	}
	//不同种子不应产生完全相同的序列
	EXPECT_TRUE(different);
}

//重设种子：重置后可复现原序列
TEST_F(Random_Generator_Test, 重设种子后复现序列)
{
	//固定种子生成器
	engine::Random_Generator generator(777);
	//记录首批取值
	std::vector<int64_t> first_round;
	for (int i = 0; i < 20; ++i)
		first_round.push_back(generator());
	//重设同一组种子
	generator.pcg32_seed_init(777);
	//第二批取值应与首批完全一致
	for (int i = 0; i < 20; ++i)
		EXPECT_EQ(generator(), first_round[i]);
}

//闭区间取值：产生的数值落在指定范围内
TEST_F(Random_Generator_Test, 闭区间内取值)
{
	//固定种子生成器
	engine::Random_Generator generator(2024);
	//重复抽样一千次
	for (int i = 0; i < 1000; ++i)
	{
		//获取区间随机数
		const int64_t value = generator(0, 9);
		//下界检查
		EXPECT_GE(value, 0);
		//上界检查
		EXPECT_LE(value, 9);
	}
}

//负区间取值：范围跨越零点时同样受约束
TEST_F(Random_Generator_Test, 负区间内取值)
{
	//固定种子生成器
	engine::Random_Generator generator(2025);
	//重复抽样一千次
	for (int i = 0; i < 1000; ++i)
	{
		//获取跨零区间随机数
		const int64_t value = generator(-5, 5);
		//下界检查
		EXPECT_GE(value, -5);
		//上界检查
		EXPECT_LE(value, 5);
	}
}

//全负区间：两端均为负数时仍受约束
TEST_F(Random_Generator_Test, 全负区间内取值)
{
	//固定种子生成器
	engine::Random_Generator generator(2026);
	//重复抽样一千次
	for (int i = 0; i < 1000; ++i)
	{
		//获取全负区间随机数
		const int64_t value = generator(-100, -50);
		//下界检查
		EXPECT_GE(value, -100);
		//上界检查
		EXPECT_LE(value, -50);
	}
}

//单点区间：范围退化时返回该点本身
TEST_F(Random_Generator_Test, 单点区间返回定值)
{
	//固定种子生成器
	engine::Random_Generator generator(7);
	//重复抽样一百次
	for (int i = 0; i < 100; ++i)
		EXPECT_EQ(generator(7, 7), 7);
}

//区间颠倒：参数逆序时自动交换后仍受约束
TEST_F(Random_Generator_Test, 区间颠倒自动交换)
{
	//固定种子生成器
	engine::Random_Generator generator(11);
	//重复抽样一千次
	for (int i = 0; i < 1000; ++i)
	{
		//以颠倒顺序传入区间
		const int64_t value = generator(10, 1);
		//下界检查
		EXPECT_GE(value, 1);
		//上界检查
		EXPECT_LE(value, 10);
	}
}

//区间覆盖：小范围抽样可覆盖全部取值
TEST_F(Random_Generator_Test, 小范围抽样覆盖全部取值)
{
	//固定种子生成器
	engine::Random_Generator generator(99);
	//取值命中标记
	std::set<int64_t> hit;
	//抽样一万次
	for (int i = 0; i < 10000; ++i)
		hit.insert(generator(0, 9));
	//零到九应全部出现
	EXPECT_EQ(hit.size(), 10u);
}

//全范围生成：无参调用不抛异常且连续取值不重复
TEST_F(Random_Generator_Test, 全范围生成不重复)
{
	//固定种子生成器
	engine::Random_Generator generator(1234);
	//记录首批取值
	std::set<int64_t> values;
	//连续生成一百个全范围随机数
	for (int i = 0; i < 100; ++i)
		values.insert(generator());
	//全范围取值几乎不可能出现重复
	EXPECT_GT(values.size(), 90u);
}

//极值区间：跨越 int64 全范围时仍可返回
TEST_F(Random_Generator_Test, 极值区间不溢出)
{
	//固定种子生成器
	engine::Random_Generator generator(555);
	//在最小值与临近区间抽样
	for (int i = 0; i < 100; ++i)
	{
		//获取靠近下界的区间随机数
		const int64_t value = generator(INT64_MIN, INT64_MIN + 9);
		//下界检查
		EXPECT_GE(value, INT64_MIN);
		//上界检查
		EXPECT_LE(value, INT64_MIN + 9);
	}
}

//无种子构造：缺省构造后仍能连续产生不同取值
TEST_F(Random_Generator_Test, 无种子构造可用)
{
	//缺省构造生成器
	engine::Random_Generator generator;
	//连续两次取值
	const int64_t first = generator();
	const int64_t second = generator();
	//两次取值应不同
	EXPECT_NE(first, second);
}