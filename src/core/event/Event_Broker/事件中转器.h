#pragma once
//预编译头
#include "Engine/EngineCore/common/前置头文件包含.h"
//获取预定义事件类型
#include "../event/事件.h"

namespace engine
{
    //事件标识
    struct Event_Identity
    {
        //事件分类
        std::string category{};
        //事件标签
        std::string tag{};

        //默认构造函数
        Event_Identity()
        {

        }
        //含参构造函数 —— 构造事件标签
        Event_Identity(const std::string& category, const std::string& tag)
        {
            this->category = category;
            this->tag = tag;
        }

        //使用默认等于运算符
        bool operator==(const Event_Identity& other) const
        {
            if (this->category == other.category &&
                this->tag == other.tag)
                return true;
            else
                return false;
        }
    };

}

//std 哈希特化 —— 必须先于 Event_Broker 内 unordered_map<Event_Identity,...> 的实例化
namespace std
{
    template<>
    struct hash<engine::Event_Identity>
    {
        size_t operator()(const engine::Event_Identity& acl) const noexcept
        {
            size_t seed = 0;
            engine::detail::hash_combine(seed, hash<string>{}(acl.category));
            engine::detail::hash_combine(seed, hash<string>{}(acl.tag));
            return seed;
        }
    };
}

namespace engine
{
    //事件中转器
    class Event_Broker
    {
        //事件订阅权限集合
        std::unordered_map<Event_Identity, std::vector<uint64_t>> acl_set;
        //事件订阅者集合
        std::unordered_map<std::string, uint64_t> mapping_set{};
        //事件转发入口集合
        std::unordered_map<uint64_t, std::function<void(std::shared_ptr<Event> evt)>>
            receivers;

    public:
        //中转站接入
        void attach(const std::string& subscriber,const std::vector<Event>& needed_events,
            std::function<void(std::shared_ptr<Event>)> receiver);
        //事件接收 —— 单事件重载
        void receive(std::shared_ptr<Event> evt);
        //事件接收 —— 多事件重载
        void receive(std::vector<std::shared_ptr<Event>> event_set);
        //事件处理 —— 单事件重载
        std::shared_ptr<Event> process(std::shared_ptr<Event> evt);
        //事件处理 —— 多事件重载
        std::vector<std::shared_ptr<Event>> process(std::vector<std::shared_ptr<Event>> event_set);
    };
}