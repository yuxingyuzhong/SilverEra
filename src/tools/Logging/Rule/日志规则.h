#pragma once
//预编译头
#include "Engine/EngineCore/common/前置头文件包含.h"
//获取路径操作工具（路径键规范化）
#include "Engine/EngineCore/src/tools/Detail/package/路径操作工具.h"

namespace engine
{
    //日志规则（日志根目录作用域 ＋ 级别/文件/目录三张输出许可表）
    class Log_Rules
    {
    public:
        //日志根目录设置（调用处路径作用域；空串表示整个项目）
        void root_set(const std::string& dir)
        {
            //持规则锁改写根目录键
            std::lock_guard<std::mutex> lock(rule_mtx);
            //规范化目录键（恒以 '/' 结尾）
            root_key = key_normalize(dir);
        }
        //日志根目录清除（回到未设置状态）
        void root_clear()
        {
            //持规则锁清除根目录键
            std::lock_guard<std::mutex> lock(rule_mtx);
            //置为无值表示未设置
            root_key.reset();
        }
        //日志根目录是否已设置
        bool root_is_set() const
        {
            //持规则锁读取
            std::lock_guard<std::mutex> lock(rule_mtx);
            //有值即已设置
            return root_key.has_value();
        }
        //日志根目录查询（未设置返回空串）
        std::string root_get() const
        {
            //持规则锁读取
            std::lock_guard<std::mutex> lock(rule_mtx);
            //无值返回空串
            return root_key.value_or(std::string{});
        }
        //文件输出许可设置（file → 是否允许输出）
        void file_allowed_set(const std::string& file, bool allowed)
        {
            //持规则锁登记
            std::lock_guard<std::mutex> lock(rule_mtx);
            //登记文件键与许可
            file_rules[key_normalize(file)] = allowed;
        }
        //目录输出许可设置（dir → 是否允许输出；作用于其后代调用处）
        void dir_allowed_set(const std::string& dir, bool allowed)
        {
            //持规则锁登记
            std::lock_guard<std::mutex> lock(rule_mtx);
            //登记目录键与许可
            dir_rules[key_normalize(dir)] = allowed;
        }
        //级别输出许可设置（level → 是否允许输出）
        void level_allowed_set(const std::string& level, bool allowed)
        {
            //持规则锁登记
            std::lock_guard<std::mutex> lock(rule_mtx);
            //登记级别与许可
            level_rules[level] = allowed;
        }
        //输出许可表清空
        void clear()
        {
            //持规则锁清空三张许可表
            std::lock_guard<std::mutex> lock(rule_mtx);
            //清空文件许可表
            file_rules.clear();
            //清空目录许可表
            dir_rules.clear();
            //清空级别许可表
            level_rules.clear();
        }
        //输出放行判定（作用域越界 ＋ 文件/目录/级别许可，一体判定）
        bool output_pass(const std::string& key, const std::string& level) const
        {
            //持规则锁判定
            std::lock_guard<std::mutex> lock(rule_mtx);
            //若根目录已设置且非整个项目
            if (root_key.has_value() && !root_key->empty())
            {
                //调用处键不以根目录键为前缀即越界
                if (key.size() < root_key->size() ||
                    key.compare(0, root_key->size(), *root_key) != 0)
                    return false;
            }
            //文件精确匹配
            const auto file_it = file_rules.find(key);
            //文件命中则以文件许可为准
            if (file_it != file_rules.end())
                return file_it->second;
            //目录最长前缀匹配
            bool dir_hit = false;
            bool dir_allowed = true;
            size_t dir_length = 0;
            for (const auto& rule : dir_rules)
            {
                //跳过不构成前缀或不够长的目录键
                if (rule.first.size() <= dir_length ||
                    key.size() < rule.first.size() ||
                    key.compare(0, rule.first.size(), rule.first) != 0)
                    continue;
                //记录最长前缀命中
                dir_length = rule.first.size();
                dir_allowed = rule.second;
                dir_hit = true;
            }
            //目录命中则以目录许可为准
            if (dir_hit)
                return dir_allowed;
            //级别匹配
            const auto level_it = level_rules.find(level);
            //级别命中则以级别许可为准
            if (level_it != level_rules.end())
                return level_it->second;
            //全未命中，默认允许输出
            return true;
        }
    private:
        //路径键规范化（复用通用路径操作工具，剥离到项目根相对路径并补尾部分隔符）
        static std::string key_normalize(const std::string& raw)
        {
            //项目根目录名锚点
            static const std::string anchor = "白银纪元/";
            //借助通用路径操作工具完成规范化
            return detail::path_key_normalize(raw, anchor);
        }

        //规则互斥锁（保护根目录键与三张许可表）
        mutable std::mutex rule_mtx{};
        //日志根目录键（无值＝未设置；空值＝整个项目）
        std::optional<std::string> root_key{};
        //文件输出许可表
        std::unordered_map<std::string, bool> file_rules{};
        //目录输出许可表
        std::unordered_map<std::string, bool> dir_rules{};
        //级别输出许可表
        std::unordered_map<std::string, bool> level_rules{};
    };
}