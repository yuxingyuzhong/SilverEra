#include "../局部命名空间使用.h"

namespace engine
{ 
	//函数包装器内存分配
	template <typename... Args>
	bool Terminal_Interface::memory_malloc(shared_ptr<function<void(Args ...)>>& target)
	{
		//为目标对象分配内存
		target.reset(new(nothrow) function<void(Args ...)>);
		//返回内存分配结果
		return static_cast<bool>(target);
	}
	//函数接口注册
	template <typename... Args>
	bool Terminal_Interface::function_register(interface_ID ID, 
		std::shared_ptr<std::function<void(Args ...)>>& target,
		std::function<void(Args ...)> function)
	{
		//若内存分配成功
		if (memory_malloc(target))
		{
			//注册函数接口
			*(target) = function;
			//记录当前接口已注册
			map.push_back(ID);
			//返回注册成功
			return true;
		}
		//若内存分配失败
		else
			//返回注册失败
			return false;
	}

	//中转站接入入口注册
	bool Terminal_Interface::attach_handler_register(attch_handler callback)
	{
		//注册中转站接入入口
		return function_register(interface_ID::ATTACH_HANDLER,attach_handler, callback);
	}
	//中转站交互入口注册 —— 单事件重载
	bool Terminal_Interface::event_interactor_register(event_handler callback)
	{
		//注册单事件交互接口
		return function_register(interface_ID::EVENT_INTERACTOR, event_interactor, callback);
	}
	//中转站交互入口注册 —— 多事件重载
	bool Terminal_Interface::event_interactor_register(events_handler callback)
	{
		//注册多事件交互接口
		return function_register(interface_ID::EVENTS_INTERACTOR, events_interactor, callback);
	}

	//事件发送入口注册 —— 单事件重载
	bool Terminal_Interface::event_sender_register(event_handler callback)
	{
		//注册单事件发送入口
		return function_register(interface_ID::EVENT_SENDOR,event_sender, callback);
	}
	//事件发送入口注册 —— 多事件重载
	bool Terminal_Interface::event_sender_register(events_handler callback)
	{
		//注册多事件发送入口
		return function_register(interface_ID::EVENTS_SENDOR,events_sender, callback);
	}

	//事件接收入口注册 —— 单事件重载
	bool Terminal_Interface::event_receiver_register(event_handler callback)
	{
		//注册单事件接收入口
		return function_register(interface_ID::EVENT_RECEIVER,event_receiver, callback);
	}
	//事件接收入口注册 —— 多事件重载
	bool Terminal_Interface::event_receiver_register(events_handler callback)
	{
		//注册多事件接收入口
		return function_register(interface_ID::EVENTS_RECEIVER,events_receiver, callback);
	}

	//接口注入验证
	bool Terminal_Interface::interface_check(const interface_ID& ID)
	{
		for (auto& inerface_ID : map)
		{
			//若目标接口ID已注册
			if (inerface_ID == ID)
				return true;
		}

		return false;
	}

}