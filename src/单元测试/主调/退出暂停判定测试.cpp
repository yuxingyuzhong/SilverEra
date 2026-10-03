//退出暂停判定测试：覆盖「测试结束后是否停住等待」的全部参数组合
#include <gtest/gtest.h>

//获取被测的退出等待判定
#include "Application/Test/src/主调/退出等待判定.h"

#include <string>

//退出暂停判定测试夹具
class 退出暂停判定测试 : public ::testing::Test
{
};

//默认运行（无参数）：交互运行，需要等待
TEST_F(退出暂停判定测试, 默认模式需要等待)
{
    EXPECT_TRUE(engine::需要等待退出("window", false, false));
}

//显式指定窗口模式：仍是交互运行，需要等待
TEST_F(退出暂停判定测试, 显式窗口模式需要等待)
{
    EXPECT_TRUE(engine::需要等待退出("window", false, true));
}

//显式指定控制台菜单：仍是交互运行，需要等待
TEST_F(退出暂停判定测试, 显式控制台模式需要等待)
{
    EXPECT_TRUE(engine::需要等待退出("console", false, true));
}

//关闭选择器：ctest 的调用形态，不等待
TEST_F(退出暂停判定测试, 关闭选择器不等待)
{
    EXPECT_FALSE(engine::需要等待退出("off", false, false));
    EXPECT_FALSE(engine::需要等待退出("off", false, true));
}

//gtest 参数透传（未显式指定选择器）：自动化调用，不等待
TEST_F(退出暂停判定测试, 过滤串透传不等待)
{
    EXPECT_FALSE(engine::需要等待退出("window", true, false));
    EXPECT_FALSE(engine::需要等待退出("console", true, false));
    EXPECT_FALSE(engine::需要等待退出("off", true, false));
}

//显式选择器与过滤串并存：显式指定选择器优先，仍按交互运行处理
TEST_F(退出暂停判定测试, 显式选择器优先于透传)
{
    EXPECT_TRUE(engine::需要等待退出("window", true, true));
    EXPECT_TRUE(engine::需要等待退出("console", true, true));
}

//关闭选择器与过滤串并存：不等待
TEST_F(退出暂停判定测试, 显式选择器关闭且带过滤串不等待)
{
    EXPECT_FALSE(engine::需要等待退出("off", true, true));
}

//未知选择模式：按交互运行兜底，宁可多等一次也不要一闪而过
TEST_F(退出暂停判定测试, 未知模式需要等待)
{
    EXPECT_TRUE(engine::需要等待退出("未知模式", false, true));
}

//未知模式与过滤串透传：透传优先，不等待
TEST_F(退出暂停判定测试, 未知模式带过滤串透传不等待)
{
    EXPECT_FALSE(engine::需要等待退出("未知模式", true, false));
}

//未知模式与显式选择器：显式选择优先，仍需等待
TEST_F(退出暂停判定测试, 未知模式显式选择仍等待)
{
    EXPECT_TRUE(engine::需要等待退出("未知模式", true, true));
    EXPECT_TRUE(engine::需要等待退出("未知模式", false, true));
}

//空模式串：按未知模式兜底
TEST_F(退出暂停判定测试, 空模式串按未知模式兜底)
{
    //空模式串默认交互运行
    EXPECT_TRUE(engine::需要等待退出("", false, false));
    EXPECT_TRUE(engine::需要等待退出("", false, true));
    //带过滤且未显式指定时按透传处理
    EXPECT_FALSE(engine::需要等待退出("", true, false));
}

//模式判定大小写敏感：仅小写 off 视为关闭
TEST_F(退出暂停判定测试, 模式大小写敏感)
{
    //大写 OFF 不等于 off，按未知模式处理
    EXPECT_TRUE(engine::需要等待退出("OFF", false, true));
    //透传仍优先
    EXPECT_FALSE(engine::需要等待退出("OFF", true, false));
}

//重复调用：同参数重复请求结果一致（幂等）
TEST_F(退出暂停判定测试, 重复调用判定结果幂等)
{
    //重复请求同一交互组合应得到一致结果
    EXPECT_EQ(engine::需要等待退出("window", false, false),
        engine::需要等待退出("window", false, false));
    EXPECT_EQ(engine::需要等待退出("console", true, true),
        engine::需要等待退出("console", true, true));
    EXPECT_EQ(engine::需要等待退出("off", false, false),
        engine::需要等待退出("off", false, false));
}

//全参数组合：遍历选择模式与两个布尔量，结果与判定表一致
TEST_F(退出暂停判定测试, 全参数组合与判定表一致)
{
    //待遍历的选择模式集合
    const char* 模式集合[] = { "window", "console", "off", "未知模式", "" };
    for (const char* 模式 : 模式集合)
    {
        for (int 带过滤 = 0; 带过滤 <= 1; ++带过滤)
        {
            for (int 显式 = 0; 显式 <= 1; ++显式)
            {
                //透传：带过滤且未显式指定选择器
                const bool 透传 = (带过滤 != 0) && (显式 == 0);
                //显式关闭选择器
                const bool 关闭 = std::string(模式) == "off";
                //参考判定表：透传或关闭时不等待，其余等待
                const bool 期望 = !(透传 || 关闭);
                EXPECT_EQ(engine::需要等待退出(模式, 带过滤 != 0, 显式 != 0), 期望);
            }
        }
    }
}