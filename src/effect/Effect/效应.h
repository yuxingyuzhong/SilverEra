#pragma once
//预编译头
#include "Engine/EngineSystem/common/前置头文件包含.h"
//获取预定义事件类型
#include "Engine/EngineCore/src/core/event/Event/事件.h"
//获取事件终端
#include "Engine/EngineCore/src/core/event/Event_Terminal/事件终端.h"
//获取C++类型Lua端注册方法
#include "Engine/EngineSystem/common/external/Sol2/sol类型注册.h"
//获取预定义sol2库类型别名
#include "Engine/EngineSystem/common/external/Sol2/sol类型别名.h"
//获取数据校验工具
#include "Engine/EngineCore/src/tools/Detail/package/数据校验工具.h"
//获取引擎环境
#include "Engine/EngineCore/src/tools/Engine_Env/引擎环境.h"
//获取路径字符串转化方法
#include "Engine/EngineCore/src/tools/Detail/路径字符串转换.h"

//脚本系统模块
namespace engine
{
    //效应
    class Prop_Effect
    {
    private:
        //效应归属
        uint64_t inclusion{};
        //效应编号
        std::optional<uint64_t> ID;
        //效应名称
        std::string name{};

        //Lua脚本实例
        LuaState script;
        //效应作用对象
        std::unordered_map<std::string, double>* effect_object;
    public:
        //事件终端
        Event_Terminal event_terminal;
    private:
        //事件发送密钥
        int64_t acl_key = 0;
    public:
        //构造函数
        Prop_Effect();
        //析构函数
        ~Prop_Effect();
        //默认移动构造
        Prop_Effect(Prop_Effect&&) = default;   
        //默认移动赋值
        Prop_Effect& operator=(Prop_Effect&&) = default; 

        //配置读取
        bool config_read(const nlohmann::json& config);
        //数据注入
        void data_injection(void);
        //编号绑定
        void ID_bind(const uint64_t& ID);
        //作用对象绑定
        void effect_object_bind(std::unordered_map<std::string, double>* object);

        //效应名称获取
        const std::string& effect_name_get(void) const;

        //效应触发
        void effect_act(void);
    };
}
