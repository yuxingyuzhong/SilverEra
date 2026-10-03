#pragma once
//预编译头
#include "common/前置头文件包含.h"
//获取流节点链表树
#include "../Stream_Tree/流节点树.h"
//获取标准库 format 特化集（错误码 / 错误条件 / 文件系统路径）
#include "src/tools/Detail/标准库format特化.h"

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
        //输出流绑定（路径节点绑定文件输出流，目录节点绑定后其后代共享该流）
        bool stream_bind(const std::string& path, const std::string& file_name)
        {
            //转发给流节点链表树
            return stream_tree.stream_bind(path, file_name);
        }
        //输出流绑定（路径节点绑定外部输出流）
        bool stream_bind(const std::string& path, const Stream& stream)
        {
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
        //日志行输出（合成「类型前缀 + 正文 + 换行」的消息行后按调用处路径路由）
        void output(const char* file_name, const char* type, const std::string& text)
        {
            //构造消息行（以 '\n' 结尾，不做强制刷新）
            const std::string msg = std::format("[{}]{}\n", type, text);
            //交由流节点链表树按调用处路径路由输出
            stream_tree.stream_output(file_name, msg);
        }

        //流节点链表树（has-a：日志系统持有分流与存储设施）
        Stream_Tree stream_tree{};
    };
}