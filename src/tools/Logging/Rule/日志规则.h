#pragma once
//预编译头
#include "Engine/EngineCore/common/前置头文件包含.h"
//获取路径操作工具（路径键规范化）
#include "Engine/EngineCore/src/tools/Detail/package/路径操作工具.h"

namespace engine
{
    //规则优先级模式（决定「日志级别」与「日志归属」的判定先后）
    enum class Rule_Priority
    {
        //归属优先（默认）：文件/目录先判，命中即终裁；级别仅可否决
        Scope_First,
        //级别优先：级别先判，命中即终裁；归属仅可否决
        Level_First
    };

    /*
    日志规则（日志根目录作用域 ＋ 文件/目录/级别三张输出许可表）

    根目录即路径规范化的锚点：设置后以该根为界剥离调用处绝对路径，键＝根相对路径；
    未设置则禁止绑定输出流；空串表示整个项目（不做剥离、不限制作用域）。
    允许设置的根目录只有一个（项目根）；再次设置即覆盖。

    优先级模式决定「级别」与「归属（文件/目录）」的判定先后：
        高优先级一级命中规则即终裁（允许或否决都由其决定）；
        低优先级一级仅在命中且为否决时才生效，其允许不放行。
    归属内部恒为「文件精确 → 目录最长前缀」。

    全局取反开关对上述判定的**最终结果**取反（原本放行的丢弃、原本否决的放行）；
    它不作用于作用域越界截断（域外日志仍先被丢弃）。
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
        //优先级模式设置（同模式＝重复设置返回 false，否则返回 true）
        bool priority_set(Rule_Priority mode)
        {
            //持规则锁改写
            std::lock_guard<std::mutex> lock(rule_mtx);
            //同模式即重复设置，不作改动
            if (priority == mode)
                return false;
            //改写优先级模式
            priority = mode;
            return true;
        }
        //优先级模式查询
        Rule_Priority priority_get() const
        {
            //持规则锁读取
            std::lock_guard<std::mutex> lock(rule_mtx);
            //返回当前模式
            return priority;
        }
        //全局取反开关设置（同值＝重复设置返回 false，否则返回 true）
        bool verdict_invert_set(bool invert)
        {
            //持规则锁改写
            std::lock_guard<std::mutex> lock(rule_mtx);
            //同值即重复设置，不作改动
            if (verdict_invert == invert)
                return false;
            //改写全局取反开关
            verdict_invert = invert;
            return true;
        }
        //全局取反开关查询
        bool verdict_invert_get() const
        {
            //持规则锁读取
            std::lock_guard<std::mutex> lock(rule_mtx);
            //返回当前开关
            return verdict_invert;
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
        //输出许可表清空（保留根目录设置；优先级模式复位为默认的归属优先）
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
            //优先级模式复位为默认值
            priority = Rule_Priority::Scope_First;
            //全局取反开关复位为默认值
            verdict_invert = false;
        }
        //输出放行判定（按优先级模式得出基础结论，再按全局开关取反；越界已由 key_make 处理）
        bool output_pass(const std::string& key, const std::string& level) const
        {
            //持规则锁判定
            std::lock_guard<std::mutex> lock(rule_mtx);
            //基础结论（未取反）
            bool pass = true;
            //若为级别优先模式
            if (priority == Rule_Priority::Level_First)
            {
                //级别为高优先：命中即终裁
                const auto level_it = level_rules.find(level);
                if (level_it != level_rules.end())
                    pass = level_it->second;
                else
                {
                    //归属为低优先：仅可否决（命中允许不放行）
                    bool scope_hit = false;
                    bool scope_allowed = true;
                    scope_eval_locked(key, scope_hit, scope_allowed);
                    pass = !scope_hit || scope_allowed;
                }
            }
            //归属优先模式：归属为高优先
            else
            {
                //归属命中即终裁
                bool scope_hit = false;
                bool scope_allowed = true;
                scope_eval_locked(key, scope_hit, scope_allowed);
                if (scope_hit)
                    pass = scope_allowed;
                else
                {
                    //级别为低优先：仅可否决（命中允许不放行）
                    const auto level_it = level_rules.find(level);
                    pass = level_it == level_rules.end() || level_it->second;
                }
            }
            //按全局开关对最终结论取反（不影响作用域截断）
            return verdict_invert ? !pass : pass;
        }
    private:
        //归属许可求值（持锁调用版：文件精确 → 目录最长前缀；hit 表示是否命中，allowed 表示命中结论）
        void scope_eval_locked(const std::string& key, bool& hit, bool& allowed) const
        {
            //默认未命中
            hit = false;
            //默认允许
            allowed = true;
            //文件精确匹配优先
            const auto file_it = file_rules.find(key);
            //文件命中即以文件许可为结论
            if (file_it != file_rules.end())
            {
                hit = true;
                allowed = file_it->second;
                return;
            }
            //目录最长前缀匹配
            size_t dir_length = 0;
            for (const auto& rule : dir_rules)
            {
                //跳过不构成前缀或不够长的目录键
                if (rule.first.size() <= dir_length ||
                    key.size() < rule.first.size() ||
                    key.compare(0, rule.first.size(), rule.first) != 0)
                    continue;
                //记录最长前缀命中与结论
                dir_length = rule.first.size();
                allowed = rule.second;
                hit = true;
            }
        }
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
        //规则优先级模式（默认归属优先）
        Rule_Priority priority = Rule_Priority::Scope_First;
        //全局取反开关（默认关闭＝按判定原义）
        bool verdict_invert = false;
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
