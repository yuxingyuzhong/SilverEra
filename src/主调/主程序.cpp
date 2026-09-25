//测试层主调 —— main() 入口
//============================================================================
// 位置：白银纪元/测试层/src/主调/主程序.cpp
// ---------------------------------------------------------------------------
// 分流顺序：
//   ① 未显式指定 --selector 且带 --gtest_* 参数 → 透传给 googletest（自动化 / ctest 用）
//   ② --selector=off             → 不开窗，直接全跑
//   ③ --selector=console         → 控制台编号菜单
//   ④ 默认（或 --selector=window）→ 打开 ImGui 选择窗口；窗口创建失败退控制台菜单
// 选择完成后销毁窗口与 ImGui 上下文、恢复控制台输出，再执行测试。
//============================================================================
#include <gtest/gtest.h>

#include "src/主调/测试选择模型.h"
#include "src/主调/图形选择窗口.h"
#include "src/主调/控制台选择菜单.h"

#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include <windows.h>

//文件内部工具
namespace
{
    //自定义开关前缀：选择器模式
    const char* 选择器前缀 = "--selector=";
}

//主函数
int main(int argc, char** argv)
{
    //控制台切换 UTF-8，保证中文用例名可读
    ::SetConsoleOutputCP(CP_UTF8);
    ::SetConsoleCP(CP_UTF8);

    //—— 第一步：摘出自定义开关，其余参数原样留给 googletest ——
    std::string 选择模式 = "window";
    //是否显式指定了选择器（显式指定时优先于「带 gtest 参数即透传」的自动化约定）
    bool 显式选择器 = false;
    std::vector<std::string> 保留参数文本;
    保留参数文本.push_back((argv[0] != nullptr) ? argv[0] : "EngineTests");
    bool 命令行带过滤 = false;

    for (int i = 1; i < argc; ++i)
    {
        const std::string 参数 = (argv[i] != nullptr) ? argv[i] : "";

        //摘出自定义开关
        if (参数.rfind(选择器前缀, 0) == 0)
        {
            选择模式 = 参数.substr(std::strlen(选择器前缀));
            显式选择器 = true;
            continue;
        }

        //记录是否出现 gtest 参数（出现即视为自动化调用，直接透传）
        if (参数.rfind("--gtest_", 0) == 0)
            命令行带过滤 = true;

        保留参数文本.push_back(参数);
    }

    //重建 argv（googletest 需要 char**）
    std::vector<char*> 参数指针;
    for (std::string& 文本 : 保留参数文本)
        参数指针.push_back(文本.data());
    参数指针.push_back(nullptr);
    int 参数个数 = static_cast<int>(保留参数文本.size());

    //—— 第二步：交给 googletest 解析（必须在取反射前完成） ——
    ::testing::InitGoogleTest(&参数个数, 参数指针.data());

    //—— 第三步：透传 / 关闭分支：不开窗直接全跑 ——
    //未显式指定选择器时，出现 gtest 参数即视为自动化调用，原样透传
    if ((命令行带过滤 && !显式选择器) || 选择模式 == "off")
        return RUN_ALL_TESTS();

    //—— 第四步：建立选择模型，按模式取得过滤串 ——
    engine::测试选择模型 模型;
    模型.建立套件树();

    std::string 过滤串;
    if (选择模式 == "console")
    {
        //控制台菜单
        engine::控制台选择菜单_运行(模型, 过滤串);
    }
    else
    {
        //默认走图形窗口；创建失败（无图形环境）退控制台菜单
        if (!engine::图形选择窗口_运行(模型, 过滤串))
        {
            std::cout << "[测试选择] 图形窗口不可用，改用控制台菜单。" << std::endl;
            engine::控制台选择菜单_运行(模型, 过滤串);
        }
    }

    //—— 第五步：恢复控制台输出并执行测试 ——
    std::cout << "[测试选择] 过滤串：" << 过滤串 << std::endl;
    std::cout << "[测试选择] 可用 --gtest_filter=" << 过滤串 << " 复跑同一批用例。" << std::endl;
    ::testing::GTEST_FLAG(filter) = 过滤串;
    return RUN_ALL_TESTS();
}
