//退出暂停判定测试：覆盖「测试结束后是否停住等待」的全部参数组合
#include <gtest/gtest.h>

//获取被测的退出等待判定
#include "src/主调/退出等待判定.h"

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