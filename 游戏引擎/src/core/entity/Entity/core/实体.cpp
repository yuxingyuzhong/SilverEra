#include "../局部命名空间使用.h"
#include "src/tools/Non_GUI/Logging/日志系统.h"

//引擎命名空间
namespace engine
{
	//构造函数
	Entity::Entity(void)
	{
		//获取权限密钥
		this->acl_key = event_terminal.acl_key_gen();
	}

	//构造函数
	Entity::Entity(const int64_t& ID) : Entity()
	{
		//设置实体ID编号
		this->ID = ID;
	}

	//构造函数
	Entity::Entity(const int64_t& ID, const std::string& load_path):Entity(ID)
	{
		//加载决策树
		action_load(load_path);
	}

	//析构函数
	Entity::~Entity()
	{

	}

	//ID绑定
	void Entity::ID_bind(const uint64_t& ID)
	{
		this->ID = ID;
	}

	//ID信息获取
	uint64_t Entity::ID_get(void)
	{
		return ID;
	}

	//标签信息获取
	std::string Entity::type_get(void)
	{
		return type;
	}

	//属性槽绑定
	void Entity::prop_slot_bind(unordered_map<string, double>* ptr)
	{
		//绑定属性槽
		property_slot = ptr;
	}

	//行为加载
	void Entity::action_load(const std::string& load_path)
	{
		//为决策树打开所有标准库
		action.open_libraries();
		//注册行为决策脚本
		action.load_file(load_path);

		// ———— 成员变量注册环节 ————

		//注册通用属性槽
		action.set("pros", sol::as_table(property_slot));

		//注册事件基类信息
		register_event(action);
		//注册配置事件信息
		register_config_event(action);
		//注册事件集合引用
		action.set("event_set", ref(event_terminal.event_get(acl_key)));

		//注册事件发送函数
		action.set_function("event_send", [this](shared_ptr<config_event> event)->void
			{
				this->event_terminal.event_send({ event }, acl_key);
			});
	}

	//行为决策
	void Entity::act(void)
	{
		//运行决策树
		action["decision"]();
		//清空事件集合
		event_terminal.clear(acl_key);
	}

}
