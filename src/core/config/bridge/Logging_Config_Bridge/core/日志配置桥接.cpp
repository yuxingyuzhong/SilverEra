#include "src/core/config/bridge/Logging_Config_Bridge/局部命名空间使用.h"
//获取日志系统
#include "src/tools/Logging/日志系统运行包.h"
//获取 JSON 字段校验工具
#include "src/tools/Detail/package/数据校验工具.h"

//引擎命名空间
namespace engine
{
    //事件终端接入
    void Log_Config_Bridge::attach(void)
    {
        //注册本模块事件接收入口（事件送达后交由事件处理分派）
        event_terminal->bind(Port_ID::Evt_Receiver,
            [this](shared_ptr<Event> evt) { this->event_process(evt); });
        //若事件接收入口注册失败
        if (!event_terminal->check(Port_ID::Evt_Receiver))
        {
            logger.error("Log_Config_Bridge::事件接收入口注册失败，接入中止");
            return;
        }

        //待订阅事件清单（日志配置加载 / 卸载）
        vector<Event> needed_events{
            Event("", "", "Log", "Load"),
            Event("", "", "Log", "Unload"),
        };

        //接入事件中转站（仅登记订阅者身份与接收入口）
        if (!event_terminal.attach("Log_Config_Bridge", acl_key))
        {
            logger.error("Log_Config_Bridge::事件中转站接入失败");
            return;
        }

        //构造订阅事件清单
        json subscribed_events = json::array();
        //逐条写入订阅事件标识
        for (const auto& needed_event : needed_events)
        {
            //单条订阅事件标识
            json subscribed_event;
            subscribed_event["category"] = needed_event.category;
            subscribed_event["tag"] = needed_event.tag;
            subscribed_events.push_back(subscribed_event);
        }

        //构造订阅事件（目标对象名与权限密钥经载荷传递）
        auto attach_evt = event_terminal.build("", "", "Subscribed_Events", "Add");
        //若事件构造失败
        if (attach_evt == nullptr)
        {
            logger.error("Log_Config_Bridge::订阅事件构造失败");
            return;
        }
        //写入目标对象名、权限密钥与订阅清单
        attach_evt->config["object"] = "Log_Config_Bridge";
        attach_evt->config["acl_key"] = static_cast<uint64_t>(acl_key);
        attach_evt->config["subscribed_events"] = subscribed_events;
        //发送订阅事件（登记模块订阅清单）
        if (!event_terminal.send(attach_evt, acl_key))
            logger.error("Log_Config_Bridge::订阅事件发送失败");
    }

    //根目录配置应用
    void Log_Config_Bridge::root_apply(const json& config)
    {
        //若未定义 root 字段
        if (!config.contains("root"))
            return;
        //若 root 字段非字符串
        if (!config["root"].is_string())
        {
            logger.error("Log_Config_Bridge::root 字段非字符串\n已驳回");
            return;
        }
        //取根目录字符串（允许空串，表示整个项目）
        const string root = config["root"].get<string>();
        //登记日志根目录
        logger.root_set(root);
        //打印根目录登记结果
        logger.info("Log_Config_Bridge::日志根目录已设置: {}",
            root.empty() ? string("<整个项目>") : root);
    }

    //输出流配置应用
    void Log_Config_Bridge::streams_apply(const json& config)
    {
        //若未定义 streams 字段
        if (!config.contains("streams"))
            return;
        //若 streams 字段非数组
        if (!config["streams"].is_array())
        {
            logger.error("Log_Config_Bridge::streams 字段非数组\n已驳回");
            return;
        }
        //逐条处理输出流条目
        for (const auto& item : config["streams"])
        {
            //若条目字段无效
            if (!item.is_object() ||
                !detail::field_check<string>(item, "path") ||
                !detail::field_check<string>(item, "file"))
            {
                logger.warn("Log_Config_Bridge::输出流条目字段无效，已略过该条目");
                continue;
            }
            //取路径键与日志文件名
            const string path = item["path"].get<string>();
            const string file = item["file"].get<string>();
            //若输出流绑定失败
            if (!logger.stream_bind(path, file))
            {
                logger.warn("Log_Config_Bridge::输出流绑定失败，已略过该条目: {}", path);
                continue;
            }
            //记录已绑定路径，供卸载时清除
            bound_paths.push_back(path);
        }
    }

    //文件许可配置应用
    void Log_Config_Bridge::files_apply(const json& config)
    {
        //若未定义 files 字段
        if (!config.contains("files"))
            return;
        //若 files 字段非对象
        if (!config["files"].is_object())
        {
            logger.error("Log_Config_Bridge::files 字段非对象\n已驳回");
            return;
        }
        //逐条处理文件许可
        for (const auto& rule : config["files"].items())
        {
            //若许可值非布尔
            if (!rule.value().is_boolean())
            {
                logger.warn("Log_Config_Bridge::文件许可值非布尔，已略过: {}", rule.key());
                continue;
            }
            //登记文件输出许可（true 白名单 / false 黑名单）
            logger.file_rule_set(rule.key(), rule.value().get<bool>());
        }
    }

    //目录许可配置应用
    void Log_Config_Bridge::dirs_apply(const json& config)
    {
        //若未定义 dirs 字段
        if (!config.contains("dirs"))
            return;
        //若 dirs 字段非对象
        if (!config["dirs"].is_object())
        {
            logger.error("Log_Config_Bridge::dirs 字段非对象\n已驳回");
            return;
        }
        //逐条处理目录许可
        for (const auto& rule : config["dirs"].items())
        {
            //若许可值非布尔
            if (!rule.value().is_boolean())
            {
                logger.warn("Log_Config_Bridge::目录许可值非布尔，已略过: {}", rule.key());
                continue;
            }
            //登记目录输出许可（作用于其后代调用处）
            logger.dir_rule_set(rule.key(), rule.value().get<bool>());
        }
    }

    //级别许可配置应用
    void Log_Config_Bridge::levels_apply(const json& config)
    {
        //若未定义 levels 字段
        if (!config.contains("levels"))
            return;
        //若 levels 字段非对象
        if (!config["levels"].is_object())
        {
            logger.error("Log_Config_Bridge::levels 字段非对象\n已驳回");
            return;
        }
        //逐条处理级别许可
        for (const auto& rule : config["levels"].items())
        {
            //若许可值非布尔
            if (!rule.value().is_boolean())
            {
                logger.warn("Log_Config_Bridge::级别许可值非布尔，已略过: {}", rule.key());
                continue;
            }
            //登记级别输出许可（INFO / WARN / ERROR / DEBUG）
            logger.level_rule_set(rule.key(), rule.value().get<bool>());
        }
    }

    //输出流绑定还原
    void Log_Config_Bridge::streams_revert(void)
    {
        //按已绑定清单逐条清除绑定
        for (const auto& path : bound_paths)
            logger.stream_clear(path);
        //清空绑定记录
        bound_paths.clear();
    }

    //配置卸载
    void Log_Config_Bridge::config_unload(void)
    {
        //清除本模块登记的输出流绑定
        streams_revert();
        //清空三张输出许可表
        logger.rule_clear();
        //清除日志根目录
        logger.root_clear();
        //打印卸载完成
        logger.info("Log_Config_Bridge::日志配置已卸载，恢复初始态");
    }

    //事件处理
    void Log_Config_Bridge::event_process(std::shared_ptr<Event> evt)
    {
        //日志配置加载
        if (evt->category == "Log" && evt->tag == "Load")
        {
            //应用根目录配置
            root_apply(evt->config);
            //应用输出流配置
            streams_apply(evt->config);
            //应用文件许可配置
            files_apply(evt->config);
            //应用目录许可配置
            dirs_apply(evt->config);
            //应用级别许可配置
            levels_apply(evt->config);
            return;
        }

        //日志配置卸载
        if (evt->category == "Log" && evt->tag == "Unload")
            config_unload();
    }
}
