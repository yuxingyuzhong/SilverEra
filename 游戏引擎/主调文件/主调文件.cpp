#include "common/引擎总头文件.h"

int main(void)
{
	// 输出切到 UTF-8（调试输出仍用控制台）
	SetConsoleOutputCP(CP_UTF8);
	// 输入切到 UTF-8
	SetConsoleCP(CP_UTF8);

	//引入全部名称空间
	using namespace engine;

	//事件中转站
	Event_Broker event_broker;
	//配置加载器
	Config_Loader config_loader;
	//属性槽分发器
	Prop_Distributor prop_distributor;
	//实体管理器
	Entity_Manager entity_manager;

	// ———— 事件中转站提供依赖封装 ————

	//接入入口封装
	auto attach_entry = [&event_broker](const std::string& name,
		const std::vector<event>& events,
		std::function<void(std::shared_ptr<event>)> event_entry)
		{
			event_broker.info_register(name, events, event_entry);
		};
	//单事件入口封装
	auto event_entry = [&event_broker](std::shared_ptr<event> evt)
		{
			event_broker.receive(evt);
		};
	//多事件入口封装
	auto event_set_entry = [&event_broker](std::vector<std::shared_ptr<event>> event_set)
		{
		event_broker.receive(event_set);
		};

	// ———— 属性槽分发器初始化 ———— 

	//接入入口注入
	prop_distributor.event_terminal->attach_handler_register(attach_entry);
	//接入事件中转站
	prop_distributor.attach();

	// ———— 实体管理器初始化 ————

	//接入入口注入
	entity_manager.event_terminal->attach_handler_register(attach_entry);
	//单事件入口注入
	entity_manager.event_terminal->event_sender_register(event_entry);
	//多事件入口注入
	entity_manager.event_terminal->event_sender_register(event_set_entry);
	//接入事件中转站
	entity_manager.attach();
	//属性槽绑定入口封装
	auto prop_bind_entry = [&entity_manager](const uint64_t& distribute_key) 
		->Object_Pool<Prop>*
		{
			return entity_manager.prop_slot_get(distribute_key);
		};
	//属性槽分发器绑定属性槽
	prop_distributor.prop_slots_bind(prop_bind_entry);

	// ———— 配置加载器初始化 ————
	
	//单事件入口注入
	config_loader.event_terminal->event_sender_register(event_entry);
	//多事件入口注入
	config_loader.event_terminal->event_sender_register(event_set_entry);
	//加载配置
	config_loader.act();

	for (;;)
	{

	}
}
