#pragma once
//测试运行器 —— 在调用线程（界面主线程）执行一轮 RUN_ALL_TESTS，界面冻结至本轮结束
//声明见 测试运行记录.h / gtest输出镜像.h；实现见 测试运行器.cpp

#include <atomic>
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
    - 「运行」请求以过滤串入队；由界面主循环每帧调用 poll_on_caller，在**主线程**执行一轮；
    - 为什么必须在主线程：渲染用例会创建 SDL 窗口并逐帧抽事件，SDL3 要求事件泵在主线程
      （非主线程会触发 SDL_IsMainThread_REAL() 断言弹窗）。
    无头模式：不安装镜像、不轮询，行为与 gtest 原生一致。
    停止语义：不再排队下一轮；当前轮跑完即结束（gtest 无中途取消能力）。
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
        //请求运行一轮（过滤串为空表示全跑；运行中再次请求则排队，跑完当前轮后执行）
        void request_run(const std::string& filter);
        //请求停止：当前轮跑完后不再排队下一轮
        void request_stop(void);
        //清空全部结果与输出
        void clear_results(void);
        //关闭：请求停止并摘除监听器
        void shutdown(void);
        /*
        主线程轮询：有排队任务时在**调用线程**执行一轮 RUN_ALL_TESTS 并返回
        必须由界面主循环逐帧调用；无排队任务时立即返回（不阻塞）
        */
        void poll_on_caller(void);

        //是否处于运行状态
        bool is_running(void) const;
        //上一轮退出码（RUN_ALL_TESTS 返回值）
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
        //输出镜像（界面模式下接入 gtest 监听器）
        Gtest_Output_Mirror mirror{};
        //互斥锁（保护排队过滤串与状态）
        std::mutex mtx{};
        //待运行的过滤串
        std::string pending_filter{};
        //是否有排队任务
        bool queued = false;
        //运行状态
        std::atomic<int> state{ 0 };
        //上一轮退出码（工作线程写、界面线程读，故用原子量避免数据竞争）
        std::atomic<int> exit_code{ 0 };
        //界面模式是否已安装
        bool installed = false;
    };
}
