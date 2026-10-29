#pragma once
//测试操作界面 —— 常驻 ImGui 界面：选择树 + 运行控制 + 结果表 + 输出镜像 + 轮次历史 + 引擎日志

#include <cstdint>
#include <string>

#include "Application/Test/src/主调/测试选择模型.h"

namespace engine
{
    //测试运行器（前向声明，避免本头引入 gtest）
    class Test_Runner;

    //界面运行状态（由图形操作界面从渲染器帧统计采集，避免本文件依赖渲染层头）
    struct Ui_Runtime_Stats
    {
        //最近一帧主机侧耗时（毫秒）
        double frame_ms = 0.0;
        //最近一帧 GPU 侧耗时（毫秒；不支持时间戳时为 0）
        double gpu_ms = 0.0;
        //已推进帧数
        uint64_t frames = 0;
        //累计绘制项数（含被跳过项）
        uint64_t draw_items = 0;
        //累计实际绘制项数
        uint64_t drawn_items = 0;
        //最近一帧交给录制的绘制项数
        uint64_t last_draw_items = 0;
        //最近一帧被视锥剔除的项数
        uint64_t last_culled_items = 0;
        //最近一帧被合批合并掉的项数
        uint64_t last_merged_items = 0;
        //累计实例数
        uint64_t instances = 0;
        //最近一帧实际执行的后处理级数
        uint32_t post_stages = 0;
        //当前生效的 MSAA 采样数（1 = 关闭）
        uint32_t msaa_samples = 1;
        //最近一次阴影重绘的绘制项数
        uint32_t last_shadow_items = 0;
        //最近一帧是否重绘阴影图（1 / 0）
        uint32_t shadow_last_redrawn = 0;
        //渲染工作线程数（第一百九十八轮；0 = 单线程）
        uint32_t render_threads = 0;
        //累计并行派发次数（第一百九十八轮）
        uint64_t parallel_dispatches = 0;
        //最近一帧并行录制段数（第一百九十八轮；0 = 单线程录制）
        uint32_t parallel_segments = 0;
        //最近一次阴影 pass 的并行段数（第二百一十三轮；0 = 单线程录制）
        uint32_t shadow_parallel_segments = 0;
        //实例剔除并行阈值（第二百二十二轮；默认 256）
        uint32_t cull_parallel_min_instances = 0;
        //设备是否支持 GPU 时间戳
        bool gpu_supported = false;
    };

    //测试操作界面绘制（每帧由图形操作界面的 UI 绘制回调调用）
    //runner：测试运行器（读快照 / 下发运行请求）
    //model：选择模型（套件 / 用例勾选态）
    //exit_requested：界面请求退出（由调用方主循环检查）
    //运行状态：可选的渲染器帧统计快照（缺省不显示性能条）
    //进入展示请求：可选；非空时在运行控制条行末绘制「进入渲染能力展示」按钮并置位
    //抑制退出键：可选；非空且为真时，本帧不把 Esc 当作「退出界面」（用于展示模式返回后的同一按键沿）
    void 测试界面_绘制(Test_Runner& runner, Test_Selection_Model& model, bool& exit_requested,
        const Ui_Runtime_Stats* 运行状态 = nullptr, bool* 进入展示请求 = nullptr,
        bool* 抑制退出键 = nullptr);
}
