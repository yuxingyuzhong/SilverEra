#pragma once
//预编译头
#include "common/前置头文件包含.h"
//获取预定义事件类型
#include "common/types/事件类型.h"
//获取终端接口
#include "终端接口.h"
//获取随机数生成器(用于权限密钥生成)
#include "src/tools/Non_GUI/Random/随机数生成器.h"

//游戏引擎命名空间
namespace engine
{
	//事件终端
	class Event_Terminal
	{
	private:
		//权限密钥
		int64_t acl_key = 0;
		//密钥生成器
		Random_Generator key_generator{};

		//事件集合
		std::vector<std::shared_ptr<config_event>> event_set{};
		//终端接口
		Terminal_Interface terminal_interface;

	public:
		//构造函数
		Event_Terminal() = default;
		//析构函数
		~Event_Terminal() = default;
		//默认移动构造函数
		Event_Terminal(Event_Terminal&&) = default;
		//默认移动复制函数
		Event_Terminal& operator=(Event_Terminal&&) = default;

		//终端接口快捷通道
		Terminal_Interface* operator->(void)
		{
			return &terminal_interface;
		}

		//权限密钥生成
		int64_t acl_key_gen(void);

		//中转站接入
		bool attach(const std::string& module_name,const std::vector<config_event>& needed_events,
			const int64_t& acl_key);
		//目标对象接入检查
		bool check(const std::string& module_name);
		//目标对象呼叫
		bool call(const std::string& module_name);

		//事件构造
		std::shared_ptr<config_event> build();

		//事件发送 —— 单事件重载
		bool send(std::shared_ptr<config_event> event, const int64_t& acl_key);
		//事件发送 —— 多事件重载
		bool send(std::vector<std::shared_ptr<config_event>> events,const int64_t& acl_key);

		//事件接收 —— 单事件重载
		void receive(std::shared_ptr<config_event> event);
		//事件接收 —— 多事件重载
		void receive(std::vector<std::shared_ptr<config_event>> events);

		//事件查阅 —— 全量查阅
		const std::vector<std::shared_ptr<config_event>>& query(const int64_t& acl_key);

		//事件删除 —— 指定事件
		//事件清空
		bool clear(const int64_t& acl_key);

		//括号重载 —— 事件发送
		bool operator()(std::shared_ptr<config_event> event, const int64_t& acl_key)
		{
			return send(event,acl_key);
		}
		bool operator()(std::vector<std::shared_ptr<config_event>> events, const int64_t& acl_key)
		{
			return send(events, acl_key);
		}
		//括号重载 —— 事件接收
		void operator()(std::shared_ptr<config_event> event)
		{
			receive(event);
		}
		void operator()(std::vector<std::shared_ptr<config_event>> events)
		{
			receive(events);
		}
	};
}