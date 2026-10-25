#include "src/core/event/Event_Broker/局部命名空间使用.h"
#include "src/tools/Logging/日志系统运行包.h"

//引擎命名空间
namespace engine
{
    //构造函数
    Event_Broker::Event_Broker(void)
    {
        //生成自身权限密钥
        uint64_t acl_key = static_cast<uint64_t>(std::hash<std::string>{}("Event_Broker"));
        //封装事件处理入口
        auto evt_process = [this](std::shared_ptr<Event> evt)
            {
                this->process(evt);
            };
        //自身接入事件系统
        attach("Event_Broker", acl_key, evt_process);

        //中转站保留事件标识（自身处理契约的入口标识）
        vector<Event_Identity> reserved_identities{
            Event_Identity("Subscriber", "Check"),
            Event_Identity("Subscriber", "Detach"),
            Event_Identity("Subscribed_Events", "Add"),
            Event_Identity("Subscribed_Events", "Remove")
        };
        //逐条登记自身订阅权限（否则这些事件会被事件接收直接丢弃）
        for (const auto& identity : reserved_identities)
            acl_set[identity].push_back(acl_key);
    }

    //订阅者登记注册
    void Event_Broker::attach
    (const std::string& subscriber, const uint64_t& acl_key,
        std::function<void(std::shared_ptr<Event>)> receiver)
    {
        //若接入者为空或事件接收器不存在
        if (subscriber.empty() || !receiver)
        {
            logger.warn("Event_Broker::订阅者为空或未包含有效事件接收器");
            logger.info("接入失败");
            return;
        }

        //注册订阅者权限密钥
        mapping.insert_or_assign(subscriber, acl_key);
        //注册事件接收器
        receivers.insert_or_assign(acl_key, receiver);
    }

    //事件接收 —— 单事件重载
    void Event_Broker::receive(shared_ptr<Event> evt)
    {
        //若事件为空
        if (!evt)
        {
            logger.warn("Event_Broker::事件为空\n已略过该事件处理");
            return;
        }

        //简化表示路径
        auto& sender = evt->sender;
        auto& receiver = evt->receiver;

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

        //若为定向发送
        if (!receiver.empty())
        {
            //若目标存在
            if(mapping.count(receiver))
            {
                //获取目标权限密钥
                uint64_t target_key = mapping[receiver];
                //使用密钥映射事件接收端口定向发送事件
                receivers[target_key](evt);
            }
            //处理下一事件
            return;
        }

        //事件发起者权限密钥记录
        std::optional<uint64_t> sender_key = nullopt;
        //查找事件发起者
        auto it = mapping.find(sender);
        //若事件发起者存在
        if (it != mapping.end())
            sender_key = it->second;

        //获取该事件标识订阅者列表
        vector<uint64_t> acl_keys = acl_set[{ evt->category, evt->tag }];
        //获取该事件分类满订阅者列表
        acl_keys.insert(acl_keys.end(),
            acl_set[{ evt->category, "All" }].begin(), acl_set[{ evt->category, "All" }].end());

        //发送事件
        for (int send_time = 0; send_time < acl_keys.size(); send_time++)
        {
            //若启用事件发起者屏蔽
            if (!sender_key.has_value() || acl_keys[send_time] != sender_key)
                receivers[acl_keys[send_time]](evt);
        }
    }

    //事件接收 —— 多事件重载
    void Event_Broker::receive(vector<shared_ptr<Event>> event_set)
    {
        //批处理事件
        for (int process_time = 0; process_time < event_set.size(); process_time++)
            receive(event_set[process_time]);
    }

    //事件处理
    void Event_Broker::process(shared_ptr<Event> evt)
    {
        //若事件为空
        if (!evt)
        {
            logger.warn("Event_Broker::当前事件未分配内存\n已略过该事件");
            return;
        }

        //简化表示路径
        auto& config = evt->config;

        //若目标对象字段无效
        if (!detail::field_check<string>(config, "object"))
        {
            logger.warn("Event_Broker::目标对象字段无效\n已略过该事件");
            return;
        }

        //若为订阅者分支
        if (evt->category == "Subscriber")
        {
            //若为订阅者查看分支
            if(evt->tag == "Check")
            {
                //获取待查询对象名
                string obejct = config["object"];
                //查询目标对象
                auto it = mapping.find(obejct);
                //若目标对象不存在
                if (it == mapping.end())
                    //设置目标对象不存在
                    config["subscribed"] = false;
                else
                    //设置目标对象存在
                    config["subscribed"] = true;
            }
            //若为订阅者退出分支
            else if (evt->tag == "Detach")
            {
                //获取待退出订阅者名称
                string object = config["object"];
                //待退出订阅者权限密钥记录
                uint64_t suscriber_key;
                //查找待退出订阅者
                auto it = mapping.find(object);
                //若待退出订阅者存在
                if (it != mapping.end())
                    suscriber_key = it->second;
                //若待退出订阅者不存在
                else
                {
                    logger.warn("Event_Broker::目标订阅者不存在");
                    logger.warn("Event_Broker::无法卸载其订阅记录");
                    return;
                }
                
                //若目标对象权限密钥字段无效
                if (!detail::field_check<uint64_t>(config, "acl_key"))
                {
                    logger.warn("Event_Broker::目标对象权限密钥字段无效\n已略过该事件");
                    return;
                }
                //检查密钥是否匹配
                if (suscriber_key == config["acl_key"])
                {
                    //取消订阅者编号映射
                    mapping.erase(object);
                    //卸载订阅者接收入口
                    receivers.erase(suscriber_key);
                    //清除订阅者接收权限
                    for (auto& acl : acl_set)
                    {
                        //简化表示路径
                        auto& subscribers = acl.second;
                        for (uint64_t index = 0; index < subscribers.size(); index++)
                        {
                            //若ID匹配则清除该权限
                            if (subscribers[index] == suscriber_key)
                            {
                                subscribers.erase(subscribers.begin() + index);
                                break;
                            }
                        }
                    }

                    logger.info("事件中转站接入已下线");
                }
                else
                {
                    logger.error("Event_Broker::权限密钥不匹配");
                    logger.error("Event_Broker::已驳回该违规操作");
                } 
            }
        }
        //若为订阅事件分支
        if (evt->category == "Subscribed_Events")
        {
            //获取目标订阅者名称
            string object = config["object"];
            //目标订阅者权限密钥记录
            uint64_t suscriber_key;
            //查找目标订阅者
            auto it = mapping.find(object);
            //若目标订阅者存在
            if (it != mapping.end())
                suscriber_key = it->second;
            //若目标订阅者不存在
            else
            {
                logger.warn("Event_Broker::目标订阅者不存在");
                logger.warn("Event_Broker::无法更新其订阅记录");
                return;
            }

            //若目标对象权限密钥字段无效
            if (!detail::field_check<uint64_t>(config, "acl_key"))
            {
                logger.warn("Event_Broker::目标对象权限密钥字段无效\n已略过该事件");
                return;
            }
            //检查密钥是否匹配
            if (suscriber_key == config["acl_key"])
            {
                //若为订阅事件添加分支
                if (evt->tag == "Add")
                {
                    //若订阅事件清单缺失或非数组
                    if (!detail::field_check<nlohmann::json>(config, "subscribed_events") ||
                        !config["subscribed_events"].is_array())
                    {
                        logger.warn("Event_Broker::订阅事件清单缺失或非数组\n已略过该事件");
                        return;
                    }

                    //逐条登记订阅事件
                    for (const auto& subscribed_event : config["subscribed_events"])
                    {
                        //若订阅事件标识信息不完整
                        if (!detail::field_check<string>(subscribed_event, "category") ||
                            !detail::field_check<string>(subscribed_event, "tag"))
                        {
                            logger.warn("Event_Broker::订阅事件标识信息不完整\n已略过该事件");
                            continue;
                        }

                        //获取订阅事件标识
                        Event_Identity identity(subscribed_event["category"], subscribed_event["tag"]);
                        //获取该事件标识订阅者列表
                        auto& acl_keys = acl_set[identity];
                        //已授权标记
                        bool is_added = false;
                        //匹配订阅者是否已授权
                        for (uint64_t index = 0; index < acl_keys.size(); index++)
                        {
                            //若订阅者已授权
                            if (acl_keys[index] == suscriber_key)
                            {
                                is_added = true;
                                break;
                            }
                        }
                        //若订阅者尚未授权
                        if (!is_added)
                            //登记订阅者权限密钥
                            acl_keys.push_back(suscriber_key);
                    }

                    logger.info("事件中转站订阅事件已登记");
                }
                //若为订阅事件移除分支
                else if (evt->tag == "Remove")
                {
                    //若订阅事件清单缺失或非数组
                    if (!detail::field_check<nlohmann::json>(config, "subscribed_events") ||
                        !config["subscribed_events"].is_array())
                    {
                        logger.warn("Event_Broker::订阅事件清单缺失或非数组\n已略过该事件");
                        return;
                    }

                    //逐条移除订阅事件
                    for (const auto& subscribed_event : config["subscribed_events"])
                    {
                        //若订阅事件标识信息不完整
                        if (!detail::field_check<string>(subscribed_event, "category") ||
                            !detail::field_check<string>(subscribed_event, "tag"))
                        {
                            logger.warn("Event_Broker::订阅事件标识信息不完整\n已略过该事件");
                            continue;
                        }

                        //获取订阅事件标识
                        Event_Identity identity(subscribed_event["category"], subscribed_event["tag"]);
                        //查找该事件标识订阅者列表
                        auto acl_it = acl_set.find(identity);
                        //若该事件标识未登记任何订阅者
                        if (acl_it == acl_set.end())
                        {
                            logger.warn("Event_Broker::订阅事件标识未登记\n已略过该事件");
                            continue;
                        }
                        //获取订阅者列表
                        auto& acl_keys = acl_it->second;
                        //逐项匹配待移除订阅者
                        for (uint64_t index = 0; index < acl_keys.size(); index++)
                        {
                            //若订阅者已授权
                            if (acl_keys[index] == suscriber_key)
                            {
                                //移除订阅者权限密钥
                                acl_keys.erase(acl_keys.begin() + index);
                                break;
                            }
                        }
                    }

                    logger.info("事件中转站订阅事件已注销");
                }
            }
            else
            {
                logger.error("Event_Broker::权限密钥不匹配");
                logger.error("Event_Broker::已驳回该违规操作");
            }
        }
    }

}

