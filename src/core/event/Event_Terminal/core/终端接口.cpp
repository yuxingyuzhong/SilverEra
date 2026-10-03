#include "../局部命名空间使用.h"
#include "src/tools/Logging/日志系统运行包.h"

namespace engine
{ 
	//函数包装器内存分配
	template <typename Ret, typename... Args>
	bool Terminal_Interface::memory_malloc(unique_ptr<function<Ret(Args ...)>>& target) const
	{
		//为目标对象分配内存
		target.reset(new(nothrow) function<Ret(Args ...)>);
		//返回内存分配结果
		return static_cast<bool>(target);
	}
	//函数接口注册
	template <typename Ret, typename... Args>
	bool Terminal_Interface::function_register(Interface_ID ID, 
		std::unique_ptr<std::function<Ret(Args ...)>>& target,
		std::function<Ret(Args ...)> function)
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
	Terminal_Interface& Terminal_Interface::attach_handler_register(Attach_Handler callback)
	{
		//注册中转站接入入口
		if (!function_register(Interface_ID::ATTACH_HANDLER, attach_handler, callback))
			logger.warn("Event_Terminal::内存不足\n中转站接口注册失败");
		return *this;
	}
	//中转站交互入口注册 —— 单事件重载
	Terminal_Interface& Terminal_Interface::event_interactor_register(Event_Handler<std::shared_ptr<Event>> callback)
	{
		//注册单事件交互接口
		if(!function_register(Interface_ID::EVENT_INTERACTOR, event_interactor, callback))
			logger.warn("Event_Terminal::内存不足\n单事件中转站交互接口注册失败");
		return *this;
	}
	//中转站交互入口注册 —— 多事件重载
	Terminal_Interface& Terminal_Interface::events_interactor_register
	(Events_Handler<std::vector<std::shared_ptr<Event>>> callback)
	{
		//注册多事件交互接口
		if(!function_register(Interface_ID::EVENTS_INTERACTOR, events_interactor, callback))
			logger.warn("Event_Terminal::内存不足\n多事件中转站交互接口注册失败");
		return *this;
	}

	//事件发送入口注册 —— 单事件重载
	Terminal_Interface& Terminal_Interface::event_sender_register(Event_Handler<void> callback)
	{
		//注册单事件发送入口
		if(!function_register(Interface_ID::EVENT_SENDOR,event_sender, callback))
			logger.warn("Event_Terminal::内存不足\n单事件发送接口注册失败");
		return *this;
	}
	//事件发送入口注册 —— 多事件重载
	Terminal_Interface& Terminal_Interface::event_sender_register(Events_Handler<void> callback)
	{
		//注册多事件发送入口
		if(!function_register(Interface_ID::EVENTS_SENDOR,events_sender, callback))
			logger.warn("Event_Terminal::内存不足\n多事件发送接口注册失败");
		return *this;
	}

	//事件接收入口注册 —— 单事件重载
	Terminal_Interface& Terminal_Interface::event_receiver_register(Event_Handler<void> callback)
	{
		//注册单事件接收入口
		if(!function_register(Interface_ID::EVENT_RECEIVER,event_receiver, callback))
			logger.warn("Event_Terminal::内存不足\n单事件接收接口注册失败");
		return *this;
	}
	//事件接收入口注册 —— 多事件重载
	Terminal_Interface& Terminal_Interface::event_receiver_register(Events_Handler<void> callback)
	{
		//注册多事件接收入口
		if(!function_register(Interface_ID::EVENTS_RECEIVER,events_receiver, callback))
			logger.warn("Event_Terminal::内存不足\n多事件接收接口注册失败");
		return *this;
	}

	//接口注入验证
	bool Terminal_Interface::interface_check(const Interface_ID& ID) const
	{
		//匹配已接入接口ID集合
		for (auto& inerface_ID : map)
		{
			//若目标接口ID已注册
			if (inerface_ID == ID)
				return true;
		}

		return false;
	}

	//已注册接口返回
	vector<Interface_ID> Terminal_Interface::interface_state_get(void) const
	{
		//已注册接口存储缓冲
		vector<Interface_ID> buffer{};
		//匹配已接入接口ID集合
		for (auto& inerface_ID : map)
			buffer.push_back(inerface_ID);		
		//返回注册情况
		return buffer;
	}

}