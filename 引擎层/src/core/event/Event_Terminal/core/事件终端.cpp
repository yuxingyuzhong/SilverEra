#include "../局部命名空间使用.h"

namespace engine
{
	//权限密钥生成
	int64_t Event_Terminal::acl_key_gen(void)
	{
		//若当前尚未生成密钥
		if (acl_key == 0)
		{
			//无限循环保证密钥成功生成
			for (;;)
			{
				//生成密钥
				acl_key = key_generator();
				//若密钥成功生成则返回
				if (acl_key != 0)
					return acl_key;
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
		//若密钥权限未匹配
		if (this->acl_key != acl_key)
			return false;

		//获取当前单事件接收入口
		auto event_receiver = terminal_interface.event_receiver;
		//若未注册单事件接收入口
		if(!event_receiver)
		{
			//包装单事件接收入口
			*event_receiver = [this](shared_ptr<event> evt) -> void
				{
					this->receive(evt);
				};
		}
		
		//接入中转站
		(*terminal_interface.attach_handler)(module_name, needed_events, *event_receiver);
		return true;
	}

	//中转站查询
	bool Event_Terminal::check(const std::string& module_name)
	{
		return true;
	}

	//事件发送 —— 单事件重载
	bool Event_Terminal::send(std::shared_ptr<event> evt, const int64_t& acl_key)
	{
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
	void Event_Terminal::receive(std::vector<std::shared_ptr<event>> events)
	{
		//若事件接收入口已额外注册
		if (terminal_interface.events_receiver)
			(*terminal_interface.events_receiver)(events);
		//若未额外注册则使用原生通道
		else
			event_set.insert(event_set.end(), events.begin(), events.end());
	}

	//事件查阅
	const vector<shared_ptr<event>>& Event_Terminal::query(const int64_t& acl_key)
	{
		//若密钥匹配则发送事件集合
		if (this->acl_key == acl_key)
			return event_set;
		else
			return {};
	}

	//事件清空
	bool Event_Terminal::clear(const int64_t& acl_key)
	{
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
