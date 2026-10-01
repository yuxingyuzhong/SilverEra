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

//未构建任务：普通读取与重置读取均为零
TEST_F(Timer_Test, 未构建任务重置读取仍为零)
{
	//未构建任务的普通读取
	EXPECT_EQ(timer.elapsed("未建任务"), engine::Duration{});
	//未构建任务的重置读取
	EXPECT_EQ(timer.elapsed("未建任务", true), engine::Duration{});
}

//构建后立即读取：间隔非负
TEST_F(Timer_Test, 构建后立即读取非负)
{
	//构建任务
	timer.task_build("即时");
	//立即读取间隔
	const engine::Duration interval = timer.elapsed("即时");
	//间隔不应为负
	EXPECT_GE(interval, engine::Duration{});
}

//重复构建：不重置已累计的计时起点
TEST_F(Timer_Test, 重复构建不重置计时起点)
{
	//构建任务
	EXPECT_TRUE(timer.task_build("重复"));
	//等待十毫秒
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	//读取首次累计
	const engine::Duration before = timer.elapsed("重复");
	//同名任务应构建失败
	EXPECT_FALSE(timer.task_build("重复"));
	//再次读取累计
	const engine::Duration after = timer.elapsed("重复");
	//重复构建不改变计时起点
	EXPECT_GE(after, before);
}

//卸载重建：重新构建后计时原点归零
TEST_F(Timer_Test, 卸载重建后计时归零)
{
	//构建任务并等待二十毫秒
	timer.task_build("重建");
	std::this_thread::sleep_for(std::chrono::milliseconds(20));
	//记录重建前累计
	const engine::Duration before = timer.elapsed("重建");
	//卸载并重新构建
	timer.task_unload("重建");
	timer.task_build("重建");
	//读取重建后累计
	const engine::Duration after = timer.elapsed("重建");
	//重建后累计应小于重建前
	EXPECT_LT(after, before);
}

//卸载后等待：缺失任务不再累计
TEST_F(Timer_Test, 卸载后等待不再累计)
{
	//构建后立即卸载
	timer.task_build("零累计");
	timer.task_unload("零累计");
	//卸载后等待十五毫秒
	std::this_thread::sleep_for(std::chrono::milliseconds(15));
	//缺失任务读取应为零
	EXPECT_EQ(timer.elapsed("零累计"), engine::Duration{});
}

//连续重置：多次重置均能清空累计
TEST_F(Timer_Test, 连续重置均清空累计)
{
	//构建任务并等待二十毫秒
	timer.task_build("幂等");
	std::this_thread::sleep_for(std::chrono::milliseconds(20));
	//记录重置前累计
	const engine::Duration accumulated = timer.elapsed("幂等");
	//第一次带重置读取
	EXPECT_GE(timer.elapsed("幂等", true), engine::Duration{});
	//第一次重置后立即读取应远小于累计
	EXPECT_LT(timer.elapsed("幂等"), accumulated);
	//第二次带重置读取
	EXPECT_GE(timer.elapsed("幂等", true), engine::Duration{});
	//第二次重置后立即读取仍应远小于累计
	EXPECT_LT(timer.elapsed("幂等"), accumulated);
}

//跨阶段累计：多次读取的累计值单调不减
TEST_F(Timer_Test, 跨阶段累计单调不减)
{
	//构建任务
	timer.task_build("单调");
	//上一次读取的间隔
	engine::Duration previous{};
	//分四个阶段推进并比对各阶段累计
	for (int stage = 0; stage < 4; ++stage)
	{
		//每阶段等待五毫秒
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
		//读取当前累计间隔
		const engine::Duration current = timer.elapsed("单调");
		//当前累计不应小于上一阶段
		EXPECT_GE(current, previous);
		//更新上一阶段累计
		previous = current;
	}
}

//状态一致：任务存在返回正值，卸载后归零，重建后恢复正值
TEST_F(Timer_Test, 存在状态与间隔一致)
{
	//构建并等待十毫秒
	timer.task_build("状态");
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	//存在时应有正间隔
	EXPECT_GT(timer.elapsed("状态"), engine::Duration{});
	//卸载后应归零
	EXPECT_TRUE(timer.task_unload("状态"));
	EXPECT_EQ(timer.elapsed("状态"), engine::Duration{});
	//重建并等待后应恢复正间隔
	timer.task_build("状态");
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	EXPECT_GT(timer.elapsed("状态"), engine::Duration{});
}

//独立计时器：实例之间互不共享任务
TEST_F(Timer_Test, 独立计时器互不影响)
{
	//另一个计时器实例
	engine::Timer other;
	//仅在本计时器构建任务
	timer.task_build("独占");
	//等待十毫秒
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	//本计时器应有正间隔
	EXPECT_GT(timer.elapsed("独占"), engine::Duration{});
	//另一实例查询同名任务应为零
	EXPECT_EQ(other.elapsed("独占"), engine::Duration{});
}

//局部计时器：离开作用域后新实例不含旧任务
TEST_F(Timer_Test, 局部计时器析构不残留状态)
{
	{
		//作用域内计时器
		engine::Timer local;
		//构建并等待十毫秒
		local.task_build("局部");
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		//作用域内应有正间隔
		EXPECT_GT(local.elapsed("局部"), engine::Duration{});
	}
	//新实例查询旧任务名应为零
	engine::Timer fresh;
	EXPECT_EQ(fresh.elapsed("局部"), engine::Duration{});
}