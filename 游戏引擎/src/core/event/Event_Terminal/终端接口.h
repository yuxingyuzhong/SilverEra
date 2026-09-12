#pragma once
//预编译头
#include "common/前置头文件包含.h"
//获取预定义事件类型
#include "common/types/事件类型.h"
//获取随机数生成器(用于权限密钥生成)
#include "src/tools/Non_GUI/Random/随机数生成器.h"

namespace engine
{
	//终端接口列表
	enum class interface_ID
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
		using needed_events = const std::vector<config_event>&;
		//事件入口类型别名 —— 单事件重载
		using event_handler = std::function<void(std::shared_ptr<config_event> evt)>;
		//事件入口类型别名 —— 多事件重载
		using events_handler = std::function<void(std::vector<std::shared_ptr<config_event>>)>;
		//接口入口类型别名
		using attch_handler = std::function<void(const std::string& name,needed_events events,
			event_handler receiver)>;

		//接口注册表
		std::vector<interface_ID> map{};

		//中转站接入入口
		std::shared_ptr<attch_handler> attach_handler;
		//中转站交互入口 —— 单事件重载
		std::shared_ptr<event_handler> event_interactor;
		//中转站交互入口 —— 多事件重载
		std::shared_ptr<events_handler> events_interactor;

		//事件发送入口 —— 单事件重载
		std::shared_ptr<event_handler> event_sender;
		//事件发送入口 —— 多事件重载
		std::shared_ptr<events_handler> events_sender;

		//事件接收入口 —— 单事件重载
		std::shared_ptr<event_handler> event_receiver;
		//事件接收入口 —— 多事件重载
		std::shared_ptr<events_handler> events_receiver;

		//函数包装器内存分配
		template <typename... Args>
		bool memory_malloc(std::shared_ptr<std::function<void(Args ...)>>& target);

		//函数接口注册
		template <typename... Args>
		bool function_register(interface_ID ID,std::shared_ptr<std::function<void(Args ...)>>& target,
			std::function<void(Args ...)> function);
	public:
		//事件终端友元
		friend class Event_Terminal;

		//中转站接入入口注册
		bool attach_handler_register(attch_handler callback);
		//中转站交互入口注册 —— 单事件重载
		bool event_interactor_register(event_handler callback);
		//中转站交互入口注册 —— 多事件重载
		bool event_interactor_register(events_handler callback);

		//事件发送入口注册 —— 单事件重载
		bool event_sender_register(event_handler callback);
		//事件发送入口注册 —— 多事件重载
		bool event_sender_register(events_handler callback);

		//事件接收入口注册 —— 单事件重载
		bool event_receiver_register(event_handler callback);
		//事件接收入口注册 —— 多事件重载
		bool event_receiver_register(events_handler callback);

		//接口注入验证
		bool interface_check(const interface_ID& ID);
	};
}