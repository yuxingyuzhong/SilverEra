#pragma once
//预编译头
#include "Engine/EngineCore/common/前置头文件包含.h"
//获取流节点链表树
#include "../Stream_Tree/流节点树.h"
//获取日志规则（根目录作用域与输出许可）
#include "../Rule/日志规则.h"
//获取标准库 format 特化集（错误码 / 错误条件 / 文件系统路径）
#include "Engine/EngineCore/src/tools/Detail/标准库format特化.h"

namespace engine
{
    //日志系统（门面：日志方法与流设置接口；流的分流与存储交由流节点链表树）
    class Log
    {
    public:
        //输出流别名（对外绑定接口沿用共享输出流形态）
        using Stream = Log_Stream;
        //日志格式串（携带调用处信息，并承担占位符与实参的编译期校验）
        template<typename... Args>
        class Format
        {
        public:
            //从字面量构造（默认参数在调用点求值，从而自动捕获调用处文件路径）
            template<typename Text>
            consteval Format(const Text& text, std::source_location place = std::source_location::current())
                : pattern(text), site(place){}

            //格式串本体（编译期校验占位符与实参匹配）
            std::format_string<Args...> pattern;
            //调用处信息
            std::source_location site;
        };
    public:
        //信息输出
        template<typename... Args>
        void info(Format<std::type_identity_t<Args>...> fmt, Args&&... args)
        {
            //格式化正文并合成消息行交由链表树路由输出（方法不感知任何流设置）
            output(fmt.site.file_name(), "INFO", std::format(fmt.pattern, std::forward<Args>(args)...));
        }
        //警告输出
        template<typename... Args>
        void warn(Format<std::type_identity_t<Args>...> fmt, Args&&... args)
        {
            //格式化正文并合成消息行交由链表树路由输出
            output(fmt.site.file_name(), "WARN", std::format(fmt.pattern, std::forward<Args>(args)...));
        }
        //错误输出
        template<typename... Args>
        void error(Format<std::type_identity_t<Args>...> fmt, Args&&... args)
        {
            //格式化正文并合成消息行交由链表树路由输出
            output(fmt.site.file_name(), "ERROR", std::format(fmt.pattern, std::forward<Args>(args)...));
        }
        //调试输出
        template<typename... Args>
        void debug(Format<std::type_identity_t<Args>...> fmt, Args&&... args)
        {
            //格式化正文并合成消息行交由链表树路由输出
            output(fmt.site.file_name(), "DEBUG", std::format(fmt.pattern, std::forward<Args>(args)...));
        }
        //日志根目录设置（调用处路径作用域；空串表示整个项目；未设置则禁止绑定输出流）
        bool root_set(const std::string& dir)
        {
            //登记根目录键
            rules.root_set(dir);
            return true;
        }
        //日志根目录清除（回到未设置状态，此后禁止绑定输出流）
        void root_clear(void)
        {
            //清除根目录键
            rules.root_clear();
        }
        //日志根目录查询（未设置返回空串）
        std::string root_get(void) const
        {
            //读取根目录键
            return rules.root_get();
        }
        //文件输出许可设置（file → 是否允许输出）
        bool file_rule_set(const std::string& file, bool allowed)
        {
            //登记文件许可
            rules.file_allowed_set(file, allowed);
            return true;
        }
        //目录输出许可设置（dir → 是否允许输出；作用于其后代调用处）
        bool dir_rule_set(const std::string& dir, bool allowed)
        {
            //登记目录许可
            rules.dir_allowed_set(dir, allowed);
            return true;
        }
        //级别输出许可设置（level → 是否允许输出）
        bool level_rule_set(const std::string& level, bool allowed)
        {
            //登记级别许可
            rules.level_allowed_set(level, allowed);
            return true;
        }
        //输出许可表清空（保留根目录设置）
        void rule_clear(void)
        {
            //清空三张许可表
            rules.clear();
        }
        //输出流绑定（路径节点绑定文件输出流，目录节点绑定后其后代共享该流）
        bool stream_bind(const std::string& path, const std::string& file_name)
        {
            //根目录未设置则拒绝绑定并走后备通道
            if (!bind_gate())
                return false;
            //转发给流节点链表树
            return stream_tree.stream_bind(path, file_name);
        }
        //输出流绑定（路径节点绑定外部输出流）
        bool stream_bind(const std::string& path, const Stream& stream)
        {
            //根目录未设置则拒绝绑定并走后备通道
            if (!bind_gate())
                return false;
            //转发给流节点链表树
            return stream_tree.stream_bind(path, stream);
        }
        //输出流清空（清除路径节点自有输出流，使该节点及其后代回落共享父节点输出流）
        bool stream_clear(const std::string& path)
        {
            //转发给流节点链表树
            return stream_tree.stream_clear(path);
        }
        //输出流落盘（把整棵树内所有输出目标的缓冲内容写出并刷新）
        void stream_flush()
        {
            //转发给流节点链表树
            return stream_tree.stream_flush();
        }
    private:
        //绑定门禁（根目录未设置则拒绝绑定：有其他可用接口则切换并告警，无则报错）
        bool bind_gate(void)
        {
            //根目录已设置则放行
            if (rules.root_is_set())
                return true;
            //存在其他可用输出接口（控制台兜底）则切换并打印警告日志
            if (stream_tree.console_available())
                warn("日志系统::日志根目录未设置，输出流绑定已拒绝，日志回落控制台输出");
            //无其他可用输出接口则打印错误日志
            else
                error("日志系统::日志根目录未设置且无其他可用输出接口，输出流绑定失败");
            return false;
        }
        //日志行输出（合成「类型前缀 + 正文 + 换行」的消息行后按调用处路径路由）
        void output(const char* file_name, const char* type, const std::string& text)
        {
            //构造消息行（以 '\n' 结尾，不做强制刷新）
            const std::string msg = std::format("[{}]{}\n", type, text);
            //交由流节点链表树按调用处路径路由输出（内含越界截断与选择性输出判定）
            stream_tree.stream_output(file_name, type, msg, rules);
        }

        //日志规则（has-a：日志根目录作用域与三张输出许可表）
        Log_Rules rules{};
        //流节点链表树（has-a：日志系统持有分流与存储设施）
        Stream_Tree stream_tree{};
    };
}