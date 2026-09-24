//计时器测试：覆盖任务构建与卸载、重复构建、缺失任务查询、重置语义与时间单位换算
#include <gtest/gtest.h>

//获取计时器
#include "src/tools/Timer/计时器.h"

//计时器测试夹具
class Timer_Test : public ::testing::Test
{
protected:
	//被测计时器
	engine::Timer timer;
};

//构建计时任务：新任务构建成功
TEST_F(Timer_Test, 构建新任务成功)
{
	//首次构建应成功
	EXPECT_TRUE(timer.task_build("渲染"));
}

//重复构建：同名任务第二次构建失败
TEST_F(Timer_Test, 重复构建同名任务失败)
{
	//首次构建成功
	EXPECT_TRUE(timer.task_build("渲染"));
	//同名任务二次构建失败
	EXPECT_FALSE(timer.task_build("渲染"));
}

//卸载任务：已存在任务卸载成功
TEST_F(Timer_Test, 卸载已存在任务成功)
{
	//先构建任务
	timer.task_build("渲染");
	//卸载该任务应成功
	EXPECT_TRUE(timer.task_unload("渲染"));
}

//卸载任务：不存在任务卸载失败
TEST_F(Timer_Test, 卸载不存在任务失败)
{
	//未构建过的任务卸载应失败
	EXPECT_FALSE(timer.task_unload("物理"));
}

//卸载后可重建：任务名重新可用
TEST_F(Timer_Test, 卸载后可重新构建)
{
	//构建后卸载
	timer.task_build("渲染");
	timer.task_unload("渲染");
	//同名任务可再次构建
	EXPECT_TRUE(timer.task_build("渲染"));
}

//缺失任务查询：返回零时长
TEST_F(Timer_Test, 缺失任务返回零时长)
{
	//未构建任务查询间隔
	const engine::Duration interval = timer.elapsed("物理");
	//零时长应等于默认构造的时长
	EXPECT_EQ(interval, engine::Duration{});
}

//时间累计：构建后经过一段时间可测出正间隔
TEST_F(Timer_Test, 构建后可测出正间隔)
{
	//构建任务
	timer.task_build("渲染");
	//等待十毫秒
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	//读取间隔
	const engine::Duration interval = timer.elapsed("渲染");
	//间隔应为正
	EXPECT_GT(interval, engine::Duration{});
	//换算到毫秒后应不小于十毫秒
	EXPECT_GE(engine::Timer::Milli_units(interval), 10.0);
}

//多次读取不重置：间隔随等待单调不减
TEST_F(Timer_Test, 多次读取不重置)
{
	//构建任务
	timer.task_build("加载");
	//等待并读取第一次
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	const engine::Duration first = timer.elapsed("加载");
	//再等待并读取第二次
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	const engine::Duration second = timer.elapsed("加载");
	//第二次读取的累计间隔应不小于第一次
	EXPECT_GE(second, first);
}

//重置读取：重启计时原点后间隔重新累计
TEST_F(Timer_Test, 重置读取重新计次)
{
	//构建任务
	timer.task_build("加载");
	//等待二十毫秒后带重置读取
	std::this_thread::sleep_for(std::chrono::milliseconds(20));
	const engine::Duration first = timer.elapsed("加载", true);
	//紧接着再读取一次
	const engine::Duration second = timer.elapsed("加载");
	//重置后的间隔应明显小于重置前累计值
	EXPECT_LT(second, first);
}

//多任务隔离：互不干扰且可分别卸载
TEST_F(Timer_Test, 多任务互相隔离)
{
	//构建两个任务
	EXPECT_TRUE(timer.task_build("渲染"));
	EXPECT_TRUE(timer.task_build("物理"));
	//等待十毫秒
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	//两个任务都应测出正间隔
	EXPECT_GT(timer.elapsed("渲染"), engine::Duration{});
	EXPECT_GT(timer.elapsed("物理"), engine::Duration{});
	//卸载其中一个任务
	EXPECT_TRUE(timer.task_unload("渲染"));
	//另一个任务不受影响
	EXPECT_GT(timer.elapsed("物理"), engine::Duration{});
	//被卸载的任务归零
	EXPECT_EQ(timer.elapsed("渲染"), engine::Duration{});
}

//秒级换算：一秒时长换算为一
TEST_F(Timer_Test, 秒级换算)
{
	//一秒时长
	const engine::Duration one_second = std::chrono::seconds(1);
	//秒级换算结果
	EXPECT_DOUBLE_EQ(engine::Timer::units(one_second), 1.0);
}

//毫秒级换算：一秒时长换算为一千
TEST_F(Timer_Test, 毫秒级换算)
{
	//一秒时长
	const engine::Duration one_second = std::chrono::seconds(1);
	//毫秒级换算结果
	EXPECT_DOUBLE_EQ(engine::Timer::Milli_units(one_second), 1000.0);
}

//微秒级换算：一秒时长换算为一百万
TEST_F(Timer_Test, 微秒级换算)
{
	//一秒时长
	const engine::Duration one_second = std::chrono::seconds(1);
	//微秒级换算结果
	EXPECT_DOUBLE_EQ(engine::Timer::Micro_units(one_second), 1000000.0);
}

//纳秒级换算：一秒时长换算为十亿
TEST_F(Timer_Test, 纳秒级换算)
{
	//一秒时长
	const engine::Duration one_second = std::chrono::seconds(1);
	//纳秒级换算结果
	EXPECT_EQ(engine::Timer::Nano_units(one_second), 1000000000LL);
}

//非整秒换算：毫秒输入得到小数秒
TEST_F(Timer_Test, 非整秒换算为小数)
{
	//一秒半时长
	const engine::Duration span = std::chrono::milliseconds(1500);
	//秒级换算结果
	EXPECT_DOUBLE_EQ(engine::Timer::units(span), 1.5);
	//毫秒级换算结果
	EXPECT_DOUBLE_EQ(engine::Timer::Milli_units(span), 1500.0);
}

//小单位换算：微秒输入在各精度下一致
TEST_F(Timer_Test, 微秒输入的各级换算)
{
	//二百五十微秒
	const engine::Duration span = std::chrono::microseconds(250);
	//微秒级换算结果
	EXPECT_DOUBLE_EQ(engine::Timer::Micro_units(span), 250.0);
	//纳秒级换算结果
	EXPECT_EQ(engine::Timer::Nano_units(span), 250000LL);
	//毫秒级换算结果
	EXPECT_DOUBLE_EQ(engine::Timer::Milli_units(span), 0.25);
}

//零时长换算：各级换算结果均为零
TEST_F(Timer_Test, 零时长换算为零)
{
	//默认构造的时长
	const engine::Duration zero{};
	//秒级换算结果
	EXPECT_DOUBLE_EQ(engine::Timer::units(zero), 0.0);
	//毫秒级换算结果
	EXPECT_DOUBLE_EQ(engine::Timer::Milli_units(zero), 0.0);
	//纳秒级换算结果
	EXPECT_EQ(engine::Timer::Nano_units(zero), 0LL);
}