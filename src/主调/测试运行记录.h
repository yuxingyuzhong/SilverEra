#pragma once
//测试运行记录 —— 数据结构（供界面与监听器共享，线程安全由 测试运行器 保证）

#include <cstdint>
#include <string>
#include <vector>

namespace engine
{
    //用例状态
    enum class Case_Status
    {
        Passed = 0,   //通过
        Failed = 1,   //失败
        Skipped = 2   //跳过
    };

    //单用例运行结果
    struct Case_Result
    {
        //所属套件名
        std::string suite{};
        //用例名
        std::string name{};
        //运行状态
        Case_Status status = Case_Status::Passed;
        //耗时（毫秒）
        int64_t elapsed_ms = 0;
        //失败消息（逐条 TestPartResult 的摘要）
        std::vector<std::string> failures{};
    };

    //单轮测试汇总
    struct Round_Summary
    {
        //轮次号（从 1 开始）
        int round = 0;
        //用例总数
        int total = 0;
        //通过数
        int passed = 0;
        //失败数
        int failed = 0;
        //跳过数
        int skipped = 0;
        //总耗时（毫秒）
        int64_t elapsed_ms = 0;
    };

    //输出行（供界面按级别着色）
    struct Output_Line
    {
        //输出文本
        std::string text{};
        //级别（0 分隔/标题 1 运行 2 通过 3 失败 4 跳过 5 信息）
        int level = 0;
    };
}
