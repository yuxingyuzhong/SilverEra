#include "src/core/event/Event_Terminal/局部命名空间使用.h"
//获取日志系统
#include "src/tools/Logging/日志系统运行包.h"

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
	bool Event_Terminal::attach(const std::string& subscriber, const int64_t& acl_key)
	{
		//若当前尚未生成密钥
		if (!this->acl_key.has_value())
		{
			logger.warn("Event_Terminal::密钥未生成\n功能已锁定");
			return false;
		}
		//若密钥权限未匹配
		if (this->acl_key != acl_key)
		{
			logger.warn("Event_Terminal::密钥匹配失败");
			return false;
		}

		//简化表示路径
		auto& Tinterface = terminal_port;
		//若未注册中转站接入端口
		if (!Tinterface.check(Port_ID::Broker_Attacher))
		{
			logger.error("Event_Terminal::事件中转站接入端口未绑定");
			logger.error("Event_Terminal::接入失败");
			return false;
		}

		//若未注册单事件接收端口
		if(!Tinterface.check(Port_ID::Evt_Receiver))
		{
			//包装单事件接收端口
			auto it = [this](shared_ptr<Event> evt) -> void
				{
					this->receive(evt);
				};
			//接入中转站
			(*Tinterface.broker_attacher)(subscriber, this->acl_key.value(), it);
		}
		else
		    //接入中转站
		    (*Tinterface.broker_attacher)(subscriber, this->acl_key.value(), *Tinterface.evt_receiver);
		//返回接入成功
		return true;
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
			//若事件为空
			if (!evt)
			{
				logger.warn("Event_Terminal::事件为空\n已取消该事件发送");
				return false;
			}

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
