#pragma once
//============================================================================
// 实体配置模型 —— 内部工具（共享声明）
// 由 实体配置模型.cpp 的匿名命名空间工具拆分而来（架构改革 阶段 2）
// 原工具位于匿名命名空间，拆分后改为 engine 命名空间共享函数，行为与拆分前完全一致。
//============================================================================
#include "src/tools/GUI/Config_Editor/实体配置模型.h"

namespace engine
{
    std::filesystem::path utf8_path(const std::string& s);
    std::string path_utf8(const std::filesystem::path& p);
    bool 原子写入文件(const std::filesystem::path& path, const std::string& content, std::string& error);
}
