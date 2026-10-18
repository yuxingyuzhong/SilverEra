#pragma once
//预编译头
#include "Engine/EngineCore/common/前置头文件包含.h"
//获取预定义事件类型
#include "../Event/事件.h"
//获取随机数生成器(用于权限密钥生成)
#include "Engine/EngineCore/src/tools/Random_Generator/随机数生成器.h"
//获取日志系统
#include "Engine/EngineCore/src/tools/Logging/日志系统运行包.h"

namespace engine
{
	//终端端口列表
	enum class Port_ID
	{
		None,
		Broker_Attacher,
		Evt_Sender,
		Evts_Sender,
		Evt_Receiver,
		Evts_Receiver
	};

	//事件终端前置声明
	class Event_Terminal;

	//终端接口
	class Terminal_Port
	{
	private:
		//事件入口类型别名 —— 单事件重载
		using Event_Handler = std::function<void(std::shared_ptr<Event>)>;
		//事件入口类型别名 —— 多事件重载
		using Events_Handler = std::function<void(std::vector<std::shared_ptr<Event>>)>;
		//接口入口类型别名
		using Broker_Attacher = std::function<void(const std::string& name,const uint64_t& acl_key,
			Event_Handler receiver)>;

		//接口注册表
		std::vector<Port_ID> binding_table{};

		//中转站接入入口
		std::unique_ptr<Broker_Attacher> broker_attacher;

		//事件发送入口 —— 单事件重载
		std::unique_ptr<Event_Handler> evt_sender;
		//事件发送入口 —— 多事件重载
		std::unique_ptr<Events_Handler> evts_sender;

		//事件接收入口 —— 单事件重载
		std::unique_ptr<Event_Handler> evt_receiver;
		//事件接收入口 —— 多事件重载
		std::unique_ptr<Events_Handler> evts_receiver;

		//函数包装器内存分配
		template <typename Ret,typename... Args>
		bool memory_malloc(std::unique_ptr<std::function<Ret(Args ...)>>& target) const
        {
            //为目标对象分配内存
            target.reset(new(std::nothrow) std::function<Ret(Args ...)>);
            //返回内存分配结果
            return static_cast<bool>(target);
        }

		//函数接口注册
		template <typename Ret,typename... Args>
		bool function_register(Port_ID ID,std::unique_ptr<std::function<Ret(Args ...)>>& target,
			std::function<Ret(Args ...)> function)
        {
            //若内存分配成功
            if (memory_malloc(target))
            {
                //注册函数接口
                *(target) = function;
                //记录当前端口已绑定
                binding_table.push_back(ID);
                //返回注册成功
                return true;
            }
            //若内存分配失败
            else
                //返回注册失败
                return false;
        }
	public:
		//事件终端友元
		friend class Event_Terminal;

        //终端端口绑定
        template <typename T>
        Terminal_Port& bind(const Port_ID& ID, T callback)
        {
            //若回调可绑定中转站接入端口
            if constexpr (std::is_invocable_v<T, const std::string&, const uint64_t&, Event_Handler>)
            {
                //若回调返回类型为void
                if constexpr (std::is_same_v<std::invoke_result_t<T, const std::string&, const uint64_t&,
                    Event_Handler>, void>)
                {
                    //若接口ID不匹配
                    if (ID != Port_ID::Broker_Attacher)
                    {
                        //记录ID匹配失败日志
                        logger.error("Event_Terminal::ID匹配失败\n已驳回中转站接入端口绑定请求");
                        return *this;
                    }
                    //绑定中转站接入端口
                    function_register(Port_ID::Broker_Attacher, broker_attacher, Broker_Attacher(callback));
                }
            }
            //若回调可绑定单事件重载端口
            else if constexpr (std::is_invocable_v<T, std::shared_ptr<Event>>)
            {
                //若接口ID为单事件发送端口
                if (ID == Port_ID::Evt_Sender)
                    //绑定单事件发送端口
                    function_register(Port_ID::Evt_Sender, evt_sender, Event_Handler(callback));
                //若接口ID为单事件接收端口
                else if (ID == Port_ID::Evt_Receiver)
                    //绑定单事件接收端口
                    function_register(Port_ID::Evt_Receiver, evt_receiver, Event_Handler(callback));
                //若接口ID不属于单事件发送/接收端口
                else
                    //记录ID匹配失败日志
                    logger.error("Event_Terminal::ID匹配失败\n已驳回单事件发送/接收端口绑定请求");
            }
            //若回调可绑定多事件重载端口
            else if constexpr (std::is_invocable_v<T, std::vector<std::shared_ptr<Event>>>)
            {
               //若接口ID为多事件发送端口
                if (ID == Port_ID::Evts_Sender)
                    //绑定多事件发送端口
                    function_register(Port_ID::Evts_Sender, evts_sender, Events_Handler(callback));
                //若接口ID为多事件接收端口
                else if (ID == Port_ID::Evts_Receiver)
                    //绑定多事件接收端口
                    function_register(Port_ID::Evts_Receiver, evts_receiver, Events_Handler(callback));
                //若接口ID不属于多事件发送/接收端口
                else
                    //记录ID匹配失败日志
                    logger.error("Event_Terminal::ID匹配失败\n已驳回多事件发送/接收端口绑定请求");
                
            }
            //若传入回调类型不受支持
            else
                //编译期断言拦截不支持的callback类型
                static_assert(!sizeof(T), "Terminal_Port::bind: 不支持的callback类型");

            return *this;
        }
        //接口注入验证
		bool check(const Port_ID& ID) const
        {
            //匹配已接入接口ID集合
            for (auto& inerface_ID : binding_table)
            {
                //若目标接口ID已注册
                if (inerface_ID == ID)
                    return true;
            }

            return false;
        }
		//已注册接口返回
		std::vector<Port_ID> state(void) const
        {
            //已注册接口存储缓冲
            std::vector<Port_ID> buffer{};
            //匹配已接入接口ID集合
            for (auto& inerface_ID : binding_table)
                buffer.push_back(inerface_ID);
            //返回注册情况
            return buffer;
        }
	};
}