#include "../局部命名空间使用.h"
#include "src/tools/Logging/日志系统.h"

//引擎命名空间
namespace engine
{
    //订阅者登记注册
    void Event_Broker::info_register
    (const string& module_name, const vector<event>& needed_events,
        function<void(shared_ptr<event> evt)> event_entry)
    {
        //订阅者ID记录
        int32_t subscriber_ID = -1;

        //若本次为二次注册
        if (mapping_set.count(module_name))
        {
            //获取订阅者ID
            subscriber_ID = mapping_set[module_name];
            //重新注册事件入口
            event_entries[subscriber_ID] = event_entry;
        }
        //若本次为首次注册
        else
        {
            //获取当前映射数目作为新订阅者编号
            subscriber_ID = mapping_set.size();
            //注册订阅者内部ID
            mapping_set.insert({ module_name, subscriber_ID });
            //注册事件入口
            event_entries.insert({ subscriber_ID ,event_entry });
        }

        //注册需要事件
        for (int register_time = 0; register_time < needed_events.size(); register_time++)
        {
            //简化表示路径
            auto& evt = needed_events[register_time];

            //若事件所属分类未指定则略过
            if (evt.category.empty())
                continue;

            //若事件所属分类未注册则略过
            if (!acl_set.count(evt.category))
            {
                //创建该事件分类
                acl_set.insert({ evt.category,{} });
                //全订阅标记初始化
                acl_set[evt.category].push_back({});
            }

            //获取事件分类内部信息
            auto& acl_row = acl_set[evt.category];

            //匹配事件标签
            for (int match_time = 0; match_time < acl_row.size(); match_time++)
            {
                //若事件标签匹配则订阅该事件
                if (evt.tag == acl_row[match_time].tag)
                {
                    //简化表示路径
                    auto& ID_set = acl_row[match_time].ID_set;
                    //若不存在任何订阅者
                    if (ID_set.empty())
                        ID_set.push_back(subscriber_ID);
                    //若已经存在订阅者
                    else
                    {
                        //检查是否已经注册
                        for (int exam_time = 0; exam_time < ID_set.size(); exam_time++)
                        {
                            //若已经注册则处理下一事件
                            if (ID_set[exam_time] == subscriber_ID)
                                break;
                            //若未注册则注册
                            else if (exam_time == ID_set.size() - 1)
                                ID_set.push_back(subscriber_ID);
                        }
                    }

                    break;
                }
                //若匹配失败则创建该事件标签
                else if (match_time == acl_row.size() - 1)
                    acl_row.push_back({ evt.tag, { subscriber_ID } });
            }
        }

    }

    //事件接收 —— 单事件重载
    void Event_Broker::receive(shared_ptr<event> evt)
    {
        //简化表示路径
        auto& sender_object = evt->sender_object;
        auto& target_object = evt->target_object;

        //若事件所属分类不存在或未注册
        if (evt->category.empty() || !acl_set.count(evt->category))
            return;

        //若事件标签不存在
        if (evt->tag.empty())
            return;

        //若为定向发送且目标存在
        if (!target_object.empty() && mapping_set.count(target_object))
        {
            //获取目标ID
            int target_ID = mapping_set[target_object];
            //定向发送事件
            event_entries[target_ID](evt);
            //处理下一事件
            return;
        }

        //简化表示路径
        auto& acl_row = acl_set[evt->category];

        //授权订阅者ID记录
        vector<uint16_t> acled_IDs;

        //插入订阅事件分类内全部事件标签订阅者集合
        acled_IDs.insert(acled_IDs.end(),
            acl_row.front().ID_set.begin(), acl_row.front().ID_set.end());

        //在部分订阅集合内匹配授权订阅者
        for (int match_time = 1; match_time < acl_row.size(); match_time++)
        {
            //简化表示路径
            auto& acl = acl_row[match_time];
            //若事件标签匹配
            if (evt->tag == acl.tag)
                acled_IDs.insert(acled_IDs.end(), acl.ID_set.begin(),
                    acl.ID_set.end());
        }

        //事件发起者ID记录
        int32_t sender_ID;
        //获取事件发送者ID
        auto it = mapping_set.find(sender_object);
        //若事件发起者映射存在
        if (it != mapping_set.end())
            sender_ID = it->second;
        //若事件发起者映射不存在
        else
            sender_ID = -1;

        //发送事件
        for (int send_time = 0; send_time < acled_IDs.size(); send_time++)
        {
            //若非事件发起者
            if(acled_IDs[send_time] != sender_ID)
                event_entries[acled_IDs[send_time]](evt);
        }
    }

    //事件接收 —— 多事件重载
    void Event_Broker::receive(vector<shared_ptr<event>> event_set)
    {
        //批处理事件
        for (int process_time = 0; process_time < event_set.size(); process_time++)
            receive(event_set[process_time]);
    }

    //事件处理 —— 单事件重载
    shared_ptr<event> Event_Broker::process(shared_ptr<event> evt)
    {
        //简化表示路径
        auto& config = evt->config;

        //若未定义事件发送者则直接返回
        if (evt->sender_object.empty())
        {
            Log::warn("Event_Broker::事件发送者未定义\n事件无法处理");
            return;
        }
        //若未定义事件目标则直接返回
        if (evt->target_object.empty())
        {
            Log::warn("Event_Broker::事件目标未定义\n事件无法处理");
            return;
        }
        //若事件发送者不存在则直接返回
        if (!mapping_set.count(evt->sender_object))
        {
            Log::warn("Event_Broker::事件发送者不存在\n事件无法处理");
            return;
        }

        //若事件大类可处理
        if (evt->category == "Subscriber")
        {
            //若事件标签可处理
            if (evt->tag == "Check" || evt->tag == "Call")
            {
                //构造回复事件
                shared_ptr<event> response_event(new(nothrow)event("Subscriber","Response"));
                //查询目标对象
                auto it = mapping_set.find(evt->target_object);
                //若目标对象不存在
                if (it == mapping_set.end())
                    //设置目标对象不存在
                    response_event->config["object_existence"] = false;
                else
                {
                    //设置目标对象存在
                    response_event->config["object_existence"] = true;
                    //若为目标呼叫事件
                    if (evt->tag == "Call")
                        //将呼叫事件转发给目标对象
                        event_entries[it->second](evt);
                }

                //返回答复事件
                return response_event;
            }
            
        }
    }

    //事件处理 —— 多事件重载
    vector<shared_ptr<event>> Event_Broker::process(vector<shared_ptr<event>> event_set)
    {
        //回复事件缓冲
        vector<shared_ptr<event>> buffer{};
        //处理事件
        for (auto& evt:event_set)
            buffer.push_back(process(evt));
        //返回回复事件集合
        return buffer;
    }
}

