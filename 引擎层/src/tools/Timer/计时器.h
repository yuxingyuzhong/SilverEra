#pragma once
//预编译头
#include "common/前置头文件包含.h"

namespace engine
{
    //时钟别名
    using Clock = std::chrono::steady_clock;
    //时间戳别名
    using TimePoint = Clock::time_point;
    //时间段别名
    using Duration = Clock::duration;

    //计时器
    class Timer 
    {
    public:
        //构造函数
        Timer(void) = default;
        
        //析构函数
        ~Timer() = default;

        //构建计时任务
        bool task_build(const std::string& task_name);

        //卸载计时任务
        bool task_unload(const std::string& task_name);

        //获取时间间隔
        Duration elapsed(const std::string& task_name,bool restart = false);

        //获取秒级时间精度
        static double units(const Duration& duration);

        //获取毫秒级时间精度
        static double Milli_units(const Duration& duration);

        //获取微秒级时间精度
        static double Micro_units(const Duration& duration);

        //获取纳秒级时间精度
        static long long Nano_units(const Duration& duration);

    private:
        //计时任务起点集合
        std::unordered_map<std::string, TimePoint> task_origin_set;
    };
}