#pragma once
//预编译头
#include "common/前置头文件包含.h"
//获取预定义事件类型
#include "../Event/事件.h"
//获取随机数生成器(用于权限密钥生成)
#include "src/tools/Random_Generator/随机数生成器.h"

namespace engine
{
	//终端接口列表
	enum class Interface_ID
	{
		NONE,
		ATTACH_HANDLER,
		EVENT_INTERACTOR,
		EVENTS_INTERACTOR,
		EVENT_SENDOR,
		EVENTS_SENDOR,
		EVENT_RECEIVER,
		EVENTS_RECEIVER
	};

	//事件终端前置声明
	class Event_Terminal;

	//终端接口
	class Terminal_Interface
	{
	private:
		//订阅事件类型别名
		using Needed_Events = const std::vector<Event>&;
		//事件入口类型别名 —— 单事件重载
		using Event_Handler = std::function<void(std::shared_ptr<Event> evt)>;
		//事件入口类型别名 —— 多事件重载
		using Events_Handler = std::function<void(std::vector<std::shared_ptr<Event>>)>;
		//接口入口类型别名
		using Attach_Handler = std::function<void(const std::string& name,Needed_Events events,
			Event_Handler receiver)>;

		//接口注册表
		std::vector<Interface_ID> map{};

		//中转站接入入口
		std::unique_ptr<Attach_Handler> attach_handler;
		//中转站交互入口 —— 单事件重载
		std::unique_ptr<Event_Handler> event_interactor;
		//中转站交互入口 —— 单事件重载
		std::unique_ptr<Events_Handler> events_interactor;

		//事件发送入口 —— 单事件重载
		std::unique_ptr<Event_Handler> event_sender;
		//事件发送入口 —— 多事件重载
		std::unique_ptr<Events_Handler> events_sender;

		//事件接收入口 —— 单事件重载
		std::unique_ptr<Event_Handler> event_receiver;
		//事件接收入口 —— 多事件重载
		std::unique_ptr<Events_Handler> events_receiver;

		//函数包装器内存分配
		template <typename... Args>
		bool memory_malloc(std::unique_ptr<std::function<void(Args ...)>>& target) const;

		//函数接口注册
		template <typename... Args>
		bool function_register(Interface_ID ID,std::unique_ptr<std::function<void(Args ...)>>& target,
			std::function<void(Args ...)> function);
	public:
		//事件终端友元
		friend class Event_Terminal;

		//中转站接入入口注册
		Terminal_Interface& attach_handler_register(Attach_Handler callback);
		//中转站交互入口注册
		Terminal_Interface& event_interactor_register(Event_Handler callback);
		//中转站交互入口注册
		Terminal_Interface& events_interactor_register(Events_Handler callback);

		//事件发送入口注册 —— 单事件重载
		Terminal_Interface& event_sender_register(Event_Handler callback);
		//事件发送入口注册 —— 多事件重载
		Terminal_Interface& event_sender_register(Events_Handler callback);

		//事件接收入口注册 —— 单事件重载
		Terminal_Interface& event_receiver_register(Event_Handler callback);
		//事件接收入口注册 —— 多事件重载
		Terminal_Interface& event_receiver_register(Events_Handler callback);

		//接口注入验证
		bool interface_check(const Interface_ID& ID) const;
		//已注册接口返回
		std::vector<Interface_ID> interface_state_get(void) const;
	};
}