//测试运行器 —— 实现（声明见 测试运行器.h）
#include "src/主调/测试运行器.h"

#include <gtest/gtest.h>

namespace engine
{
    //析构（停止并等待线程结束）
    Test_Runner::~Test_Runner(void)
    {
        shutdown();
    }

    //界面模式安装：接入监听器并释放默认打印器（幂等）
    void Test_Runner::ui_install(void)
    {
        //仅安装一次
        if (installed)
            return;
        installed = true;
        //接入输出镜像监听器
        ::testing::UnitTest::GetInstance()->listeners().Append(&mirror);
        //释放默认打印器（转移所有权给本进程，需自行释放，避免控制台重复输出）
        ::testing::TestEventListener* default_printer =
            ::testing::UnitTest::GetInstance()->listeners().default_result_printer();
        if (default_printer != nullptr)
            ::testing::UnitTest::GetInstance()->listeners().Release(default_printer);
    }

    //请求运行一轮
    void Test_Runner::request_run(const std::string& filter)
    {
        {
            //登记排队任务（覆盖前一个未执行的请求）
            std::lock_guard<std::mutex> lock(mtx);
            pending_filter = filter;
            queued = true;
        }
        //若当前空闲则启动工作线程
        if (state.load() == static_cast<int>(State::Idle))
        {
            state.store(static_cast<int>(State::Running));
            worker = std::thread([this] { worker_main(); });
        }
    }

    //请求停止
    void Test_Runner::request_stop(void)
    {
        //清空排队：当前轮跑完后工作线程自行退出
        std::lock_guard<std::mutex> lock(mtx);
        queued = false;
        pending_filter.clear();
    }

    //清空全部结果与输出
    void Test_Runner::clear_results(void)
    {
        mirror.results_clear();
    }

    //关闭：停止并等待工作线程结束
    void Test_Runner::shutdown(void)
    {
        //请求停止
        request_stop();
        //等待工作线程结束（若正在跑一轮，将等待其完成）
        if (worker.joinable())
            worker.join();
    }

    //是否处于运行状态
    bool Test_Runner::is_running(void) const
    {
        return state.load() == static_cast<int>(State::Running);
    }

    //上一轮退出码
    int Test_Runner::last_exit_code(void) const
    {
        return exit_code;
    }

    //取当前轮进度
    void Test_Runner::progress_get(int& completed_out, int& total_out) const
    {
        mirror.progress_get(completed_out, total_out);
    }

    //取轮次汇总拷贝
    void Test_Runner::rounds_snapshot(std::vector<Round_Summary>& rounds_out) const
    {
        mirror.rounds_snapshot(rounds_out);
    }

    //取用例结果拷贝
    void Test_Runner::cases_snapshot(std::vector<Case_Result>& cases_out, int max_count) const
    {
        mirror.cases_snapshot(cases_out, max_count);
    }

    //取输出尾部拷贝
    void Test_Runner::output_tail(std::vector<Output_Line>& lines_out, int max_count) const
    {
        mirror.output_tail(lines_out, max_count);
    }

    //工作线程入口
    void Test_Runner::worker_main(void)
    {
        //串行执行排队任务，直到无排队任务或收到停止请求
        for (;;)
        {
            //取排队过滤串
            std::string filter;
            {
                std::lock_guard<std::mutex> lock(mtx);
                if (!queued)
                {
                    //无排队任务：退出线程
                    state.store(static_cast<int>(State::Idle));
                    return;
                }
                filter = pending_filter;
                pending_filter.clear();
                queued = false;
            }

            //设置本轮 gtest 参数（仅工作线程改动这些 flag）
            ::testing::GTEST_FLAG(filter) = filter;
            ::testing::GTEST_FLAG(repeat) = 1;
            ::testing::GTEST_FLAG(break_on_failure) = false;

            //清空本轮用例记录并执行一轮测试
            mirror.round_begin();
            exit_code = RUN_ALL_TESTS();
            //汇总本轮入轮次历史
            mirror.round_end();
        }
    }
}
