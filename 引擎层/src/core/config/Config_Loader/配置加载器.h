#pragma once
//预编译头
#include "common/前置头文件包含.h"
//获取事件系统运行包
#include "src/core/event/事件系统运行包.h"
//获取引擎环境信息
#include "src/tools/Engine_Env/引擎环境.h"
//获取数据校验工具
#include "src/tools/Detail/package/数据校验工具.h"
//获取路径字符串转换工具
#include "src/tools/Detail/package/路径操作工具.h"

//游戏引擎命名空间
namespace engine
{
    //配置目录
    struct Config_Content
    {
        //索引目录
        std::filesystem::path route{};
        //配置目录
        std::filesystem::path config{};

        //默认等于重载
        bool operator==(const Config_Content&) const = default;
        //构造函数
        explicit Config_Content(const std::filesystem::path& route = {},
            const std::filesystem::path& config = {})
        {
            this->route = detail::path_normalize(route);
            this->config = detail::path_normalize(config);
        }
        //构造函数
        explicit Config_Content(const std::string& route = {}, 
            const std::string& config = {})
        {
            this->route = detail::path_normalize(detail::string_to_path(route));
            this->config = detail::path_normalize(detail::string_to_path(config));
        }
    };

}

//哈希特化
namespace std
{
    //配置目录哈希特化
    template<>
    struct hash<engine::Config_Content>
    {
        std::size_t operator()(const engine::Config_Content& c) const noexcept
        {
            std::size_t seed = 0;
            engine::detail::hash_combine(seed, std::hash<std::filesystem::path>{}(c.route));
            engine::detail::hash_combine(seed, std::hash<std::filesystem::path>{}(c.config));
            return seed;
        }
    };

    //配置目录格式化特化（输出「索引[索引目录] 配置[配置目录]」，均为 UTF-8 文本）
    template<>
    struct formatter<engine::Config_Content>
    {
        //忽略格式说明符，不做特殊解析
        constexpr auto parse(format_parse_context& ctx)
        {
            return ctx.begin();
        }
        //输出索引目录与配置目录
        auto format(const engine::Config_Content& content, format_context& ctx) const
        {
            return std::format_to(ctx.out(), "索引[{}] 配置[{}]",
                engine::detail::path_to_string(content.route),
                engine::detail::path_to_string(content.config));
        }
    };

} 

//游戏引擎命名空间
namespace engine
{
    /*
    配置加载器

    成员声明顺序（声明与实现同序）
        公开门面置顶（构造 / 析构 / 接入），其后的私有方法按「被依赖者在依赖者前面」
        与「功能相近者相邻排列」分成八组：
            组1 路径与文件 · 组2 脏标记 · 组3 路由文件读写 · 组4 配置投递
            组5 路由文件处理 · 组6 配置加载 · 组7 缓存清除 · 组8 事件分发
        实现分散在 core/ 下的九个编译单元，每个单元内的函数顺序与本声明顺序一致。
    */
    class Config_Loader
    {
    private:
        //配置可信加载根目录集合
        std::unordered_set<Config_Content> believed_roots;
        //对象配置目录记录（对象名 → 配置目录；供对象级加载与二次加载做路径安全检查）
        std::unordered_map<std::string, std::unordered_set<Config_Content>> config_routes;
        //对象配置文件路径缓存（对象名 → 配置文件路径集合）
        std::unordered_map<std::string, std::unordered_map<std::string, nlohmann::json>> config_files;
    public:
        //事件终端
        Event_Terminal event_terminal;
    private:
        //权限密钥
        int64_t acl_key = 0;
        //配置待发送对象
        std::unordered_set<std::string> waited_objects{};

    public:
        //构造函数
        Config_Loader();
        //析构函数
        ~Config_Loader() = default;

        //事件终端接入
        void attach(void);

    private:
        // ---------- 组1 路径与文件 ----------

        //基目录获取
        const std::filesystem::path base_dir_get(void) const;

        //异常信息输出
        bool error_out(std::error_code& info) const;

        //路径前缀判定（前缀是否为目标路径的目录前缀）
        static bool path_prefix_check(const std::filesystem::path& prefix,
            const std::filesystem::path& target);

        //文件读取
        void file_read(const std::filesystem::path& file_path, nlohmann::json& receiver) const;

        //目录递归扫描
        void content_scan(const std::string& route,std::vector<std::string>& receiver) const;

        //路径安全检查
        bool path_safety_check(const Config_Content& content,const std::string& config_path) const;

        // ---------- 组2 脏标记 ----------

        //运行盐获取（进程内唯一且只生成一次；跨运行随机，使上一轮遗留脏标记自动失效）
        static uint64_t run_salt_get(void);
        //脏标记计算 —— 路由文件自身
        static uint64_t mark_make(const std::string& file_key);
        //脏标记计算 —— 对象×文件
        static uint64_t mark_make(const std::string& object, const std::string& config_path);

        // ---------- 组3 路由文件读写 ----------

        //路由文件读取（解出顶层脏标记与路由条目数组）
        void route_file_read(const std::filesystem::path& file_path, nlohmann::json& mark,
            nlohmann::json& entries) const;
        //路由文件写回（刷新顶层脏标记与条目内脏标记）
        bool route_file_write(const std::filesystem::path& file_path, uint64_t mark,
            const nlohmann::json& entries) const;

        // ---------- 组4 配置投递 ----------

        //接收者存在性检查（向中转站询问目标对象是否接入）
        bool receiver_check(const std::string& object);
        //对象配置投递（读配置 → 发 Config/Load → 写条目标记）
        bool object_config_deliver(const std::string& object, const std::string& config_path,
            uint64_t mark, nlohmann::json& entry);
        //单份配置文件处理
        bool config_file_process(const std::string& config_path, nlohmann::json& entry,
            const std::string& object);

        // ---------- 组5 路由文件处理 ----------

        //单份路由文件处理（按可选过滤条件处理其中条目；deliver 表示是否投递配置）
        bool route_file_process(const Config_Content& content, const std::filesystem::path& file_path,
            const std::string& only_object, const std::vector<std::string>& only_files,
            bool deliver = true);

        // ---------- 组6 配置加载 ----------

        //配置加载 —— 对象配置加载
        void config_load(const std::string& object, const std::vector<std::string>& paths = {});
        //配置路由加载（事件指定配置目录）
        void route_load(const Config_Content& content,bool delivery = true,
            const std::string& object = {});

        // ---------- 组7 缓存清除 ----------

        //可信根目录缓存清除（按目录前缀清除该根下的对象缓存）
        void root_cache_clear(const Config_Content& root);

        // ---------- 组8 事件分发 ----------

        //事件处理
        void event_process(std::shared_ptr<Event> evt);
    };
}