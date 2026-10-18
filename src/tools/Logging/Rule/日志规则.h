#pragma once
//预编译头
#include "Engine/EngineCore/common/前置头文件包含.h"
//获取路径操作工具（路径键规范化）
#include "Engine/EngineCore/src/tools/Detail/package/路径操作工具.h"

namespace engine
{
    /*
    日志规则（日志根目录作用域 ＋ 文件/目录/级别三张输出许可表）

    根目录即路径规范化的锚点：设置后以该根为界剥离调用处绝对路径，键＝根相对路径；
    未设置则禁止绑定输出流；空串表示整个项目（不做剥离、不限制作用域）。
    允许设置的根目录只有一个（项目根）；再次设置即覆盖。
    */
    class Log_Rules
    {
    public:
        //日志根目录设置（调用处路径作用域；空串表示整个项目）
        bool root_set(const std::string& dir)
        {
            //持规则锁改写根目录键
            std::lock_guard<std::mutex> lock(rule_mtx);
            //基础规范化目录键（剥前导分隔符并补尾 '/'；空串保持空串）
            root_key = detail::path_key_normalize(dir, std::string{});
            //单根设置无其他失败路径，恒返回成功
            return true;
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
            //有值即已设置（含空串＝整个项目）
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
        //路径键生成（返回是否位于根目录作用域内）
        bool key_make(const std::string& raw, std::string& out) const
        {
            //持规则锁生成
            std::lock_guard<std::mutex> lock(rule_mtx);
            //作用域判定结果
            bool in_scope = true;
            //交由持锁版本生成键并回传作用域结果
            out = key_make_locked(raw, in_scope);
            return in_scope;
        }
        //文件输出许可设置（同键同值＝重复设置返回 false，否则返回 true）
        bool file_allowed_set(const std::string& file, bool allowed)
        {
            //持规则锁登记
            std::lock_guard<std::mutex> lock(rule_mtx);
            //生成文件路径键（作用域结果在此无关紧要）
            bool in_scope = true;
            const std::string key = key_make_locked(file, in_scope);
            //查询既有许可
            const auto it = file_rules.find(key);
            //同键同值即重复设置，不作改动
            if (it != file_rules.end() && it->second == allowed)
                return false;
            //登记或覆盖文件许可
            file_rules[key] = allowed;
            return true;
        }
        //目录输出许可设置（作用于其后代调用处）
        bool dir_allowed_set(const std::string& dir, bool allowed)
        {
            //持规则锁登记
            std::lock_guard<std::mutex> lock(rule_mtx);
            //生成目录路径键
            bool in_scope = true;
            const std::string key = key_make_locked(dir, in_scope);
            //查询既有许可
            const auto it = dir_rules.find(key);
            //同键同值即重复设置，不作改动
            if (it != dir_rules.end() && it->second == allowed)
                return false;
            //登记或覆盖目录许可
            dir_rules[key] = allowed;
            return true;
        }
        //级别输出许可设置（级别名不做路径规范化）
        bool level_allowed_set(const std::string& level, bool allowed)
        {
            //持规则锁登记
            std::lock_guard<std::mutex> lock(rule_mtx);
            //查询既有许可
            const auto it = level_rules.find(level);
            //同键同值即重复设置，不作改动
            if (it != level_rules.end() && it->second == allowed)
                return false;
            //登记或覆盖级别许可
            level_rules[level] = allowed;
            return true;
        }
        //输出许可表清空（保留根目录设置）
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
        //输出放行判定（文件精确 → 目录最长前缀 → 级别 → 默认允许；越界已由 key_make 处理）
        bool output_pass(const std::string& key, const std::string& level) const
        {
            //持规则锁判定
            std::lock_guard<std::mutex> lock(rule_mtx);
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
        //路径键生成（持锁调用版：根非空则以根为锚点剥离，命中即域内；否则只做基础规范化且恒域内）
        std::string key_make_locked(const std::string& raw, bool& in_scope) const
        {
            //基础规范化文本（不剥离锚点）
            const std::string basic = detail::path_key_normalize(raw, std::string{});
            //若根目录未设置或为空串（整个项目）
            if (!root_key.has_value() || root_key->empty())
            {
                //视为域内且以基础规范化文本为键
                in_scope = true;
                return basic;
            }
            //若规范化文本中不含项目根则该调用处越界
            if (basic.rfind(*root_key) == std::string::npos)
            {
                //标记越界（键保留基础规范化文本，供上游截断）
                in_scope = false;
                return basic;
            }
            //以项目根为锚点剥离，得到根相对路径键
            in_scope = true;
            return detail::path_key_normalize(raw, *root_key);
        }

        //规则互斥锁（保护根目录键与三张许可表）
        mutable std::mutex rule_mtx{};
        //日志根目录键（无值＝未设置；空值＝整个项目；非空＝锚点兼作用域）
        std::optional<std::string> root_key{};
        //文件输出许可表
        std::unordered_map<std::string, bool> file_rules{};
        //目录输出许可表
        std::unordered_map<std::string, bool> dir_rules{};
        //级别输出许可表
        std::unordered_map<std::string, bool> level_rules{};
    };
}
