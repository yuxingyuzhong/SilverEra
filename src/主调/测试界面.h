#pragma once
//测试操作界面 —— 常驻 ImGui 界面：选择树 + 运行控制 + 结果表 + 输出镜像 + 轮次历史 + 引擎日志

#include <string>

#include "Application/Test/src/主调/测试选择模型.h"

namespace engine
{
    //测试运行器（前向声明，避免本头引入 gtest）
    class Test_Runner;

    //测试操作界面绘制（每帧由图形选择窗口的 UI 绘制回调调用）
    //runner：测试运行器（读快照 / 下发运行请求）
    //model：选择模型（套件 / 用例勾选态）
    //exit_requested：界面请求退出（由调用方主循环检查）
    void 测试界面_绘制(Test_Runner& runner, Test_Selection_Model& model, bool& exit_requested);
}
