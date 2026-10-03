#pragma once
//预编译头
#include "Engine/EngineCore/common/前置头文件包含.h"
//获取路径字符串转换工具
#include "路径字符串转换.h"

/*
标准库 format 特化集
    标准库 std::format 未为下列类型提供格式化器，此处补齐，使日志系统与业务代码可直接
    以 "{}" 输出这些类型。

    依赖约束（重要）
        本头文件会被 Logging/Log/日志系统.h 包含，因此**只允许依赖预编译头与同目录的
        纯工具头**（路径字符串转换.h 同样只依赖预编译头）。一旦引入引擎环境.h 等上层头，
        就会构成「日志系统.h → 本文件 → 引擎环境.h → 日志系统运行包.h → 日志系统.h」
        的头文件循环，编译必然失败。

    已补充
        std::error_code        错误码（值 ＋ 消息）
        std::error_condition   错误条件（值 ＋ 消息）
        std::filesystem::path  文件路径（按 UTF-8 文本输出，中文字节不丢失）

    未补充（技术原因）
        std::vector<T> / std::optional<T> 等需要「偏特化 std::formatter」，标准不允许，
        故不予添加；项目自定义类型的特化应放在各自类型所在的头文件中（如
        Config_Content 的特化在 Config_Loader/配置加载器.h）。
*/
namespace std
{
    //错误码格式化特化
    template<>
    struct formatter<std::error_code>
    {
        //忽略格式说明符，不做特殊解析
        constexpr auto parse(format_parse_context& ctx)
        {
            return ctx.begin();
        }
        //输出「[值] 消息」
        auto format(const std::error_code& error_info, format_context& ctx) const
        {
            return std::format_to(ctx.out(), "[{}] {}", error_info.value(), error_info.message());
        }
    };

    //错误条件格式化特化
    template<>
    struct formatter<std::error_condition>
    {
        //忽略格式说明符，不做特殊解析
        constexpr auto parse(format_parse_context& ctx)
        {
            return ctx.begin();
        }
        //输出「[值] 消息」
        auto format(const std::error_condition& condition, format_context& ctx) const
        {
            return std::format_to(ctx.out(), "[{}] {}", condition.value(), condition.message());
        }
    };

    //文件系统路径格式化特化（经路径字符串转换按 UTF-8 文本输出）
    template<>
    struct formatter<std::filesystem::path>
    {
        //忽略格式说明符，不做特殊解析
        constexpr auto parse(format_parse_context& ctx)
        {
            return ctx.begin();
        }
        //输出 UTF-8 文本形式的路径
        auto format(const std::filesystem::path& path, format_context& ctx) const
        {
            //转为 UTF-8 文本后写出
            const std::string path_text = engine::detail::path_to_string(path);
            return std::format_to(ctx.out(), "{}", path_text);
        }
    };
}