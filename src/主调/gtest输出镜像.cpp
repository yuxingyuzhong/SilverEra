//gtest 输出镜像 —— 实现（声明见 gtest输出镜像.h）
#include "src/主调/gtest输出镜像.h"

#include <chrono>

//引擎命名空间
namespace engine
{
    //开始一轮：清空本轮用例记录与进度
    void Gtest_Output_Mirror::round_begin(void)
    {
        //清空本轮用例与进度（轮次历史与输出缓冲保留，供界面多轮展示）
        std::lock_guard<std::mutex> lock(mtx);
        cases.clear();
        completed_cases.store(0);
        total_cases.store(0);
        current_case = Case_Result{};
        in_case = false;
    }

    //结束一轮：汇总本轮结果入轮次历史
    void Gtest_Output_Mirror::round_end(void)
    {
        //汇总本轮用例计数
        std::lock_guard<std::mutex> lock(mtx);
        Round_Summary summary{};
        summary.round = static_cast<int>(rounds.size()) + 1;
        for (const Case_Result& item : cases)
        {
            ++summary.total;
            //累计本轮总耗时（各用例耗时之和）
            summary.elapsed_ms += item.elapsed_ms;
            if (item.status == Case_Status::Passed)
                ++summary.passed;
            else if (item.status == Case_Status::Failed)
                ++summary.failed;
            else
                ++summary.skipped;
        }
        rounds.push_back(summary);
    }

    //清空全部结果与输出
    void Gtest_Output_Mirror::results_clear(void)
    {
        std::lock_guard<std::mutex> lock(mtx);
        cases.clear();
        rounds.clear();
        output.clear();
        completed_cases.store(0);
        total_cases.store(0);
    }

    //取当前轮进度
    void Gtest_Output_Mirror::progress_get(int& completed_out, int& total_out) const
    {
        completed_out = completed_cases.load();
        total_out = total_cases.load();
    }

    //取轮次汇总拷贝
    void Gtest_Output_Mirror::rounds_snapshot(std::vector<Round_Summary>& rounds_out) const
    {
        std::lock_guard<std::mutex> lock(mtx);
        rounds_out = rounds;
    }

    //取用例结果拷贝（限量拷贝，避免大对象）
    void Gtest_Output_Mirror::cases_snapshot(std::vector<Case_Result>& cases_out, int max_count) const
    {
        std::lock_guard<std::mutex> lock(mtx);
        cases_out.clear();
        //从末尾向前取最多 max_count 条（当前轮新结果在尾部）
        const int begin_index = static_cast<int>(cases.size()) > max_count
            ? static_cast<int>(cases.size()) - max_count : 0;
        for (int index = begin_index; index < static_cast<int>(cases.size()); ++index)
            cases_out.push_back(cases[static_cast<std::size_t>(index)]);
    }

    //取输出尾部拷贝
    void Gtest_Output_Mirror::output_tail(std::vector<Output_Line>& lines_out, int max_count) const
    {
        std::lock_guard<std::mutex> lock(mtx);
        lines_out.clear();
        const int begin_index = static_cast<int>(output.size()) > max_count
            ? static_cast<int>(output.size()) - max_count : 0;
        for (int index = begin_index; index < static_cast<int>(output.size()); ++index)
            lines_out.push_back(output[static_cast<std::size_t>(index)]);
    }

    //—— TestEventListener 覆写 ——

    void Gtest_Output_Mirror::OnTestProgramStart(const ::testing::UnitTest& unit_test)
    {
        (void)unit_test;
    }

    void Gtest_Output_Mirror::OnTestIterationStart(const ::testing::UnitTest& unit_test, int iteration)
    {
        //记录本轮总用例数
        total_cases.store(static_cast<int>(unit_test.total_test_count()));
        //镜像控制台风格的开场行
        std::lock_guard<std::mutex> lock(mtx);
        line_append("[==========] Running " + std::to_string(unit_test.total_test_count())
            + " tests from " + std::to_string(unit_test.total_test_suite_count()) + " test suites.", 0);
    }

    void Gtest_Output_Mirror::OnEnvironmentsSetUpStart(const ::testing::UnitTest& unit_test)
    {
        (void)unit_test;
    }

    void Gtest_Output_Mirror::OnEnvironmentsSetUpEnd(const ::testing::UnitTest& unit_test)
    {
        (void)unit_test;
    }

    void Gtest_Output_Mirror::OnTestSuiteStart(const ::testing::TestSuite& test_suite)
    {
        //镜像控制台风格的套件开场行
        std::lock_guard<std::mutex> lock(mtx);
        line_append("[----------] " + std::string(test_suite.name()), 0);
    }

    void Gtest_Output_Mirror::OnTestStart(const ::testing::TestInfo& test_info)
    {
        //开启新用例记录
        std::lock_guard<std::mutex> lock(mtx);
        current_case = Case_Result{};
        current_case.suite = test_info.test_suite_name();
        current_case.name = test_info.name();
        in_case = true;
        //镜像控制台风格的用例开场行
        line_append("[ RUN      ] " + case_name_get(test_info), 1);
    }

    void Gtest_Output_Mirror::OnTestPartResult(const ::testing::TestPartResult& test_part_result)
    {
        //仅镜像失败的断言行（通过项不输出，与默认打印器一致）
        if (!test_part_result.failed())
            return;
        std::lock_guard<std::mutex> lock(mtx);
        line_append(std::string(test_part_result.file_name()) + ":" + std::to_string(test_part_result.line_number())
            + ": Failure\n" + test_part_result.message(), 3);
    }

    void Gtest_Output_Mirror::OnTestEnd(const ::testing::TestInfo& test_info)
    {
        //收尾当前用例：判定状态、记录耗时、收集失败消息
        std::lock_guard<std::mutex> lock(mtx);
        const ::testing::TestResult* result = test_info.result();
        current_case.elapsed_ms = result != nullptr ? result->elapsed_time() : 0;
        //状态判定顺序：先跳过再失败（失败含致命/非致命）
        if (result != nullptr)
        {
            if (result->Skipped())
                current_case.status = Case_Status::Skipped;
            else if (result->Failed())
                current_case.status = Case_Status::Failed;
            else
                current_case.status = Case_Status::Passed;
            //收集失败消息（含多行细节）
            if (result->Failed())
            {
                for (int index = 0; index < result->total_part_count(); ++index)
                {
                    const ::testing::TestPartResult& part = result->GetTestPartResult(index);
                    if (part.failed())
                        current_case.failures.push_back(std::string(part.file_name()) + ":" + std::to_string(part.line_number())
                            + ": " + part.summary());
                }
            }
        }
        //写入本轮用例记录
        cases.push_back(current_case);
        //超出用例结果上限时丢弃最旧（与输出缓冲同样的环形语义）
        while (static_cast<int>(cases.size()) > cases_cap)
            cases.erase(cases.begin());
        ++completed_cases;
        in_case = false;
        //镜像控制台风格的用例收尾行
        const std::string case_label = case_name_get(test_info);
        const std::string elapsed_text = " (" + std::to_string(current_case.elapsed_ms) + " ms)";
        if (current_case.status == Case_Status::Passed)
            line_append("[       OK ] " + case_label + elapsed_text, 2);
        else if (current_case.status == Case_Status::Skipped)
            line_append("[  SKIPPED ] " + case_label + elapsed_text, 4);
        else
            line_append("[  FAILED  ] " + case_label + elapsed_text, 3);
    }

    void Gtest_Output_Mirror::OnTestSuiteEnd(const ::testing::TestSuite& test_suite)
    {
        //镜像控制台风格的套件收尾行
        std::lock_guard<std::mutex> lock(mtx);
        line_append("[----------] " + std::string(test_suite.name()), 0);
    }

    void Gtest_Output_Mirror::OnEnvironmentsTearDownStart(const ::testing::UnitTest& unit_test)
    {
        (void)unit_test;
    }

    void Gtest_Output_Mirror::OnEnvironmentsTearDownEnd(const ::testing::UnitTest& unit_test)
    {
        (void)unit_test;
    }

    void Gtest_Output_Mirror::OnTestIterationEnd(const ::testing::UnitTest& unit_test, int iteration)
    {
        (void)iteration;
        //镜像控制台风格的轮末统计行（通过数由总数扣失败与跳过推算）
        std::lock_guard<std::mutex> lock(mtx);
        const int failed_count = static_cast<int>(unit_test.failed_test_count());
        const int skipped_count = static_cast<int>(unit_test.skipped_test_count());
        const int passed_count = static_cast<int>(unit_test.total_test_count())
            - failed_count - skipped_count;
        line_append("[==========] " + std::to_string(unit_test.total_test_count())
            + " tests from " + std::to_string(unit_test.total_test_suite_count())
            + " test suites ran.", 0);
        line_append("[  PASSED  ] " + std::to_string(passed_count) + " tests.", 2);
        if (failed_count > 0)
            line_append("[  FAILED  ] " + std::to_string(failed_count) + " tests.", 3);
    }

    void Gtest_Output_Mirror::OnTestProgramEnd(const ::testing::UnitTest& unit_test)
    {
        (void)unit_test;
    }

    //—— 内部工具 ——

    //追加一条输出行（须已持锁）
    void Gtest_Output_Mirror::line_append(const std::string& text, int level)
    {
        output.push_back(Output_Line{ text, level });
        //超出上限丢弃最旧（环形语义）
        while (static_cast<int>(output.size()) > output_cap)
            output.erase(output.begin());
    }

    //当前用例完整名称（套件.用例）
    std::string Gtest_Output_Mirror::case_name_get(const ::testing::TestInfo& test_info) const
    {
        return std::string(test_info.test_suite_name()) + "." + test_info.name();
    }
}
