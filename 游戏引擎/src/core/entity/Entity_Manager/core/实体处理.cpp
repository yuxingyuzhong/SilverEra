#include "../局部命名空间使用.h"
#include "src/tools/Non_GUI/Logging/日志系统.h"

//引擎命名空间
namespace engine
{
    //实体构建
    vector<uint64_t> Entity_Manager::entity_build(const string& type, const int& counts)
    {
        //获取目标实体类型行为加载路径迭代器
        auto action_it = action_load_path.find(type);
        //若迭代器无效则直接返回
        if (action_it == action_load_path.end())
        {
            Log::warn("Entity_Manager::未定义目标实体类型行为加载路径");
            return {};
        }

        //获取目实体类型属性槽配置脚本加载路径迭代器
        auto prop_it = prop_config_paths.find(type);
        //若迭代器无效则直接返回
        if (prop_it == prop_config_paths.end())
        {
            Log::warn("Entity_Manager::未定义目标实体类型属性槽配置加载路径");
            return {};
        }
        //新实体ID集合记录
        vector<uint64_t> IDs{};
        //构建新实体记录
        IDs = entity_records.build(counts);
        //构造新属性槽记录
        prop_records.build(counts);

        //初始化新实体
        for (const auto& ID : IDs)
        {
            //获取新实体
            auto& new_entity = entity_records.get(ID)->entity;
            //获取新属性槽
            auto& new_prop_slot = prop_records.get(ID)->property_slot;
            //清空属性槽避免数据残留
            new_prop_slot.clear();

            //构造待注入依赖
            auto event_entry = [this](vector<shared_ptr<config_event>> events)->void
                {
                    //直接转发事件至外部
                    event_terminal(events);
                };
            //设置事件发送入口
            new_entity.event_terminal->event_sender_register(event_entry);
            //绑定属性槽
            new_entity.prop_slot_bind(&new_prop_slot);
            //加载决策树
            new_entity.action_load(action_it->second);

            //返回实体ID集合
            return IDs;
        }
    }

    //实体卸载
    void Entity_Manager::entity_unload(vector<uint64_t>& IDs)
    {
        //卸载目标实体记录
        entity_records.unload(IDs);
        //销毁目标实体属性槽
        prop_records.unload(IDs);
    }

    //实体行动 —— 指定实体执行
    void Entity_Manager::entity_act(std::vector<uint64_t>& IDs)
    {
        for (const auto& ID : IDs)
            entity_records.get(ID)->entity.act();
    }

    //实体行动 —— 全量执行
    void Entity_Manager::entity_act(void)
    {
        for (auto& entity_record : entity_records.get())
            entity_record.entity.act();
    }
}
