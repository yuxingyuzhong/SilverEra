//测试运行器 —— 实现（声明见 测试运行器.h）
#include "src/主调/测试运行器.h"

#include <gtest/gtest.h>

#include <chrono>
#include <set>

namespace engine
{
    //文件内部工具
    namespace
    {
        //单个时间片的执行预算（毫秒）：预算内尽量多跑，跑满即返回渲染一帧
        const int 时间片预算毫秒 = 8;
    }

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
        //释放默认打印器：Release 把所有权交给调用方，须自行 delete，否则泄漏
        //（释放后控制台不再重复打印，输出只进界面镜像）
        ::testing::TestEventListener* default_printer =
            ::testing::UnitTest::GetInstance()->listeners().default_result_printer();
        if (default_printer != nullptr)
        {
            ::testing::UnitTest::GetInstance()->listeners().Release(default_printer);
            delete default_printer;
        }
    }

    //请求运行一轮（用例全名清单）
    bool Test_Runner::request_run(const std::vector<std::string>& cases)
    {
        //空清单视为无效请求：不启动，避免「什么都不选却全跑」
        if (cases.empty())
            return false;

        //登记排队任务（覆盖前一个未执行的请求）
        std::lock_guard<std::mutex> lock(mtx);
        pending_cases = cases;
        queued = true;
        state.store(static_cast<int>(State::Running));
        return true;
    }

    //请求停止（跑完当前用例后收尾本轮）
    void Test_Runner::request_stop(void)
    {
        //清空排队；本轮游标检查到停止标记后收尾
        std::lock_guard<std::mutex> lock(mtx);
        queued = false;
        pending_cases.clear();
        stop_requested = true;
    }

    //清空全部结果与输出
    void Test_Runner::clear_results(void)
    {
        mirror.results_clear();
    }

    //关闭：请求停止并摘除监听器
    void Test_Runner::shutdown(void)
    {
        //请求停止（清空排队）
        request_stop();
        //从 gtest 监听器列表摘除镜像：Release 只摘除、不析构（镜像为本类成员）
        //不摘除会使监听器列表在本对象销毁后仍持有悬垂指针
        if (installed)
        {
            ::testing::UnitTest::GetInstance()->listeners().Release(&mirror);
            installed = false;
        }
    }

    //是否处于运行状态
    bool Test_Runner::is_running(void) const
    {
        return state.load() == static_cast<int>(State::Running);
    }

    //上一轮退出码
    int Test_Runner::last_exit_code(void) const
    {
        return exit_code.load();
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

    //执行一个用例：过滤串设为该用例全名，只跑一次迭代
    bool Test_Runner::case_execute(const std::string& case_name)
    {
        //设置单用例过滤串（主线程改动 gtest 标志）
        ::testing::GTEST_FLAG(filter) = case_name;
        ::testing::GTEST_FLAG(repeat) = 1;
        ::testing::GTEST_FLAG(break_on_failure) = false;

        //记录执行前的完成数，用于识别「过滤串未命中任何用例」
        int 完成前 = 0;
        int 总数前 = 0;
        mirror.progress_get(完成前, 总数前);

        //执行该用例
        const int 结果 = RUN_ALL_TESTS();

        //过滤串未命中时给出提示，避免静默少跑
        int 完成后 = 0;
        int 总数后 = 0;
        mirror.progress_get(完成后, 总数后);
        if (完成后 == 完成前)
            mirror.note_append("[  WARN  ] 未匹配到用例：" + case_name, 4);

        return 结果 == 0;
    }

    //主线程轮询：有排队任务时执行一个时间片
    void Test_Runner::poll_on_caller(void)
    {
        //—— 环节一：本轮尚未开始时取走排队清单 ——
        if (!round_active)
        {
            std::vector<std::string> cases;
            {
                std::lock_guard<std::mutex> lock(mtx);
                if (!queued)
                {
                    state.store(static_cast<int>(State::Idle));
                    return;
                }
                cases.swap(pending_cases);
                queued = false;
                stop_requested = false;
            }

            //空清单不会入队（request_run 已拒绝），此处兜底
            if (cases.empty())
            {
                state.store(static_cast<int>(State::Idle));
                return;
            }

            //登记本轮清单并清空本轮记录
            {
                std::lock_guard<std::mutex> lock(mtx);
                running_cases.swap(cases);
                cursor = 0;
                round_active = true;
            }

            //统计本轮涉及套件数（按 "套件.用例" 的前缀去重）
            std::set<std::string> 套件集合;
            for (const std::string& 用例全名 : running_cases)
            {
                const std::size_t 点位置 = 用例全名.find('.');
                //无点号时按整体作为一个套件名
                套件集合.insert((点位置 == std::string::npos) ? 用例全名
                    : 用例全名.substr(0, 点位置));
            }

            //重置本轮退出码并打印轮首横幅
            exit_code.store(0);
            mirror.round_begin(static_cast<int>(running_cases.size()),
                static_cast<int>(套件集合.size()));
        }

        //—— 环节二：按时间预算执行若干条用例 ——
        const auto 起点 = std::chrono::steady_clock::now();
        while (true)
        {
            //取当前待执行用例（顺带检查停止标记）
            std::string 用例全名;
            bool 已结束 = false;
            {
                std::lock_guard<std::mutex> lock(mtx);
                已结束 = stop_requested || cursor >= running_cases.size();
                if (!已结束)
                    用例全名 = running_cases[cursor];
            }
            //本轮执行完毕或收到停止请求
            if (已结束)
                break;

            //执行当前用例
            const bool 通过 = case_execute(用例全名);
            //任一条失败即把本轮标记为失败
            if (!通过)
                exit_code.store(1);

            //推进游标
            {
                std::lock_guard<std::mutex> lock(mtx);
                ++cursor;
            }

            //超出时间片预算则让出一帧
            const auto 用时 = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - 起点).count();
            if (用时 >= 时间片预算毫秒)
                break;
        }

        //—— 环节三：判断本轮是否收尾 ——
        bool 收尾 = false;
        {
            std::lock_guard<std::mutex> lock(mtx);
            收尾 = stop_requested || cursor >= running_cases.size();
        }
        if (!收尾)
            return;

        //写入轮次历史并复位
        mirror.round_end();
        {
            std::lock_guard<std::mutex> lock(mtx);
            running_cases.clear();
            cursor = 0;
            round_active = false;
            stop_requested = false;
        }
        state.store(static_cast<int>(State::Idle));
    }
}
