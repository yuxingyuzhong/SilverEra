#include "src/core/config/Config_Loader/局部命名空间使用.h"
#include "src/tools/Logging/日志系统运行包.h"

//引擎命名空间
namespace engine
{
    //事件终端接入
    void Config_Loader::attach(void)
    {
        //注册本模块事件接收入口（事件送达后交由事件处理分派）
        event_terminal->bind(Port_ID::Evt_Receiver,
            [this](shared_ptr<Event> evt) { this->event_process(evt); });
        //若事件接收入口注册失败
        if (!event_terminal->check(Port_ID::Evt_Receiver))
        {
            logger.error("Config_Loader::事件接收入口注册失败，接入中止");
            return;
        }

        //接入事件中转站（仅登记订阅者身份与接收入口）
        if (!event_terminal.attach("Config_Loader", acl_key))
        {
            logger.error("Config_Loader::事件中转站接入失败");
            return;
        }

        //构造订阅事件清单（由预定义事件标识表生成，使订阅清单与保留判定同源）
        json subscribed_events = json::array();
        //逐条写入预定义事件标识
        for (const auto& reserved : reserved_events_get())
        {
            //单条订阅事件标识
            json subscribed_event;
            subscribed_event["category"] = reserved.first;
            subscribed_event["tag"] = reserved.second;
            subscribed_events.push_back(subscribed_event);
        }

        //构造订阅事件（目标对象名与权限密钥经载荷传递）
        auto attach_evt = event_terminal.build("", "", "Subscribed_Events", "Add");
        //若事件构造失败
        if (attach_evt == nullptr)
        {
            logger.error("Config_Loader::订阅事件构造失败");
            return;
        }
        //写入目标对象名、权限密钥与订阅清单
        attach_evt->config["object"] = "Config_Loader";
        attach_evt->config["acl_key"] = static_cast<uint64_t>(acl_key);
        attach_evt->config["subscribed_events"] = subscribed_events;
        //发送订阅事件（登记模块订阅清单）
        if (!event_terminal.send(attach_evt, acl_key))
            logger.error("Config_Loader::订阅事件发送失败");
    }

    //配置加载器预定义事件标识表（订阅清单与保留判定共用，避免两处漂移）
    const vector<std::pair<string, string>>& Config_Loader::reserved_events_get(void)
    {
        //预定义事件标识（本模块订阅的全部事件）
        static const vector<std::pair<string, string>> reserved_events{
            { "Believed_Root", "Load" },
            { "Believed_Root", "Unload" },
            { "Route", "Load" },
            { "Route", "Unload" },
            { "Config", "Load" },
            { "Config", "Unload" },
        };
        //返回预定义事件标识表
        return reserved_events;
    }

    //事件标识保留判定（该 {category, tag} 是否为预定义事件；含中转站保留标签）
    bool Config_Loader::event_identifier_reserved(const string& category, const string& tag)
    {
        //中转站保留标签（该标签下的事件会被中转站直接丢弃）
        if (tag == "All")
            return true;
        //匹配预定义事件标识
        for (const auto& reserved : reserved_events_get())
        {
            //若事件类别与标签均一致则命中保留标识
            if (reserved.first == category && reserved.second == tag)
                return true;
        }
        //未命中任何保留标识
        return false;
    }

    //事件处理
    void Config_Loader::event_process(std::shared_ptr<Event> evt)
    {
        //可信根目录修改
        if (evt->category == "Believed_Root")
        {
            //若载荷可信根目录字段无效(包含非空检查)
            if (!detail::field_check<string>(evt->config, "route") ||
                !detail::field_check<string>(evt->config, "config"))
            {
                logger.error("Config_Loader::未定义可信根目录字段\n已驳回");
                return;
            }

            //取可信根索引目录与配置目录
            const Config_Content root(evt->config["route"].get<std::string>(),
                evt->config["config"].get<std::string>());
            //若可信根目录字段为空
            if (root.route.empty() || root.config.empty())
            {
                logger.error("Config_Loader::可信根目录字段为空\n已驳回");
                return;
            }

            //若为可信根目录加载(增量加载)
            if (evt->tag == "Load")
            {
                //若该可信根目录已登记
                if (believed_roots.count(root))
                {
                    logger.warn("Config_Loader::可信根目录已登记，已跳过添加: {}",
                        detail::path_to_string(root.config));
                    return;
                }
                //登记可信根目录
                believed_roots.insert(root);
                //扫描建立对象缓存
                route_load(root, true);
                return;
            }
            //若为可信根目录卸载
            else if (evt->tag == "Unload")
            {
                //若该可信根目录未登记
                if (!believed_roots.count(root))
                {
                    logger.warn("Config_Loader::可信根目录未登记，已略过卸载");
                    return;
                }
                //清除该可信根目录下的对象缓存
                root_cache_clear(root);
                //取消可信根目录登记
                believed_roots.erase(root);
                return;
            }
        }

        //配置路由加载
        if (evt->category == "Route" && evt->tag == "Load")
        {
            //若配置路由加载对象未定义
            if (!detail::field_check<string>(evt->config, "object"))
            {
                logger.error("Config_Loader::未定义路由重加载对象\n已略过该事件处理");
                return;
            }
            //若载荷配置目录字段无效
            if (!detail::field_check<string>(evt->config, "route") ||
                !detail::field_check<string>(evt->config, "config"))
            {
                logger.error("Config_Loader::未定义配置目录字段\n已驳回");
                return;
            }

            //去索引目录加载对象
            const string object = evt->config["object"].get<std::string>();
            //取索引目录与配置目录
            const path route_dir = detail::string_to_path(evt->config["route"].get<std::string>());
            const path config_dir = detail::string_to_path(evt->config["config"].get<std::string>());
            //构造配置目录对象
            Config_Content content(route_dir, config_dir);

            //若目标对象不存在
            if (!config_routes.count(object))
            {
                logger.warn("Config_Loader::配置索引加载对象不存在\n已略过该事件处理");
                return;
            }
            //若未缓存目标配置目录
            else if (!config_routes[object].count(content))
            {
                logger.warn("Config_Loader::配置索引目录未缓存\n已略过该事件处理");
                return;
            }
            //执行配置路由加载
            route_load(Config_Content(route_dir, config_dir),true,object);
            return;
        }

        //配置路由卸载
        if (evt->category == "Route" && evt->tag == "Unload")
        {
            //若配置路由卸载对象未定义
            if (!detail::field_check<string>(evt->config, "object"))
            {
                logger.error("Config_Loader::未定义路由卸载对象\n已略过该事件处理");
                return;
            }
            //若载荷配置目录字段无效
            if (!detail::field_check<string>(evt->config, "route") ||
                !detail::field_check<string>(evt->config, "config"))
            {
                logger.error("Config_Loader::未定义配置目录字段\n已驳回");
                return;
            }

            //取卸载对象名称
            const string object = evt->config["object"].get<std::string>();
            //取索引目录与配置目录
            const path route_dir = detail::string_to_path(evt->config["route"].get<std::string>());
            const path config_dir = detail::string_to_path(evt->config["config"].get<std::string>());
            //构造配置目录对象
            const Config_Content content(route_dir, config_dir);

            //若目标对象不存在
            if (!config_routes.count(object))
            {
                logger.warn("Config_Loader::配置索引卸载对象不存在\n已略过该事件处理");
                return;
            }
            //若未缓存目标配置目录
            else if (!config_routes[object].count(content))
            {
                logger.warn("Config_Loader::配置索引目录未缓存\n已略过该事件处理");
                return;
            }
            //执行配置路由卸载（保留配置目录登记，使同目录对可再次加载重建）
            route_unload(content, object);
            return;
        }

        //对象配置加载
        if (evt->category == "Config" && evt->tag == "Load")
        {
            //若载荷缺少对象字段
            if (!detail::field_check<string>(evt->config,"object"))
            {
                logger.error("Config_Loader::对象字段无效\n已略过该事件处理");
                return;
            }
            //取目标对象名称
            const std::string object = evt->config["object"];
            //加载指定配置文件
            config_load(object,evt->config);
            return;
        }

        //对象配置卸载
        if (evt->category == "Config" && evt->tag == "Unload")
        {
            //若载荷缺少对象字段
            if (!detail::field_check<string>(evt->config, "object"))
            {
                logger.error("Config_Loader::对象字段无效\n已略过该事件处理");
                return;
            }
            //取目标对象名称
            const std::string object = evt->config["object"];
            //卸载指定对象配置缓存
            config_unload(object, evt->config);
            return;
        }
    }
}
