#include "../局部命名空间使用.h"
#include "src/tools/Logging/日志系统运行包.h"

//引擎命名空间
namespace engine
{
    //订阅者登记注册
    void Event_Broker::attach
    (const string& subscriber, const vector<Event>& needed_events,
        function<void(shared_ptr<Event> evt)> receiver)
    {
        //若接入者信息为空
        if (subscriber.empty())
        {
            logger.warn("Event_Broker::订阅者不可为空\n接入失败");
            return;
        }

        //订阅者ID记录
        int32_t subscriber_ID = -1;

        //若本次为二次注册
        if (mapping_set.count(subscriber))
        {
            //获取订阅者ID
            subscriber_ID = mapping_set[subscriber];
            //若订阅事件集为空且接收者不存在
            if (needed_events.empty() && !receiver)
            {
                //取消订阅者编号映射
                mapping_set.erase(subscriber);
                //卸载订阅者接收入口
                receivers.erase(subscriber_ID);
                //清除订阅者接收权限
                for (auto& acl:acl_set)
                {
                    //简化表示路径
                    auto& subscribers = acl.second;
                    for (uint64_t index = 0;index < subscribers.size();index++)
                    {
                        //若ID匹配则清除该权限
                        if (subscribers[index] == subscriber_ID)
                        {
                            subscribers.erase(subscribers.begin() + index);
                            break;
                        }
                    }
                }
                
                logger.info("Event_Broker::检测到二次接入未包含有效接入信息");
                logger.info("已满足接入卸载条件");
                logger.info("事件中转站接入已下线");

                return;
            }
            else
                //重新注册接收入口
                receivers[subscriber_ID] = receiver;
        }
        //若本次为首次注册
        else
        {
            //若订阅事件集为空且接收者不存在
            if (needed_events.empty() && !receiver)
            {
                logger.info("Event_Broker::检测到接入未包含有效接入信息");
                logger.info("接入失败");
                return;
            }
            //获取当前映射数目作为新订阅者编号
            subscriber_ID = mapping_set.size();
            //注册订阅者内部ID
            mapping_set.insert({ subscriber, subscriber_ID });
            //注册事件入口
            receivers.insert({ subscriber_ID ,receiver });
        }

        //注册需要事件
        for (int register_time = 0; register_time < needed_events.size(); register_time++)
        {
            //简化表示路径
            auto& evt = needed_events[register_time];

            //若事件标识信息不完整
            if (evt.category.empty() || evt.tag.empty())
            {
                logger.warn("Event_Broker::订阅事件标识信息不完整\n已略过该事件");
                continue;
            }

            //获取授权ID集合
            auto& acl_IDs = acl_set[{evt.category, evt.tag}];
            //已授权标记
            bool is_added = false;
            //匹配权限是否已存在
            for (auto& acl_ID:acl_IDs)
            {
                //若已授权
                if (acl_ID == subscriber_ID)
                {
                    is_added = true;
                    break;
                }
            }

            //若事件尚未授权
            if (!is_added)
                acl_IDs.push_back(subscriber_ID);
        }
    }

    //事件接收 —— 单事件重载
    void Event_Broker::receive(shared_ptr<Event> evt)
    {
        //简化表示路径
        auto& sender_object = evt->sender_object;
        auto& target_object = evt->target_object;

        //若事件标识信息不完整
        if (evt->category.empty() || evt->tag.empty())
        {
            logger.warn("Event_Broker::事件标识信息不完整\n已略过该事件处理");
            return;
        }
        //若事件标识未注册
        if (!acl_set.count({ evt->category,evt->tag }))
        {
            logger.warn("Event_Broker::事件订阅者不存在\n已略过该事件处理");
            return;
        }
        //若事件标签为已占用字段
        if(evt->tag == "All")
        {
            logger.warn("Event_Broker::All标签已占用\n已略过该事件处理");
            return;
        }

        //若为定向发送且目标存在
        if (!target_object.empty() && mapping_set.count(target_object))
        {
            //获取目标ID
            uint64_t target_ID = mapping_set[target_object];
            //定向发送事件
            receivers[target_ID](evt);
            //处理下一事件
            return;
        }

        //事件发起者ID记录
        optional<uint64_t> sender_ID = nullopt;
        //获取事件发送者ID
        auto it = mapping_set.find(sender_object);
        //若事件发起者映射存在
        if (it != mapping_set.end())
            sender_ID = it->second;

        //获取该事件标识订阅者列表
        vector<uint64_t> acl_IDs = acl_set[{ evt->category, evt->tag }];
        //发送事件
        for (int send_time = 0; send_time < acl_IDs.size(); send_time++)
        {
            //若非事件发起者
            if (acl_IDs[send_time] != sender_ID)
                receivers[acl_IDs[send_time]](evt);
        }
       
        //获取该事件分类满订订阅者
        acl_IDs = acl_set[{ evt->category, "All" }];
        //发送事件
        for (int send_time = 0; send_time < acl_IDs.size(); send_time++)
                receivers[acl_IDs[send_time]](evt);
    }

    //事件接收 —— 多事件重载
    void Event_Broker::receive(vector<shared_ptr<Event>> event_set)
    {
        //批处理事件
        for (int process_time = 0; process_time < event_set.size(); process_time++)
            receive(event_set[process_time]);
    }

    //事件处理 —— 单事件重载
    shared_ptr<Event> Event_Broker::process(shared_ptr<Event> evt)
    {
        //若事件为空
        if (evt == nullptr)
        {
            logger.warn("Event_Broker::当前事件未分配内存\n已略过该事件");
            return nullptr;
        }

        //简化表示路径
        auto& config = evt->config;

        //若未定义事件发送者则直接返回
        if (evt->sender_object.empty() || evt->target_object.empty())
        {
            logger.warn("Event_Broker::事件收发对象定义不完整\n已略过该事件");
            return nullptr;
        }

        //若事件发送者不存在则直接返回
        if (!mapping_set.count(evt->sender_object))
        {
            logger.warn("Event_Broker::事件发送者不存在\n已略过该事件");
            return nullptr;
        }

        //若事件大类可处理
        if (evt->category == "Subscriber")
        {
            //若事件标签可处理
            if (evt->tag == "Check" || evt->tag == "Call")
            {
                //构造回复事件
                shared_ptr<Event> response_event(new(nothrow)Event("Subscriber","Response"));
                //注入查询目标对象名
                response_event->config["object"] = evt->target_object;
                //查询目标对象
                auto it = mapping_set.find(evt->target_object);
                //若目标对象不存在
                if (it == mapping_set.end())
                    //设置目标对象不存在
                    response_event->config["existence"] = false;
                else
                {
                    //设置目标对象存在
                    response_event->config["existence"] = true;
                    //若为目标呼叫事件
                    if (evt->tag == "Call")
                        //将呼叫事件转发给目标对象
                        receivers[it->second](evt);
                }

                //返回答复事件
                return response_event;
            }
            
        }

        //返回空事件（无可处理分类）
        return nullptr;
    }

    //事件处理 —— 多事件重载
    vector<shared_ptr<Event>> Event_Broker::process(vector<shared_ptr<Event>> event_set)
    {
        //回复事件缓冲
        vector<shared_ptr<Event>> buffer{};
        //处理事件
        for (auto& evt:event_set)
            buffer.push_back(process(evt));
        //返回回复事件集合
        return buffer;
    }
}

