#pragma once
//============================================================================
// 配置编辑器 —— 内部工具（共享声明）
// 由 配置编辑器.cpp 的匿名命名空间工具拆分而来（架构改革 阶段 1）
// 原工具位于匿名命名空间，拆分后改为 engine 命名空间共享函数，行为与拆分前完全一致。
//============================================================================
#include "gui/Config_Editor/配置编辑器.h"

namespace engine
{
    extern bool 本帧有编辑失焦;
    std::string 解析资源路径(const char* 相对路径);
    bool 输入文本(const char* label, std::string& value);
    bool 输入文本提示(const char* label, std::string& value, const char* hint, bool 计入未保存 = true);
    nlohmann::json 生成默认值(const 配置字段定义& f);
}
