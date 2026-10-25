#pragma once
//预编译头
#include "Engine/EngineCore/common/前置头文件包含.h"
//获取事件系统运行包（事件终端与事件类型）
#include "Engine/EngineCore/src/core/event/事件系统运行包.h"

//游戏引擎命名空间
namespace engine
{
    /*
    Logging_Config_Bridge

    职责：把「经事件中转站下发」的日志配置，翻译为日志系统（Log）的纯数据设置调用。
    分层：本模块位于 core 层，与 Config_Loader 同级。向下只依赖 Logging 的公开设置接口，
          不反向让 Logging 包含事件系统（否则 Logging 与 Event_Broker 之间形成头文件循环）。
    成员声明顺序（声明与实现同序）
        公开门面置顶（构造 / 析构 / 接入），其后的私有方法按「被依赖者在依赖者前面」
        与「功能相近者相邻排列」分成三组：
            组1 配置应用 · 组2 配置卸载 · 组3 事件分发
        实现集中于 core/Logging_Config_Bridge.cpp，其中函数顺序与本声明顺序一致。
    */
    class Log_Config_Bridge
    {
    public:
        //事件终端
        Event_Terminal event_terminal;
    private:
        //权限密钥
        int64_t acl_key = 0;
        //已绑定输出流路径记录（卸载时按此清单逐条清除绑定）
        std::vector<std::string> bound_paths{};

    public:
        //构造函数
        Log_Config_Bridge()
        {
            //生成权限密钥
            this->acl_key = event_terminal.key().value();
        }
        //析构函数
        ~Log_Config_Bridge() = default;

        //事件终端接入
        void attach(void);

    private:
        // ---------- 组1 配置应用 ----------

        //根目录配置应用（root 字段 → 日志根目录作用域）
        void root_apply(const nlohmann::json& config);
        //规则优先级配置应用（priority 字段 → 级别与归属的判定先后）
        void priority_apply(const nlohmann::json& config);
        //全局取反配置应用（verdict_invert 字段 → 是否对许可判定最终结果取反）
        void verdict_apply(const nlohmann::json& config);
        //输出流配置应用（streams 数组 → 路径节点绑定文件输出流）
        void streams_apply(const nlohmann::json& config);
        //文件许可配置应用（files 对象 → 文件输出白/黑名单）
        void files_apply(const nlohmann::json& config);
        //目录许可配置应用（dirs 对象 → 目录输出白/黑名单）
        void dirs_apply(const nlohmann::json& config);
        //级别许可配置应用（levels 对象 → 级别输出白/黑名单）
        void levels_apply(const nlohmann::json& config);

        // ---------- 组2 配置卸载 ----------

        //输出流绑定还原（按已绑定清单逐条清除绑定）
        void streams_revert(void);
        //配置卸载（输出流绑定 / 三张许可表 / 根目录一并复位）
        void config_unload(void);

        // ---------- 组3 事件分发 ----------

        //事件处理
        void event_process(std::shared_ptr<Event> evt);
    };
}