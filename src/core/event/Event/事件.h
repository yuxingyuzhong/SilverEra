#pragma once
//预编译头
#include "common/前置头文件包含.h"

//游戏引擎命名空间
namespace engine
{
    //事件
    struct event
    {
        //默认构造函数
        event()
        {

        }
        //含参构造函数 —— 构造事件标签
        event(const std::string& category, const std::string& tag)
        {
            this->category = category;
            this->tag = tag;
        }

        //含参构造函数 —— 构造对象标签
        event(const std::string& sender_object, const std::string& target_object,
            const std::string& category, const std::string& tag)
        {
            this->sender_object = sender_object;
            this->target_object = target_object;
            this->category = category;
            this->tag = tag;
        }

        //含参构造函数 —— 全量构造
        event(const std::string& sender_object,const std::string& target_object,
            const std::string& category,const std::string& tag,
            const nlohmann::json& config)
        {
            this->sender_object = sender_object;
            this->target_object = target_object;
            this->category = category;
            this->tag = tag;
            this->config = config;
        }

        //默认析构函数
        ~event()
        {

        }

        //事件发起者
        std::string sender_object{};
        //事件目标
        std::string target_object{};
        //事件大类
        std::string category;
        //类内标签
        std::string tag;
        //配置包
        nlohmann::json config;

        //使用默认等于运算符
        bool operator==(const event& other) const
        {
            if (this->category == other.category &&
                this->tag == other.tag &&
                this->target_object == other.target_object &&
                this->config == other.config)
                return true;
            else
                return false;
        }
    };

    // 哈希组合工具
    namespace detail
    {
        inline void hash_combine(size_t& seed, size_t val) noexcept {
            seed ^= val + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }
    }
}

namespace std
{
    template<>
    struct hash<engine::event>
    {
        size_t operator()(const engine::event& evt) const noexcept
        {
            std::hash<std::string> str_hasher;
            size_t seed = 0;

            // 1. 哈希基类成员（与 event 一致）
            seed = str_hasher(evt.category);
            engine::detail::hash_combine(seed, str_hasher(evt.tag));
            engine::detail::hash_combine(seed, str_hasher(evt.target_object));

            // 2. 哈希派生类成员 config（将 json 转为字符串再哈希）
            //    注意：dump() 可能抛出异常，但 noexcept 标记要求不抛，这里假设不会。
            //    若担心，可以捕获异常并返回一个默认值（但会破坏一致性）。
            std::string config_str = evt.config.dump();
            engine::detail::hash_combine(seed, str_hasher(config_str));

            return seed;
        }
    };

}
