#pragma once
//预编译头
#include "common/前置头文件包含.h"
//获取预定义事件类型
#include "common/types/事件类型.h"
//获取预定义记录类型
#include "common/types/记录类型.h"
//获取事件终端
#include "src/core/event/Event_Terminal/事件终端.h"
//获取实体
#include "src/core/entity/Entity/实体.h"
//获取预定义sol2库类型别名
#include "common/external/Sol2/sol类型别名.h"
//获取配置检查器
#include "src/tools/Non_GUI/Config_Checker/配置检查器.h"
//获取对象池
#include "src/tools/Non_GUI/Object_Pool/对象池.h"
//获取引擎环境
#include "src/tools/Non_GUI/Engine_Env/引擎环境.h"
//获取路径字符串转化方法
#include "src/tools/Non_Gui/Auxi_Algorithm/路径字符串转换.h"
//获取辅助算法(如binary_search)
#include "src/tools/Non_GUI/Auxi_Algorithm/二分查找.h"

namespace engine
{
    //实体管理器
    class Entity_Manager
    {       
    private:
        //实体记录
        struct entity_record : public object_record
        {
            //实体
            Entity entity{};
        };
        //订阅事件集合
        std::unordered_set<config_event> event_map{};
    public:
        //事件终端
        Event_Terminal event_terminal;
    private:
        //事件发送权限密钥
        int64_t acl_key = 0;

        //属性槽分发权限密钥
        std::optional<uint64_t> distribute_key = std::nullopt;
        //属性槽配置脚本加载路径集合
        std::unordered_map<std::string, LuaState> prop_config_paths;
        //行为脚本加载路径集合
        std::unordered_map<std::string,std::string> action_load_path;
        //属性槽记录集合
        Object_Pool<prop_record> prop_records;
        //实体记录集合
        Object_Pool<entity_record> entity_records;

    public:
        //构造函数
        Entity_Manager();
        //析构函数
        ~Entity_Manager() = default;
        //事件中转站接入
        void attach(void);

    private:
        //配置字段检验
        bool config_field_parse(const nlohmann::json& config);
        //决策树加载路径注册
        void action_load_path_register(const std::string& entity_type, const nlohmann::json& config);
        //属性槽配置加载路径注册
        void prop_load_path_register(const std::string& entity_type, const nlohmann::json& config);

    public:
        //实体创建
        std::vector<uint64_t> entity_build(const std::string& type, const int& counts);
        //实体卸载
        void entity_unload(std::vector<uint64_t>& ID);
        //实体行动 —— 指定实体执行
        void entity_act(std::vector<uint64_t>& IDs);
        //实体行动 —— 全量执行
        void entity_act(void);
        //属性槽分发密钥生成
        bool distribute_key_gen(void);
        //属性槽获取
        Object_Pool<prop_record>* prop_slot_get(const uint64_t& distribute_key);

    public:
        //事件广播
        void event_broadcast(std::shared_ptr<config_event> event);
        //事件定向发送
        bool event_unicast(const std::string& type,const uint64_t& ID,
            std::shared_ptr<config_event> event);
    private:
        //事件处理
        void event_process(std::shared_ptr<config_event> evt);
    };

}