#pragma once
//测试运行器 —— 在调用线程（界面主线程）分片执行一轮用例，界面在片间保持可交互
//声明见 测试运行记录.h / gtest输出镜像.h；实现见 测试运行器.cpp

#include <atomic>
#include <cstddef>
#include <mutex>
#include <string>
#include <vector>

#include "Application/Test/src/主调/gtest输出镜像.h"
#include "Application/Test/src/主调/测试运行记录.h"

namespace engine
{
    /*
    测试运行器
    界面模式（ui_install 后）：
    - 由本类把 Gtest_Output_Mirror 接入 gtest 监听器并释放默认打印器（控制台不再重复输出）；
    - 「运行」请求以用例清单入队；由界面主循环每帧调用 poll_on_caller，在**主线程**分片执行；
    - 为什么必须在主线程：渲染用例会创建 SDL 窗口并逐帧抽事件，SDL3 要求事件泵在主线程
      （非主线程会触发 SDL_IsMainThread_REAL() 断言弹窗）。
    - 为什么按用例分片：一整轮 RUN_ALL_TESTS 跑完才回到渲染循环会让窗口长时间不重绘
      （系统判「未响应」）。分片后每帧只跑一个时间片（默认 8 ms，至少一条），
      片间照常渲染，进度条逐帧前进。单条用例自身的耗时无法再切分。
    无头模式：不安装镜像、不轮询，行为与 gtest 原生一致。
    停止语义：跑完当前用例即收尾本轮，不再排队下一轮。
    */
    class Test_Runner
    {
    public:
        //运行状态
        enum class State
        {
            Idle = 0,      //空闲
            Running = 1    //运行中（含排队等待）
        };

        //构造
        Test_Runner(void) = default;
        //析构（摘除监听器）
        ~Test_Runner(void);
        //不可拷贝、不可移动
        Test_Runner(const Test_Runner&) = delete;
        Test_Runner& operator=(const Test_Runner&) = delete;

        //界面模式安装：接入监听器并释放默认打印器（幂等）
        void ui_install(void);
        //请求运行一轮（用例全名清单；空清单被拒绝并返回 false）
        bool request_run(const std::vector<std::string>& cases);
        //请求停止：跑完当前用例后收尾本轮，不再排队
        void request_stop(void);
        //清空全部结果与输出
        void clear_results(void);
        //关闭：请求停止并摘除监听器
        void shutdown(void);
        /*
        主线程轮询：有排队任务时在**调用线程**执行一个时间片并返回
        必须由界面主循环逐帧调用；无排队任务时立即返回（不阻塞）
        */
        void poll_on_caller(void);

        //是否处于运行状态
        bool is_running(void) const;
        //上一轮退出码（0 表示本轮无失败）
        int last_exit_code(void) const;

        //—— 只读快照（供界面线程） ——
        //取当前轮进度（完成数 / 总数）
        void progress_get(int& completed_out, int& total_out) const;
        //取轮次汇总拷贝
        void rounds_snapshot(std::vector<Round_Summary>& rounds_out) const;
        //取用例结果拷贝（限量）
        void cases_snapshot(std::vector<Case_Result>& cases_out, int max_count) const;
        //取输出尾部拷贝（限量）
        void output_tail(std::vector<Output_Line>& lines_out, int max_count) const;

    private:
        //执行一个用例（设置单用例过滤串并跑一次 RUN_ALL_TESTS）
        bool case_execute(const std::string& case_name);

        //输出镜像（界面模式下接入 gtest 监听器）
        Gtest_Output_Mirror mirror{};
        //互斥锁（保护排队清单与本轮游标）
        std::mutex mtx{};
        //待运行的用例清单
        std::vector<std::string> pending_cases{};
        //是否有排队任务
        bool queued = false;
        //本轮是否已开始
        bool round_active = false;
        //本轮是否收到停止请求
        bool stop_requested = false;
        //本轮用例清单
        std::vector<std::string> running_cases{};
        //本轮游标（已完成条数）
        std::size_t cursor = 0;
        //运行状态
        std::atomic<int> state{ 0 };
        //上一轮退出码
        std::atomic<int> exit_code{ 0 };
        //界面模式是否已安装
        bool installed = false;
    };
}
