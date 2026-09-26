//测试选择模型测试：覆盖反射建树、勾选态判定、过滤串生成与控制台输入解析
#include <gtest/gtest.h>

//获取被测的测试选择模型与控制台输入解析
#include "src/主调/测试选择模型.h"

#include <cstddef>
#include <string>
#include <vector>

//在模型中按名称查找套件，命中时写回序号
static bool 查找套件(const engine::测试选择模型& 模型, const std::string& 名称, std::size_t& 序号)
{
    const std::vector<engine::套件条目>& 套件表 = 模型.套件表();
    for (std::size_t i = 0; i < 套件表.size(); ++i)
    {
        if (套件表[i].名称 == 名称)
        {
            序号 = i;
            return true;
        }
    }
    return false;
}

//找一个用例数不少于两的套件（供「逐条列出」分支使用），返回其序号
static bool 找多用例套件(const engine::测试选择模型& 模型, std::size_t& 序号)
{
    const std::vector<engine::套件条目>& 套件表 = 模型.套件表();
    for (std::size_t i = 0; i < 套件表.size(); ++i)
    {
        if (套件表[i].用例表.size() >= 2)
        {
            序号 = i;
            return true;
        }
    }
    return false;
}

//测试选择模型测试夹具
class 测试选择模型测试 : public ::testing::Test
{
};

//反射建树：套件数与 googletest 反射一致
TEST_F(测试选择模型测试, 反射建树套件数一致)
{
    //建立模型
    engine::测试选择模型 模型;
    模型.建立套件树();
    //套件数应与反射报告的一致
    EXPECT_EQ(模型.套件数(),
        static_cast<std::size_t>(::testing::UnitTest::GetInstance()->total_test_suite_count()));
}

//反射建树：用例总数与反射一致
TEST_F(测试选择模型测试, 反射建树用例总数一致)
{
    //建立模型
    engine::测试选择模型 模型;
    模型.建立套件树();

    //按反射统计用例总数
    ::testing::UnitTest* 单元测试 = ::testing::UnitTest::GetInstance();
    std::size_t 期望总数 = 0;
    for (int i = 0; i < 单元测试->total_test_suite_count(); ++i)
    {
        const ::testing::TestSuite* 套件 = 单元测试->GetTestSuite(i);
        if (套件 != nullptr)
            期望总数 += static_cast<std::size_t>(套件->total_test_count());
    }
    //用例总数应与反射一致
    EXPECT_EQ(模型.用例总数(), 期望总数);
}

//反射建树：默认全部勾选
TEST_F(测试选择模型测试, 默认全部勾选)
{
    //建立模型
    engine::测试选择模型 模型;
    模型.建立套件树();
    //已勾选数等于总数
    EXPECT_EQ(模型.已勾选用例数(), 模型.用例总数());
    //每个套件都处于全选
    for (std::size_t i = 0; i < 模型.套件数(); ++i)
        EXPECT_TRUE(模型.套件全选(i));
}

//反射建树：树中包含本用例所在套件
TEST_F(测试选择模型测试, 树中包含当前套件)
{
    //建立模型
    engine::测试选择模型 模型;
    模型.建立套件树();
    //本套件应可在树中查到
    std::size_t 序号 = 0;
    EXPECT_TRUE(查找套件(模型, "测试选择模型测试", 序号));
}

//半选态：部分勾选时全选为假、半选为真
TEST_F(测试选择模型测试, 半选态判定)
{
    //建立模型
    engine::测试选择模型 模型;
    模型.建立套件树();

    //取一个多用例套件
    std::size_t 序号 = 0;
    ASSERT_TRUE(找多用例套件(模型, 序号));
    //取消其中一条用例
    模型.设置用例勾选(序号, 0, false);
    //整组不再全选，但仍处于半选
    EXPECT_FALSE(模型.套件全选(序号));
    EXPECT_TRUE(模型.套件半选(序号));
}

//半选态：整组取消后全选与半选均为假
TEST_F(测试选择模型测试, 整组取消后无半选)
{
    //建立模型
    engine::测试选择模型 模型;
    模型.建立套件树();

    //取一个多用例套件并整组取消
    std::size_t 序号 = 0;
    ASSERT_TRUE(找多用例套件(模型, 序号));
    模型.设置套件勾选(序号, false);
    //全选与半选都应为假
    EXPECT_FALSE(模型.套件全选(序号));
    EXPECT_FALSE(模型.套件半选(序号));
}

//批量操作：全选 / 清空 / 反选后的已勾选数正确
TEST_F(测试选择模型测试, 批量勾选操作)
{
    //建立模型
    engine::测试选择模型 模型;
    模型.建立套件树();

    //清空后无勾选
    模型.全部清空();
    EXPECT_EQ(模型.已勾选用例数(), 0u);

    //全选后勾选数等于总数
    模型.全部勾选();
    EXPECT_EQ(模型.已勾选用例数(), 模型.用例总数());

    //反选后全部取消
    模型.反向勾选();
    EXPECT_EQ(模型.已勾选用例数(), 0u);
}

//展开态：设置后可读回
TEST_F(测试选择模型测试, 套件展开态读写一致)
{
    //建立模型
    engine::测试选择模型 模型;
    模型.建立套件树();

    //默认未展开
    EXPECT_FALSE(模型.套件已展开(0));
    //设置展开后读回为真
    模型.设置套件展开(0, true);
    EXPECT_TRUE(模型.套件已展开(0));
    //再设置为折叠后读回为假
    模型.设置套件展开(0, false);
    EXPECT_FALSE(模型.套件已展开(0));
}

//过滤串：全不选视为全跑
TEST_F(测试选择模型测试, 过滤串全不选视为全跑)
{
    //建立模型并清空勾选
    engine::测试选择模型 模型;
    模型.建立套件树();
    模型.全部清空();
    //过滤串退化为通配
    EXPECT_EQ(模型.生成过滤串(), "*");
}

//过滤串：整套件全选压缩为 套件.*
TEST_F(测试选择模型测试, 过滤串整套件压缩)
{
    //建立模型并清空勾选
    engine::测试选择模型 模型;
    模型.建立套件树();
    模型.全部清空();

    //只勾选本套件
    std::size_t 序号 = 0;
    ASSERT_TRUE(查找套件(模型, "测试选择模型测试", 序号));
    模型.设置套件勾选(序号, true);
    //整套件全选应压缩成通配形式
    EXPECT_EQ(模型.生成过滤串(), "测试选择模型测试.*");
}

//过滤串：只勾一个用例时逐条列出
TEST_F(测试选择模型测试, 过滤串单用例逐条列出)
{
    //建立模型
    engine::测试选择模型 模型;
    模型.建立套件树();

    //取一个多用例套件
    std::size_t 序号 = 0;
    ASSERT_TRUE(找多用例套件(模型, 序号));
    const std::string 用例名 = 模型.套件表()[序号].用例表[0].名称;

    //全部清空后只勾该套件的首条用例，过滤串便只含这一条
    模型.全部清空();
    模型.设置用例勾选(序号, 0, true);
    //应逐条列出（不加通配）
    EXPECT_EQ(模型.生成过滤串(), 模型.套件表()[序号].名称 + "." + 用例名);
}

//过滤串：多个套件部分勾选时用 ':' 连接
TEST_F(测试选择模型测试, 过滤串多套件拼接)
{
    //建立模型并清空勾选
    engine::测试选择模型 模型;
    模型.建立套件树();
    模型.全部清空();

    //找出前两个多用例套件，各只勾首条，确保走「逐条列出」分支
    std::size_t 甲 = 模型.套件数();
    std::size_t 乙 = 模型.套件数();
    for (std::size_t i = 0; i < 模型.套件数() && 乙 == 模型.套件数(); ++i)
    {
        if (模型.套件表()[i].用例表.size() < 2)
            continue;
        if (甲 == 模型.套件数())
            甲 = i;
        else
            乙 = i;
    }
    ASSERT_LT(乙, 模型.套件数());
    模型.设置用例勾选(甲, 0, true);
    模型.设置用例勾选(乙, 0, true);

    //两段以 ':' 连接
    const std::string 期望 = 模型.套件表()[甲].名称 + "." + 模型.套件表()[甲].用例表[0].名称
        + ":" + 模型.套件表()[乙].名称 + "." + 模型.套件表()[乙].用例表[0].名称;
    EXPECT_EQ(模型.生成过滤串(), 期望);
}

//控制台解析：all 全选
TEST_F(测试选择模型测试, 控制台解析全选)
{
    //解析 all
    std::vector<bool> 勾选表;
    ASSERT_TRUE(engine::解析控制台选择("all", 4, 勾选表));
    //结果长度正确且全为真
    ASSERT_EQ(勾选表.size(), 4u);
    for (bool 值 : 勾选表)
        EXPECT_TRUE(值);
}

//控制台解析：单编号与区间混合
TEST_F(测试选择模型测试, 控制台解析编号与区间)
{
    //解析 "1,3-5"
    std::vector<bool> 勾选表;
    ASSERT_TRUE(engine::解析控制台选择("1,3-5", 6, 勾选表));
    //编号从 1 开始，区间展开为连续勾选
    const std::vector<bool> 期望 = { true, false, true, true, true, false };
    EXPECT_EQ(勾选表, 期望);
}

//控制台解析：空段与非法字符判非法
TEST_F(测试选择模型测试, 控制台解析非法文本)
{
    std::vector<bool> 勾选表;
    //非数字文本
    EXPECT_FALSE(engine::解析控制台选择("abc", 3, 勾选表));
    //空段
    EXPECT_FALSE(engine::解析控制台选择("1,,3", 3, 勾选表));
    //区间顺序颠倒
    EXPECT_FALSE(engine::解析控制台选择("5-2", 6, 勾选表));
    //全空白
    EXPECT_FALSE(engine::解析控制台选择("   ", 3, 勾选表));
}

//控制台解析：越界编号判非法
TEST_F(测试选择模型测试, 控制台解析越界编号)
{
    std::vector<bool> 勾选表;
    //编号从 1 开始，0 非法
    EXPECT_FALSE(engine::解析控制台选择("0", 3, 勾选表));
    //超出套件总数非法
    EXPECT_FALSE(engine::解析控制台选择("4", 3, 勾选表));
    //区间右端超出套件总数非法
    EXPECT_FALSE(engine::解析控制台选择("2-9", 3, 勾选表));
}
