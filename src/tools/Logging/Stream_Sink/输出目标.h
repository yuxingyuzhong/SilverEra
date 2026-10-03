#pragma once
//预编译头
#include "Engine/EngineCore/common/前置头文件包含.h"

namespace engine
{
    //日志输出流别名（对外绑定接口沿用共享输出流形态）
    using Log_Stream = std::shared_ptr<std::ostream>;

    //线程安全输出目标（内部互斥 + 内部缓冲：单次写入原子完成，避免多线程交错；达阈值才落盘，减少系统调用）
    class Stream_Sink
    {
    public:
        //以输出流、落盘阈值与「落盘时是否刷新底层流」构造
        Stream_Sink(const Log_Stream& owner, size_t flush_size, bool flush_target)
            : owner(owner), flush_size(flush_size), flush_target(flush_target){}
        //禁用拷贝
        Stream_Sink(const Stream_Sink&) = delete;
        //禁用赋值
        Stream_Sink& operator=(const Stream_Sink&) = delete;
        //析构时落盘残余缓冲
        ~Stream_Sink()
        {
            //落盘残余缓冲
            flush();
        }
        //写出文本（锁内整体入缓冲，缓冲达阈值即落盘）
        void write(const std::string& text)
        {
            //加锁保护缓冲与目标流
            std::lock_guard<std::mutex> lock(sink_mtx);
            //追加待写内容
            buffer += text;
            //缓冲达到阈值才落盘
            if (buffer.size() >= flush_size)
                flush_locked();
        }
        //把缓冲内容落盘（线程安全）
        void flush()
        {
            //加锁保护缓冲与目标流
            std::lock_guard<std::mutex> lock(sink_mtx);
            //落盘
            flush_locked();
        }
    private:
        //缓冲落盘（须已持锁）
        void flush_locked()
        {
            //无待写内容或目标流不可用则直接返回
            if (buffer.empty() || !owner)
                return;
            //整段写入目标流
            *owner << buffer;
            //按需刷新底层流（文件流需刷新后外部读取方可即时看到）
            if (flush_target)
                owner->flush();
            //清空缓冲
            buffer.clear();
        }

        //目标输出流（共享所有权，保证其生命周期不短于本对象）
        Log_Stream owner{};
        //落盘阈值（达此字节数即落盘；为 0 表示每次写出即落盘）
        size_t flush_size = 0;
        //落盘时是否刷新底层流
        bool flush_target = false;
        //内部缓冲
        std::string buffer{};
        //内部互斥（保护内部缓冲与目标流）
        std::mutex sink_mtx{};
    };
}