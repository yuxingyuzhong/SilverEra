#pragma once
//gtest 输出镜像 —— 采集每用例结果并按控制台风格格式化输出（供界面展示）
//声明见 测试运行记录.h 与本文件；实现见 gtest输出镜像.cpp

#include <gtest/gtest.h>

#include <atomic>
#include <mutex>

#include "Application/Test/src/主调/测试运行记录.h"

namespace engine
{
    /*
    gtest 输出镜像监听器
    同时承担两件事：
    1) 把 gtest 每用例结果采集进 用例结果 容器（界面只读快照）；
    2) 按 gtest 控制台同风格把事件格式化为 输出行（界面滚动显示）。
    界面模式下由 测试运行器 把它 Append 进监听器列表并 Release 默认打印器；
    无头模式下不安装本类，行为与 gtest 原生一致。
    线程安全：全部采集接口持内部互斥锁，可被测试线程写入、被界面线程读取。
    */
    class Gtest_Output_Mirror final : public ::testing::TestEventListener
    {
    public:
        //构造
        Gtest_Output_Mirror(void) = default;
        //析构（默认）
        ~Gtest_Output_Mirror(void) override = default;

        //开始一轮：清空本轮用例记录与进度（由运行器在每轮 RUN_ALL_TESTS 前调用）
        void round_begin(void);
        //结束一轮：汇总本轮结果入轮次历史（由运行器在 RUN_ALL_TESTS 后调用）
        void round_end(void);
        //清空全部结果与输出（界面「清空结果」按钮）
        void results_clear(void);

        //取当前轮进度（完成数 / 总数）
        void progress_get(int& completed_out, int& total_out) const;
        //取轮次汇总拷贝（界面只读）
        void rounds_snapshot(std::vector<Round_Summary>& rounds_out) const;
        //取用例结果拷贝（界面只读；限量拷贝避免大对象）
        void cases_snapshot(std::vector<Case_Result>& cases_out, int max_count) const;
        //取输出尾部拷贝（界面只读；限量拷贝避免大对象）
        void output_tail(std::vector<Output_Line>& lines_out, int max_count) const;

        //—— TestEventListener 覆写 ——
        void OnTestProgramStart(const ::testing::UnitTest& unit_test) override;
        void OnTestIterationStart(const ::testing::UnitTest& unit_test, int iteration) override;
        void OnEnvironmentsSetUpStart(const ::testing::UnitTest& unit_test) override;
        void OnEnvironmentsSetUpEnd(const ::testing::UnitTest& unit_test) override;
        void OnTestSuiteStart(const ::testing::TestSuite& test_suite) override;
        void OnTestStart(const ::testing::TestInfo& test_info) override;
        void OnTestPartResult(const ::testing::TestPartResult& test_part_result) override;
        void OnTestEnd(const ::testing::TestInfo& test_info) override;
        void OnTestSuiteEnd(const ::testing::TestSuite& test_suite) override;
        void OnEnvironmentsTearDownStart(const ::testing::UnitTest& unit_test) override;
        void OnEnvironmentsTearDownEnd(const ::testing::UnitTest& unit_test) override;
        void OnTestIterationEnd(const ::testing::UnitTest& unit_test, int iteration) override;
        void OnTestProgramEnd(const ::testing::UnitTest& unit_test) override;

    private:
        //追加一条输出行（须已持锁）
        void line_append(const std::string& text, int level);
        //当前用例完整名称（套件.用例）
        std::string case_name_get(const ::testing::TestInfo& test_info) const;

        //本轮用例结果
        std::vector<Case_Result> cases{};
        //轮次历史
        std::vector<Round_Summary> rounds{};
        //输出缓冲（环形语义：超出上限由输出裁剪处理）
        std::vector<Output_Line> output{};
        //互斥锁（保护上述容器；读接口为 const 方法，故声明 mutable）
        mutable std::mutex mtx{};
        //本轮已完成用例数
        std::atomic<int> completed_cases{0};
        //本轮用例总数
        std::atomic<int> total_cases{0};
        //当前正在采集的用例记录
        Case_Result current_case{};
        //是否正处在一个用例内
        bool in_case = false;
        //输出行上限（超出丢弃最旧）
        static constexpr int output_cap = 2000;
        //用例结果上限（超出丢弃最旧；界面表格通常只看当前轮）
        static constexpr int cases_cap = 10000;
    };
}
