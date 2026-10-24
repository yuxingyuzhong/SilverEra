//测试选择模型测试：覆盖反射建树、勾选态判定、过滤串生成与控制台输入解析
#include <gtest/gtest.h>

//获取被测的测试选择模型与控制台输入解析
#include "src/主调/测试选择模型.h"

#include <cstddef>
#include <string>
#include <vector>

//在模型中按名称查找套件，命中时写回序号
static bool 查找套件(const engine::Test_Selection_Model& 模型, const std::string& 名称, std::size_t& 序号)
{
    const std::vector<engine::Suite_Entry>& 套件表 = 模型.suite_table_get();
    for (std::size_t i = 0; i < 套件表.size(); ++i)
    {
        if (套件表[i].name == 名称)
        {
            序号 = i;
            return true;
        }
    }
    return false;
}

//找一个用例数不少于两的套件（供「逐条列出」分支使用），返回其序号
static bool 找多用例套件(const engine::Test_Selection_Model& 模型, std::size_t& 序号)
{
    const std::vector<engine::Suite_Entry>& 套件表 = 模型.suite_table_get();
    for (std::size_t i = 0; i < 套件表.size(); ++i)
    {
        if (套件表[i].case_list.size() >= 2)
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
    engine::Test_Selection_Model 模型;
    模型.suite_tree_build();
    //套件数应与反射报告的一致
    EXPECT_EQ(模型.suite_count(),
        static_cast<std::size_t>(::testing::UnitTest::GetInstance()->total_test_suite_count()));
}

//反射建树：用例总数与反射一致
TEST_F(测试选择模型测试, 反射建树用例总数一致)
{
    //建立模型
    engine::Test_Selection_Model 模型;
    模型.suite_tree_build();

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
    EXPECT_EQ(模型.case_count(), 期望总数);
}

//反射建树：默认全部勾选
TEST_F(测试选择模型测试, 默认全部勾选)
{
    //建立模型
    engine::Test_Selection_Model 模型;
    模型.suite_tree_build();
    //已勾选数等于总数
    EXPECT_EQ(模型.checked_case_count(), 模型.case_count());
    //每个套件都处于全选
    for (std::size_t i = 0; i < 模型.suite_count(); ++i)
        EXPECT_TRUE(模型.suite_all_checked(i));
}

//反射建树：树中包含本用例所在套件
TEST_F(测试选择模型测试, 树中包含当前套件)
{
    //建立模型
    engine::Test_Selection_Model 模型;
    模型.suite_tree_build();
    //本套件应可在树中查到
    std::size_t 序号 = 0;
    EXPECT_TRUE(查找套件(模型, "测试选择模型测试", 序号));
}

//半选态：部分勾选时全选为假、半选为真
TEST_F(测试选择模型测试, 半选态判定)
{
    //建立模型
    engine::Test_Selection_Model 模型;
    模型.suite_tree_build();

    //取一个多用例套件
    std::size_t 序号 = 0;
    ASSERT_TRUE(找多用例套件(模型, 序号));
    //取消其中一条用例
    模型.case_check_set(序号, 0, false);
    //整组不再全选，但仍处于半选
    EXPECT_FALSE(模型.suite_all_checked(序号));
    EXPECT_TRUE(模型.suite_half_checked(序号));
}

//半选态：整组取消后全选与半选均为假
TEST_F(测试选择模型测试, 整组取消后无半选)
{
    //建立模型
    engine::Test_Selection_Model 模型;
    模型.suite_tree_build();

    //取一个多用例套件并整组取消
    std::size_t 序号 = 0;
    ASSERT_TRUE(找多用例套件(模型, 序号));
    模型.suite_check_set(序号, false);
    //全选与半选都应为假
    EXPECT_FALSE(模型.suite_all_checked(序号));
    EXPECT_FALSE(模型.suite_half_checked(序号));
}

//批量操作：全选 / 清空 / 反选后的已勾选数正确
TEST_F(测试选择模型测试, 批量勾选操作)
{
    //建立模型
    engine::Test_Selection_Model 模型;
    模型.suite_tree_build();

    //清空后无勾选
    模型.all_clear();
    EXPECT_EQ(模型.checked_case_count(), 0u);

    //全选后勾选数等于总数
    模型.all_check();
    EXPECT_EQ(模型.checked_case_count(), 模型.case_count());

    //反选后全部取消
    模型.check_invert();
    EXPECT_EQ(模型.checked_case_count(), 0u);
}

//展开态：设置后可读回
TEST_F(测试选择模型测试, 套件展开态读写一致)
{
    //建立模型
    engine::Test_Selection_Model 模型;
    模型.suite_tree_build();

    //默认未展开
    EXPECT_FALSE(模型.suite_expanded(0));
    //设置展开后读回为真
    模型.suite_expand_set(0, true);
    EXPECT_TRUE(模型.suite_expanded(0));
    //再设置为折叠后读回为假
    模型.suite_expand_set(0, false);
    EXPECT_FALSE(模型.suite_expanded(0));
}

//过滤串：全不选视为全跑
TEST_F(测试选择模型测试, 过滤串全不选视为全跑)
{
    //建立模型并清空勾选
    engine::Test_Selection_Model 模型;
    模型.suite_tree_build();
    模型.all_clear();
    //过滤串退化为通配
    EXPECT_EQ(模型.filter_string_build(), "*");
}

//过滤串：整套件全选压缩为 套件.*
TEST_F(测试选择模型测试, 过滤串整套件压缩)
{
    //建立模型并清空勾选
    engine::Test_Selection_Model 模型;
    模型.suite_tree_build();
    模型.all_clear();

    //只勾选本套件
    std::size_t 序号 = 0;
    ASSERT_TRUE(查找套件(模型, "测试选择模型测试", 序号));
    模型.suite_check_set(序号, true);
    //整套件全选应压缩成通配形式
    EXPECT_EQ(模型.filter_string_build(), "测试选择模型测试.*");
}

//过滤串：只勾一个用例时逐条列出
TEST_F(测试选择模型测试, 过滤串单用例逐条列出)
{
    //建立模型
    engine::Test_Selection_Model 模型;
    模型.suite_tree_build();

    //取一个多用例套件
    std::size_t 序号 = 0;
    ASSERT_TRUE(找多用例套件(模型, 序号));
    const std::string 用例名 = 模型.suite_table_get()[序号].case_list[0].name;

    //全部清空后只勾该套件的首条用例，过滤串便只含这一条
    模型.all_clear();
    模型.case_check_set(序号, 0, true);
    //应逐条列出（不加通配）
    EXPECT_EQ(模型.filter_string_build(), 模型.suite_table_get()[序号].name + "." + 用例名);
}

//过滤串：多个套件部分勾选时用 ':' 连接
TEST_F(测试选择模型测试, 过滤串多套件拼接)
{
    //建立模型并清空勾选
    engine::Test_Selection_Model 模型;
    模型.suite_tree_build();
    模型.all_clear();

    //找出前两个多用例套件，各只勾首条，确保走「逐条列出」分支
    std::size_t 甲 = 模型.suite_count();
    std::size_t 乙 = 模型.suite_count();
    for (std::size_t i = 0; i < 模型.suite_count() && 乙 == 模型.suite_count(); ++i)
    {
        if (模型.suite_table_get()[i].case_list.size() < 2)
            continue;
        if (甲 == 模型.suite_count())
            甲 = i;
        else
            乙 = i;
    }
    ASSERT_LT(乙, 模型.suite_count());
    模型.case_check_set(甲, 0, true);
    模型.case_check_set(乙, 0, true);

    //两段以 ':' 连接
    const std::string 期望 = 模型.suite_table_get()[甲].name + "." + 模型.suite_table_get()[甲].case_list[0].name
        + ":" + 模型.suite_table_get()[乙].name + "." + 模型.suite_table_get()[乙].case_list[0].name;
    EXPECT_EQ(模型.filter_string_build(), 期望);
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

//空选择集：反选后全部用例转为勾选
TEST_F(测试选择模型测试, 空选择集反选后全部勾选)
{
    //建立模型并清空全部勾选
    engine::Test_Selection_Model 模型;
    模型.suite_tree_build();
    模型.all_clear();
    //清空后确无勾选
    EXPECT_EQ(模型.checked_case_count(), 0u);

    //反选后全部用例转为勾选
    模型.check_invert();
    EXPECT_EQ(模型.checked_case_count(), 模型.case_count());
    //每个套件均回到全选
    for (std::size_t i = 0; i < 模型.suite_count(); ++i)
        EXPECT_TRUE(模型.suite_all_checked(i));
}

//重复选择：重复勾选同一用例与整组均幂等
TEST_F(测试选择模型测试, 重复选择幂等)
{
    //建立模型
    engine::Test_Selection_Model 模型;
    模型.suite_tree_build();

    //取一个多用例套件并清空
    std::size_t 序号 = 0;
    ASSERT_TRUE(找多用例套件(模型, 序号));
    模型.all_clear();

    //重复勾选同一条用例
    模型.case_check_set(序号, 0, true);
    模型.case_check_set(序号, 0, true);
    //重复勾选后仍只有一条被勾选
    EXPECT_EQ(模型.checked_case_count(), 1u);
    //该套件处于半选态
    EXPECT_TRUE(模型.suite_half_checked(序号));

    //重复整组勾选
    模型.suite_check_set(序号, true);
    模型.suite_check_set(序号, true);
    //整组勾选数等于该套件用例数
    EXPECT_EQ(模型.checked_case_count(), 模型.suite_table_get()[序号].case_list.size());
    //该套件处于全选态
    EXPECT_TRUE(模型.suite_all_checked(序号));
}

//越界索引：越界查询为假、越界设置为空操作且不改动勾选总数
TEST_F(测试选择模型测试, 越界索引操作被忽略)
{
    //建立模型
    engine::Test_Selection_Model 模型;
    模型.suite_tree_build();

    //越界套件序号与越界用例序号
    const std::size_t 越界套件 = 模型.suite_count();
    const std::size_t 越界用例 = 模型.case_count() + 1;
    //记录操作前的已勾选数
    const std::size_t 原勾选数 = 模型.checked_case_count();

    //越界查询一律为假
    EXPECT_FALSE(模型.suite_all_checked(越界套件));
    EXPECT_FALSE(模型.suite_half_checked(越界套件));
    EXPECT_FALSE(模型.suite_expanded(越界套件));

    //越界设置不应崩溃
    EXPECT_NO_THROW(模型.suite_check_set(越界套件, false));
    EXPECT_NO_THROW(模型.suite_expand_set(越界套件, true));
    EXPECT_NO_THROW(模型.case_check_set(越界套件, 0, false));
    EXPECT_NO_THROW(模型.case_check_set(0, 越界用例, false));

    //越界操作不改变勾选总数
    EXPECT_EQ(模型.checked_case_count(), 原勾选数);
}

//全选：清空后再全选应恢复全部勾选
TEST_F(测试选择模型测试, 清空后再全选恢复)
{
    //建立模型
    engine::Test_Selection_Model 模型;
    模型.suite_tree_build();

    //清空全部勾选
    模型.all_clear();
    EXPECT_EQ(模型.checked_case_count(), 0u);

    //再次全选应恢复全部勾选
    模型.all_check();
    EXPECT_EQ(模型.checked_case_count(), 模型.case_count());
    //每个非空套件都应回到全选
    for (std::size_t i = 0; i < 模型.suite_count(); ++i)
    {
        //空套件不参与判定
        if (模型.suite_table_get()[i].case_list.empty())
            continue;
        EXPECT_TRUE(模型.suite_all_checked(i));
    }
}

//全不选：全部清空后不存在全选与半选态
TEST_F(测试选择模型测试, 全不选无半选态)
{
    //建立模型并清空全部勾选
    engine::Test_Selection_Model 模型;
    模型.suite_tree_build();
    模型.all_clear();

    //全不选后勾选数为零
    EXPECT_EQ(模型.checked_case_count(), 0u);
    //每个套件既不处于全选也不处于半选
    for (std::size_t i = 0; i < 模型.suite_count(); ++i)
    {
        EXPECT_FALSE(模型.suite_all_checked(i));
        EXPECT_FALSE(模型.suite_half_checked(i));
    }
}

//按前缀筛选：多个整套件全选压缩为前缀通配并以冒号连接
TEST_F(测试选择模型测试, 按前缀筛选多套件拼接)
{
    //建立模型
    engine::Test_Selection_Model 模型;
    模型.suite_tree_build();
    //本用例需至少两个套件
    ASSERT_GE(模型.suite_count(), 2u);
    //前两个套件均非空
    ASSERT_FALSE(模型.suite_table_get()[0].case_list.empty());
    ASSERT_FALSE(模型.suite_table_get()[1].case_list.empty());

    //清空后仅整组勾选前两个套件
    模型.all_clear();
    模型.suite_check_set(0, true);
    模型.suite_check_set(1, true);

    //两段应各自压缩为 套件.* 并以冒号连接
    const std::string 期望 = 模型.suite_table_get()[0].name + ".*:"
        + 模型.suite_table_get()[1].name + ".*";
    EXPECT_EQ(模型.filter_string_build(), 期望);
}

//非法筛选串：非法输入判非法且勾选表被重置为全假
TEST_F(测试选择模型测试, 非法筛选输入重置勾选表)
{
    //预填为全真的勾选表
    std::vector<bool> 勾选表(3, true);

    //非数字文本判非法
    EXPECT_FALSE(engine::解析控制台选择("abc", 3, 勾选表));
    //勾选表长度归位为套件数
    EXPECT_EQ(勾选表.size(), 3u);
    //非法输入后勾选表被重置为全假
    for (bool 值 : 勾选表)
        EXPECT_FALSE(值);

    //空段判非法
    EXPECT_FALSE(engine::解析控制台选择("1,,3", 3, 勾选表));
    EXPECT_EQ(勾选表.size(), 3u);

    //超长编号判非法（超出内部防御阈值）
    EXPECT_FALSE(engine::解析控制台选择("9999999", 3, 勾选表));
    EXPECT_EQ(勾选表.size(), 3u);
}

//选择结果：解析结果与输入编号一一对应
TEST_F(测试选择模型测试, 选择结果与输入一一对应)
{
    //解析不连续编号
    std::vector<bool> 勾选表;
    ASSERT_TRUE(engine::解析控制台选择("2,4", 5, 勾选表));
    const std::vector<bool> 期望 = { false, true, false, true, false };
    EXPECT_EQ(勾选表, 期望);

    //重复编号不产生额外勾选
    ASSERT_TRUE(engine::解析控制台选择("3,3", 4, 勾选表));
    const std::vector<bool> 期望重复 = { false, false, true, false };
    EXPECT_EQ(勾选表, 期望重复);

    //末位编号命中最后一项
    ASSERT_TRUE(engine::解析控制台选择("4", 4, 勾选表));
    EXPECT_TRUE(勾选表[3]);
}
