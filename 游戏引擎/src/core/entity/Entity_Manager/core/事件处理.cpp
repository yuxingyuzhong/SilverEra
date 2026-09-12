#include "../局部命名空间使用.h"
#include "src/tools/Non_GUI/Logging/日志系统.h"

//引擎命名空间
namespace engine
{
    //事件广播
    void Entity_Manager::event_broadcast(shared_ptr<config_event> event)
    {
        //向所有实体发送事件
        for (auto& entity_record : entity_records.get())
            entity_record.entity.event_terminal(event);
    }

    //事件定向发送
    bool Entity_Manager::event_unicast(const std::string& type, const uint64_t& ID,
        std::shared_ptr<config_event> event)
    {
        //获取指定实体
        auto& entity = entity_records.get(ID)->entity;
        //若实体类型与实际类型不匹配
        if (entity.type_get() != type)
            return false;
        //向指定实体发送事件
        entity.event_terminal(event);
        //返回发送成功
        return true;
    }

    //事件处理
    void Entity_Manager::event_process(shared_ptr<config_event> event)
    {
        //若当前为配置事件
        if (event->category == "Config")
        {
            //简化表示路径
            auto& config = event->config;

            //若配置字段检查通过
            if (config_field_parse(config))
            {
                //获取目标实体类型
                string target_type = config["target_type"];
                //注册实体行为加载路径
                action_load_path_register(target_type,config);
                //注册属性槽配置加载路径
                prop_load_path_register(target_type,config);
               
                //缓冲解析结果
                vector<pair<string, string>> buffer = config["needed_events"];
                //转化解析结果
                for (int transform_time = 0; transform_time < buffer.size(); transform_time++)
                {
                    //简化表示路径
                    auto& tag = buffer[transform_time];
                    //构造事件
                    config_event needed_event("Entity_Manager","", tag.first, tag.second, json::object());
                    //若该事件不存在
                    if (!event_map.count(needed_event))
                        event_map.insert(needed_event);
                }

                //更新接入信息
                attach();
            }
        }
        //若当前非配置事件
        else
        {
            //简化表示路径
            auto& tag = event->tag;
            auto& config = event->config;

            //若实体类型字段无效
            if (!Config_Checker::field_check<string>(config, "target_type"))
                return;
            //获取目标实体类型
            string target_type = config["target_type"];

            //若为创建分支事件
            if (tag == "Build")
            {
                //新建实体ID集合
                vector<uint64_t> ID_set{};
                //创建指定数量实体
                ID_set = entity_build(target_type, config.value<int>("counts", 0));

                //若ID集合为空则直接返回
                if (!ID_set.empty())
                {
                    Log::warn("Entity_Manager::未定义实体创建数量\n实体创建事件已驳回");
                    return;
                }
                else
                {
                    //删除"counts"字段
                    config.erase("counts");
                    //增加"ID_set"字段
                    config.emplace("ID_set", ID_set);
                    //向外界发布修饰后事件
                    event_terminal.send(event, acl_key);
                }
            }
            //若为卸载分支事件
            else if (tag == "Unload")
            {
                //若待卸载实体ID集合字段无效
                if (!Config_Checker::field_check<vector<int64_t>>(config, "ID_set"))
                    return;
                //获取待卸载实体ID
                vector<uint64_t> ID_set = config["ID_set"];

                //卸载合法操作实体
                entity_unload(ID_set);
            }
            //若为行动分支事件
            else if (tag == "Act")
            {
                //若待卸载实体ID集合字段无效
                if (!Config_Checker::field_check<vector<int64_t>>(config, "ID_set"))
                    return;
            }
            //若为其他事件
            else
            {
                //若目标实体ID字段无效
                if (!Config_Checker::field_check<string>(config, "target_ID"))
                    return;
                //获取目标实体ID
                uint64_t target_ID = config["target_ID"];

                //定向发送事件
                event_unicast(target_type, target_ID, event);
            }
        }
    }

}
