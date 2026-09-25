#include "../局部命名空间使用.h"
//获取日志系统
#include "src/tools/Logging/日志系统.h"

namespace engine
{
	//权限密钥生成
	int64_t Event_Terminal::acl_key_gen(void)
	{
		//若当前尚未生成密钥
		if (!acl_key.has_value())
		{
			//无限循环保证密钥成功生成
			for (;;)
			{
				//生成密钥
				acl_key = key_generator();
				//若密钥成功生成则返回
				if (acl_key != 0)
					return acl_key.value();
			}
		}
		//若当前已经生成密钥则返回无效值
		else
			return 0;
	}

	//中转站接入
	bool Event_Terminal::attach(const string& module_name, const vector<event>& needed_events,
		const int64_t& acl_key)
	{
		//若当前尚未生成密钥
		if (!this->acl_key.has_value())
		{
			Log::warn("Event_Terminal::密钥未生成\n功能已锁定");
			return false;
		}
		//若密钥权限未匹配
		if (this->acl_key != acl_key)
			return false;

		//简化表示路径
		auto& Tinterface = terminal_interface;

		//若未注册单事件接收入口
		if(!Tinterface.event_receiver)
		{
			//包装单事件接收入口
			auto it = [this](shared_ptr<event> evt) -> void
				{
					this->receive(evt);
				};
			//接入中转站
			(*Tinterface.attach_handler)(module_name, needed_events, it);
		}
		else
		    //接入中转站
		    (*Tinterface.attach_handler)(module_name, needed_events, *Tinterface.event_receiver);
		//返回接入成功
		return true;
	}

	//中转站交互 —— 单事件重载
	bool Event_Terminal::interact(std::shared_ptr<event> evt, const int64_t& acl_key)
	{
		//若当前尚未生成密钥
		if (!this->acl_key.has_value())
		{
			Log::warn("Event_Terminal::密钥未生成\n功能已锁定");
			return false;
		}

		//若权限密钥匹配
		if (this->acl_key == acl_key)
		{
			//若未注册单事件交互接口则返回
			if (!terminal_interface.interface_check(interface_ID::EVENT_INTERACTOR))
				return false;
			else
			{
				(*terminal_interface.event_interactor)(evt);
				return true;
			}
		}
		else
			return false;
	}

	//中转站交互 —— 多事件重载
	bool Event_Terminal::interact(std::vector<std::shared_ptr<event>> events, const int64_t& acl_key)
	{
		//若当前尚未生成密钥
		if (!this->acl_key.has_value())
		{
			Log::warn("Event_Terminal::密钥未生成\n功能已锁定");
			return false;
		}
		//若权限密钥匹配
		if (this->acl_key == acl_key)
		{
			//若已注册多事件交互接口
			if (terminal_interface.interface_check(interface_ID::EVENTS_INTERACTOR))
			{
				(*terminal_interface.events_interactor)(events);
				return true;
			}
			//若已注册单事件交互接口则返回
			else if (terminal_interface.interface_check(interface_ID::EVENT_INTERACTOR))
			{
				//分多次发送事件
				for (auto& evt : events)
					(*terminal_interface.event_interactor)(evt);
				return true;
			}
			else
				return false;
		}
		else
			return false;
	}

	//事件构造
	shared_ptr<event> Event_Terminal::build(void)
	{
		return shared_ptr<event> (new(nothrow)event());
	}

	//事件构造
	shared_ptr<event> Event_Terminal::build(const string& category, const string& tag)
	{
		return shared_ptr<event>(new(nothrow)event(category,tag));
	}

	//事件构造
	shared_ptr<event> Event_Terminal::build(const string& sender_object, const string& target_object,
		const string& category, const string& tag)
	{
		return shared_ptr<event>(new(nothrow)event(sender_object,target_object,category,tag));
	}

	//事件发送 —— 单事件重载
	bool Event_Terminal::send(shared_ptr<event> evt, const int64_t& acl_key)
	{
		//若当前尚未生成密钥
		if (!this->acl_key.has_value())
		{
			Log::warn("Event_Terminal::密钥未生成\n功能已锁定");
			return false;
		}
		//若权限密钥匹配
		if (this->acl_key == acl_key)
		{
			//若事件发送入口已激活
			if (terminal_interface.event_sender)
			{
				//发送事件
				(*terminal_interface.event_sender)(evt);
				//返回发送成功
				return true;
			}
			else
				return false;
		}
		//若密钥不匹配则发送失败
		else
			return false;
	}

	//事件发送 —— 多事件重载
	bool Event_Terminal::send(vector<shared_ptr<event>> events, const int64_t& acl_key)
	{
		//若当前尚未生成密钥
		if (!this->acl_key.has_value())
		{
			Log::warn("Event_Terminal::密钥未生成\n功能已锁定");
			return false;
		}
		//若权限密钥匹配
		if (this->acl_key == acl_key)
		{
			//若事件发送入口已激活
			if (terminal_interface.events_sender)
			{
				//发送事件
				(*terminal_interface.events_sender)(events);
				//返回发送成功
				return true;
			}
			else
				return false;
		}
		//若密钥不匹配则发送失败
		else
			return false;
	}

	//事件接收 —— 单事件重载
	void Event_Terminal::receive(shared_ptr<event> evt)
	{
		//若事件接收入口已额外注册
		if (terminal_interface.event_receiver)
			(*terminal_interface.event_receiver)(evt);
		//若未额外注册则使用原生通道
		else
			event_set.push_back(evt);
	}

	//事件接收 —— 多事件重载
	void Event_Terminal::receive(vector<shared_ptr<event>> events)
	{
		//若事件接收入口已额外注册
		if (terminal_interface.events_receiver)
			(*terminal_interface.events_receiver)(events);
		//若未额外注册则使用原生通道
		else
			event_set.insert(event_set.end(), events.begin(), events.end());
	}

	//事件查阅
	const vector<shared_ptr<event>>* Event_Terminal::query(const int64_t& acl_key)
	{
		//若当前尚未生成密钥
		if (!this->acl_key.has_value())
		{
			Log::warn("Event_Terminal::密钥未生成\n功能已锁定");
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
			Log::warn("Event_Terminal::密钥未生成\n功能已锁定");
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
