#include "../局部命名空间使用.h"
#include "Engine/EngineCore/src/tools/Logging/日志系统运行包.h"

//引擎命名空间
namespace engine
{
    //构造函数
    Entity_Manager::Entity_Manager()
    {
        //生成权限密钥
        acl_key = event_terminal.acl_key_gen();
    }

    //事件中转站接入
    void Entity_Manager::attach(void)
    {
        //订阅事件集合记录
        vector<Event> needed_events(event_map.begin(), event_map.end());
        //构造事件接收入口
        auto event_receive_entry = [this](shared_ptr<Event> evt)-> void
            {
                this->event_process(evt);
            };
        //注册事件接收入口
        event_terminal->event_receiver_register(event_receive_entry);
        //更新接入信息
        event_terminal.attach("Entity_Manager", needed_events, acl_key);
    }

    //配置事件解析
    bool Entity_Manager::config_field_parse(const json& config) const
    {
        //若实体类型字段无效
        if (!detail::field_check<string>(config, "type"))
            return false;
        //若属性槽配置脚本加载路径无效
        if (!detail::field_check<string>(config, "prop_load_path"))
            return false;
        //若行为脚本加载路径字段无效
        if (!detail::field_check<string>(config, "action_load_path"))
            return false;
        //若订阅事件列表字段无效
        if (!detail::field_check<vector<pair<string, string>>>(config, "needed_events"))
            return false;

        //若所有检查均通过
        return true;
    }

    //决策树加载路径注册
    void Entity_Manager::action_load_path_register(const string& entity_type, const nlohmann::json& config)
    {
        //获取实体行为加载路径
        path decision_load_path = Engine_Env::absolute_path_get(config["decision_load_path"].get<string>());
        //记录实体行为加载路径
        action_load_path[entity_type] = detail::path_to_string(decision_load_path);
    }

    //属性槽配置加载路径注册
    void Entity_Manager::prop_load_path_register(const std::string& entity_type, const nlohmann::json& config)
    {
        //获取属性槽配置加载路径
        path prop_load_path = Engine_Env::absolute_path_get(config["prop_load_path"].get<string>());
        //记录属性槽配置加载路径
        prop_config_paths[entity_type].load_file(detail::path_to_string(prop_load_path));
    }

}
