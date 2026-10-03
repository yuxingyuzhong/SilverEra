#include "../局部命名空间使用.h"
#include "src/tools/Logging/日志系统运行包.h"

//引擎命名空间
namespace engine
{
    //接收者存在性检查（向中转站询问目标对象是否接入）
    bool Config_Loader::receiver_check(const std::string& object)
    {
        //构造查看事件
        auto evt = event_terminal.build("Config_Loader", object, "Subscriber", "Check");
        //若事件构造失败
        if (evt == nullptr)
        {
            //记录该配置待发送对象
            waited_objects.insert(object);
            return false;
        }

        //构造中转站答复接收事件
        std::shared_ptr<Event> response;
        //查询目标对象是否存在并获取答复
        if (!event_terminal.interact(evt, response, acl_key) || response == nullptr)
        {
            //记录该配置待发送对象
            waited_objects.insert(object);
            return false;
        }

        //若答复对象存在性字段无效或目标不存在
        if (!detail::field_check<bool>(response->config, "existence") ||
            !response->config["existence"].get<bool>())
        {
            //记录为配置待发送对象
            waited_objects.insert(object);
            return false;
        }
        else
            return true;
    }

    //对象配置投递（读配置 → 发 Config/Load → 写条目标记）
    bool Config_Loader::object_config_deliver(const std::string& object, const std::string& config_path,
        uint64_t mark, json& entry)
    {
        //格式化为绝对文件路径
        path absolute_config_path = base_dir_get() / detail::string_to_path(config_path);
        //配置数据记录
        json config_data;
        //读取配置文件
        file_read(absolute_config_path, config_data);

        //若配置数据读取失败
        if (config_data.is_null())
        {
            logger.error("Config_Loader::配置读取失败");
            logger.error("失败路径:{}", detail::path_to_string(absolute_config_path));
            return false;
        }

        //剔除可能存在的脏标记字段（脏标记不参与投递）
        if (config_data.is_object())
            config_data.erase("dirty_mark");

        //构造配置事件
        auto evt = event_terminal.build("Config_Loader", object, "Config", "Load");
        //若配置事件构造失败
        if (evt == nullptr)
        {
            logger.error("Config_Loader::配置事件构造失败");
            return false;
        }

        //填充配置数据
        evt->config = config_data;
        //发送配置事件
        if (event_terminal->interface_check(Interface_ID::EVENT_SENDOR))
            event_terminal.send(evt, acl_key);
        else if(event_terminal->interface_check(Interface_ID::EVENTS_SENDOR))
            event_terminal.send(vector<shared_ptr<Event>>{evt}, acl_key);
        else
        {
            logger.error("Config_Loader::配置事件发送失败");
            logger.error("配置工作无法完成");
            return false;
        }

        //投递成功后写入条目标记（只有真正阅读配置并发送配置才写脏标记）
        entry["dirty_mark"] = mark;
        //报告事件信息
        logger.info("Config_Loader::已发送配置事件: {}", object);
        //返回投递成功
        return true;
    }

    //单份配置文件处理
    bool Config_Loader::config_file_process(const string& config_path,json& entry,
        const string& object)
    {
        //计算该「对象×文件」本轮标记
        const uint64_t want_mark = mark_make(object, config_path);
        //脏标记命中表示本轮已加载过该对象该文件
        const bool marked = detail::field_check<uint64_t>(entry, "dirty_mark") &&
            entry["dirty_mark"].get<uint64_t>() == want_mark;

        //若本轮已加载过
        if (marked)
        {
            //若为事件明确指定则发出警告日志
            if (!object.empty())
                logger.warn("Config_Loader::事件指定但无需重新加载的文件，已跳过: {}", config_path);
            return false;
        }

        //若配置投递成功
        if (!object_config_deliver(object, config_path, want_mark, entry))
            return false;
        //若配置投递失败
        else
            return true;
    }
}