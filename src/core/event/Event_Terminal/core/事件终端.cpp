#include "../局部命名空间使用.h"
//获取日志系统
#include "Engine/EngineCore/src/tools/Logging/日志系统运行包.h"

namespace engine
{
	//权限密钥生成
	optional<int64_t> Event_Terminal::key(void)
	{
		//若当前尚未生成密钥
		if (!acl_key.has_value())
			//生成密钥并返回
			return acl_key = key_generator();
		//若当前已经生成密钥则返回无效值
		else
			return nullopt;
	}

	//事件构造
	shared_ptr<Event> Event_Terminal::build(void) const
	{
		return shared_ptr<Event>(new(nothrow)Event());
	}

	//事件构造
	shared_ptr<Event> Event_Terminal::build(const string& category, const string& tag) const
	{
		return shared_ptr<Event>(new(nothrow)Event(category, tag));
	}

	//事件构造
	shared_ptr<Event> Event_Terminal::build(const string& sender, const string& receiver,
		const string& category, const string& tag) const
	{
		return shared_ptr<Event>(new(nothrow)Event(sender, receiver, category, tag));
	}

	//中转站接入
	bool Event_Terminal::attach(const string& subscriber, const vector<Event>& needed_events,
		const int64_t& acl_key)
	{
		//若当前尚未生成密钥
		if (!this->acl_key.has_value())
		{
			logger.warn("Event_Terminal::密钥未生成\n功能已锁定");
			return false;
		}
		//若密钥权限未匹配
		if (this->acl_key != acl_key)
			return false;

		//简化表示路径
		auto& Tinterface = terminal_port;

		//若未注册单事件接收入口
		if(!Tinterface.check(Port_ID::Evt_Receiver))
		{
			//包装单事件接收入口
			auto it = [this](shared_ptr<Event> evt) -> void
				{
					this->receive(evt);
				};
			//接入中转站
			(*Tinterface.broker_attacher)(subscriber, needed_events, it);
		}
		else
		    //接入中转站
		    (*Tinterface.broker_attacher)(subscriber, needed_events, *Tinterface.evt_receiver);
		//返回接入成功
		return true;
	}

	//中转站呼叫 —— 单事件重载
	bool Event_Terminal::interact(std::shared_ptr<Event> evt,std::shared_ptr<Event>& receiver,
		const int64_t& acl_key)
	{
		//若当前尚未生成密钥
		if (!this->acl_key.has_value())
		{
			logger.warn("Event_Terminal::密钥未生成\n功能已锁定");
			return false;
		}

		//若权限密钥匹配
		if (this->acl_key == acl_key)
		{
			//若注册单事件中转站呼叫入口
			if (terminal_port.check(Port_ID::Evt_Broker_Caller))
			{
				receiver = (*terminal_port.evt_broker_caller)(evt);
				return true;
			}
			//若注册多事件中转站呼叫入口
			else if (terminal_port.check(Port_ID::Evts_Broker_Caller))
			{
				//降级使用多事件呼叫接口
				receiver = (*terminal_port.evts_broker_caller)({ evt }).front();
				//日志警告
				logger.warn("Event_Terminal::未注册单事件呼叫入口");
				logger.warn("Event_Terminal::现临时使用多事件呼叫入口接管逻辑");
				return true;
			}
			//若无可用接口则报错
			else
			{
				logger.error("Event_Terminal::未注册有效入口");
				return false;
			}
		}
		//若密钥不匹配则报错
		else
		{
			logger.error("Event_Terminal::密钥不匹配");
			return false;
		}
	}

	//中转站呼叫 —— 多事件重载
	bool Event_Terminal::interact(std::vector<std::shared_ptr<Event>> events,
		std::vector<std::shared_ptr<Event>> receiver, const int64_t& acl_key)
	{
		//若当前尚未生成密钥
		if (!this->acl_key.has_value())
		{
			logger.warn("Event_Terminal::密钥未生成\n功能已锁定");
			return false;
		}

		//若权限密钥匹配
		if (this->acl_key == acl_key)
		{
			//若已注册多事件呼叫接口
			if (terminal_port.check(Port_ID::Evts_Broker_Caller))
			{
				receiver = (*terminal_port.evts_broker_caller)(events);
				return true;
			}
			//若已注册单事件呼叫接口则返回
			else if (terminal_port.check(Port_ID::Evt_Broker_Caller))
			{
				//分多次发送事件
				for (auto& evt : events)
					receiver.push_back((*terminal_port.evt_broker_caller)(evt));
				//日志警告
				logger.warn("Event_Terminal::未注册多事件呼叫入口");
				logger.warn("Event_Terminal::现临时使用单事件呼叫入口接管逻辑");
				return true;
			}
			//若无可用接口则报错
			else
			{
				logger.error("Event_Terminal::未注册有效入口");
				return false;
			}
		}
		//若密钥不匹配则报错
		else
		{
			logger.error("Event_Terminal::密钥不匹配");
			return false;
		}
	}

	//事件发送 —— 单事件重载
	bool Event_Terminal::send(shared_ptr<Event> evt, const int64_t& acl_key) const
	{
		//若当前尚未生成密钥
		if (!this->acl_key.has_value())
		{
			logger.warn("Event_Terminal::密钥未生成\n功能已锁定");
			return false;
		}
		//若权限密钥匹配
		if (this->acl_key == acl_key)
		{
			//若注册单事件发送入口
			if (terminal_port.check(Port_ID::Evt_Sender))
			{
				//发送事件
				(*terminal_port.evt_sender)(evt);
				//返回发送成功
				return true;
			}
			//若注册多事件发送入口
			else if (terminal_port.check(Port_ID::Evts_Sender))
			{
				//降级使用多事件发送接口
				(*terminal_port.evts_sender)({ evt });
				//日志警告
				logger.warn("Event_Terminal::未注册单事件发送入口");
				logger.warn("Event_Terminal::现临时使用多事件发送入口接管逻辑");
				return true;
			}
			//若无可用接口则报错
			else
			{
				logger.error("Event_Terminal::未注册有效入口");
				return false;
			}
		}
		//若密钥不匹配则报错
		else
		{
			logger.error("Event_Terminal::密钥不匹配");
			return false;
		}
	}

	//事件发送 —— 多事件重载
	bool Event_Terminal::send(vector<shared_ptr<Event>> events, const int64_t& acl_key) const
	{
		//若当前尚未生成密钥
		if (!this->acl_key.has_value())
		{
			logger.warn("Event_Terminal::密钥未生成\n功能已锁定");
			return false;
		}
		//若权限密钥匹配
		if (this->acl_key == acl_key)
		{
			//若注册多事件发送入口
			if (terminal_port.check(Port_ID::Evts_Sender))
			{
				//发送事件
				(*terminal_port.evts_sender)(events);
				//返回发送成功
				return true;
			}
			//若注册单事件发送入口
			else if (terminal_port.check(Port_ID::Evt_Sender))
			{
				//分多次发送事件
				for (auto& evt : events)
					(*terminal_port.evt_sender)(evt);
				//日志警告
				logger.warn("Event_Terminal::未注册多事件发送入口");
				logger.warn("Event_Terminal::现临时使用单事件发送入口接管逻辑");
				return true;
			}
			//若无可用接口则报错
			else
			{
				logger.error("Event_Terminal::未注册有效入口");
				return false;
			}
		}
		//若密钥不匹配则报错
		else
		{
			logger.error("Event_Terminal::密钥不匹配");
			return false;
		}
	}

	//事件接收 —— 单事件重载
	void Event_Terminal::receive(shared_ptr<Event> evt)
	{
		//若注册单事件接收入口
		if (terminal_port.check(Port_ID::Evt_Receiver))
			(*terminal_port.evt_receiver)(evt);
		//若注册多事件接收入口
		else if (terminal_port.check(Port_ID::Evts_Receiver))
		{
			//降级使用多事件接收接口
			(*terminal_port.evts_receiver)({ evt });
			//日志警告
			logger.warn("Event_Terminal::未注册单事件接收入口");
			logger.warn("Event_Terminal::现临时使用多事件接收入口接管逻辑");
		}
		//若未额外注册则使用原生通道
		else
			event_set.push_back(evt);
	}

	//事件接收 —— 多事件重载
	void Event_Terminal::receive(vector<shared_ptr<Event>> events)
	{
		//若注册多事件接收入口
		if (terminal_port.check(Port_ID::Evts_Receiver))
			(*terminal_port.evts_receiver)(events);
		//若注册单事件接收入口
		else if (terminal_port.check(Port_ID::Evt_Receiver))
		{
			//分多次接收事件
			for (auto& evt : events)
				(*terminal_port.evt_receiver)(evt);
			//日志警告
			logger.warn("Event_Terminal::未注册多事件接收入口");
			logger.warn("Event_Terminal::现临时使用单事件接收入口接管逻辑");
		}
		//若未额外注册则使用原生通道
		else
			event_set.insert(event_set.end(), events.begin(), events.end());
	}

	//事件查阅
	const vector<shared_ptr<Event>>* Event_Terminal::query(const int64_t& acl_key) const
	{
		//若当前尚未生成密钥
		if (!this->acl_key.has_value())
		{
			logger.warn("Event_Terminal::密钥未生成\n功能已锁定");
			return nullptr;
		}
		//若密钥匹配则发送事件集合
		if (this->acl_key == acl_key)
			return &event_set;
		else
			return nullptr;
	}

	//事件清空
	bool Event_Terminal::clear(const int64_t& acl_key)
	{
		//若当前尚未生成密钥
		if (!this->acl_key.has_value())
		{
			logger.warn("Event_Terminal::密钥未生成\n功能已锁定");
			return false;
		}
		//若密钥匹配则清空所有事件
		if (acl_key == this->acl_key)
		{
			event_set.clear();
			//返回清空成功
			return true;
		}
		//返回无权清空
		else
			return false;
	}

}
