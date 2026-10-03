#pragma once
//预编译头
#include "common/前置头文件包含.h"
//获取预定义事件类型
#include "../Event/事件.h"
//获取终端接口
#include "终端接口.h"
//获取随机数生成器(用于权限密钥生成)
#include "src/tools/Random_Generator/随机数生成器.h"

//游戏引擎命名空间
namespace engine
{
	//事件终端
	class Event_Terminal
	{
	private:
		//权限密钥
		std::optional<int64_t> acl_key;
		//密钥生成器
		Random_Generator<int64_t> key_generator{};

		//事件集合
		std::vector<std::shared_ptr<Event>> event_set{};
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
		bool attach(const std::string& module_name,const std::vector<Event>& needed_events,
			const int64_t& acl_key);
		//中转站交互 —— 单事件重载
		bool interact(std::shared_ptr<Event> evt, std::shared_ptr<Event>& receiver, const int64_t& acl_key);
		//中转站交互 —— 多事件重载
		bool interact(std::vector<std::shared_ptr<Event>> events, std::vector<std::shared_ptr<Event>> receiver
			,const int64_t& acl_key);

		//事件构造
		std::shared_ptr<Event> build(void) const;
		//事件构造
		std::shared_ptr<Event> build(const std::string& category, const std::string& tag) const;
		//事件构造
		std::shared_ptr<Event> build(const std::string& sender_object, const std::string& target_object,
			const std::string& category, const std::string& tag) const;

		//事件发送 —— 单事件重载
		bool send(std::shared_ptr<Event> evt, const int64_t& acl_key) const;
		//事件发送 —— 多事件重载
		bool send(std::vector<std::shared_ptr<Event>> events,const int64_t& acl_key) const;
		//事件接收 —— 单事件重载
		void receive(std::shared_ptr<Event> evt);
		//事件接收 —— 多事件重载
		void receive(std::vector<std::shared_ptr<Event>> events);
		//事件查阅
		const std::vector<std::shared_ptr<Event>>* query(const int64_t& acl_key) const;
		//事件清空
		bool clear(const int64_t& acl_key);

	};
}