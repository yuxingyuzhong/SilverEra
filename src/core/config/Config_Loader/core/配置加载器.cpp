#include "../局部命名空间使用.h"
#include "src/tools/Logging/日志系统运行包.h"

//引擎命名空间
namespace engine
{
    //构造函数
    Config_Loader::Config_Loader()
    {
        //生成权限密钥
        this->acl_key = event_terminal.acl_key_gen();
    }

    //事件终端接入
    void Config_Loader::attach(void)
    {
        //注册本模块事件接收入口（事件送达后交由事件处理分派）
        event_terminal->event_receiver_register(
            [this](shared_ptr<Event> evt) { this->event_process(evt); });
        //若事件接收入口注册失败
        if (!event_terminal->interface_check(Interface_ID::EVENT_RECEIVER))
        {
            logger.error("Config_Loader::事件接收入口注册失败，接入中止");
            return;
        }

        //待订阅事件清单
        vector<Event> needed_events{
            Event("", "", "Belived_Root", "Load"),
            Event("", "", "Belived_Root", "Unload"),
            Event("", "", "Route", "load"),
            Event("", "", "Config", "load"),
        };

        //接入事件中转站
        if (!event_terminal.attach("Config_Loader", needed_events, acl_key))
            logger.error("Config_Loader::事件中转站接入失败");
    }
}