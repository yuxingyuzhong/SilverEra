//测试层级状态测试：覆盖版本比对（一致 / 不一致 / 记录缺层）与状态表读取的基本契约
#include <gtest/gtest.h>

//获取被测的层级状态模块
#include "src/主调/测试层级状态.h"


//测试层级状态测试夹具
class 测试层级状态测试 : public ::testing::Test
{
};

//版本比对：记录与当前完全一致时无待更新
TEST_F(测试层级状态测试, 版本一致无待更新)
{
    const std::map<std::string, std::string> 记录 = {
        { "EngineFoundation", "e046a4cd8ff3" },
        { "EngineSystem", "2f8c78abcabf" }
    };
    const std::map<std::string, std::string> 当前 = 记录;

    std::vector<std::string> 原因;
    EXPECT_FALSE(engine::version_mismatch(记录, 当前, 原因));
    EXPECT_TRUE(原因.empty());
}

//版本比对：任一层的哈希不同即待更新，并给出该层原因
TEST_F(测试层级状态测试, 版本不一致判待更新)
{
    const std::map<std::string, std::string> 记录 = {
        { "EngineFoundation", "e046a4cd8ff3" },
        { "EngineSystem", "2f8c78abcabf" }
    };
    const std::map<std::string, std::string> 当前 = {
        { "EngineFoundation", "e046a4cd8ff3" },
        { "EngineSystem", "9d9d9d9d9d9d" }
    };

    std::vector<std::string> 原因;
    EXPECT_TRUE(engine::version_mismatch(记录, 当前, 原因));
    ASSERT_EQ(原因.size(), 1u);
    //原因里应同时出现记录值与当前值
    EXPECT_NE(原因[0].find("2f8c78abcabf"), std::string::npos);
    EXPECT_NE(原因[0].find("9d9d9d9d9d9d"), std::string::npos);
}

//版本比对：记录缺少某层同样判待更新
TEST_F(测试层级状态测试, 记录缺层判待更新)
{
    const std::map<std::string, std::string> 记录 = {
        { "EngineFoundation", "e046a4cd8ff3" }
    };
    const std::map<std::string, std::string> 当前 = {
        { "EngineFoundation", "e046a4cd8ff3" },
        { "EngineSystem", "2f8c78abcabf" }
    };

    std::vector<std::string> 原因;
    EXPECT_TRUE(engine::version_mismatch(记录, 当前, 原因));
    ASSERT_EQ(原因.size(), 1u);
    EXPECT_NE(原因[0].find("EngineSystem"), std::string::npos);
}

//版本比对：当前版本未知（空）时不参与判定
TEST_F(测试层级状态测试, 当前版本未知不参与判定)
{
    const std::map<std::string, std::string> 记录 = {
        { "EngineFoundation", "e046a4cd8ff3" }
    };
    const std::map<std::string, std::string> 当前 = {
        { "EngineFoundation", "" }
    };

    std::vector<std::string> 原因;
    EXPECT_FALSE(engine::version_mismatch(记录, 当前, 原因));
}

//状态表：四个层级各一条，层级编号与名称齐备
TEST_F(测试层级状态测试, 状态表覆盖四个层级)
{
    const std::vector<engine::Layer_Version_State> 状态表 = engine::layer_version_states_get();
    ASSERT_EQ(状态表.size(), static_cast<std::size_t>(engine::test_layer_count));

    //层级编号按枚举顺序排列
    for (std::size_t i = 0; i < 状态表.size(); ++i)
    {
        EXPECT_EQ(static_cast<std::size_t>(状态表[i].layer), i);
        EXPECT_STRNE(engine::test_layer_name(状态表[i].layer), "");
    }
    //待更新时必须有原因，供界面展示
    for (const engine::Layer_Version_State& 状态 : 状态表)
    {
        if (状态.outdated)
            EXPECT_FALSE(状态.reasons.empty());
    }
}

//配置名：只可能是 Debug / Release
TEST_F(测试层级状态测试, 配置名合法)
{
    const std::string 配置 = engine::current_config_name();
    EXPECT_TRUE(配置 == "Debug" || 配置 == "Release");
}
