#include "src/core/config/Config_Loader/局部命名空间使用.h"
#include "src/tools/Logging/日志系统运行包.h"

//引擎命名空间
namespace engine
{
    //路由文件读取（解出顶层脏标记与路由条目数组）
    void Config_Loader::route_file_read(const path& file_path, json& mark, json& entries) const
    {
        //读取路由文件
        json route_file;
        file_read(file_path, route_file);
        //若路由文件格式非法
        if (!route_file.is_object())
        {
            entries = json();
            return;
        }
        //若顶层脏标记有效则取出
        if (detail::field_check<json>(route_file, "dirty_mark"))
            mark = route_file["dirty_mark"];
        //若路由条目数组有效则取出
        if (detail::field_check<json>(route_file, "config_routes"))
            entries = route_file["config_routes"];
        //若无效则授予空值
        else
            entries = json();
    }

    //路由文件写回（写入条目内脏标记；顶层脏标记为空值时不写，以便卸载后抹除文件头标记）
    bool Config_Loader::route_file_write(const path& file_path, const json& mark,
        const json& entries) const
    {
        //组装路由文件内容
        json route_file;
        //若顶层脏标记非空则写入（空值表示本轮不写顶层标记）
        if (!mark.is_null())
            route_file["dirty_mark"] = mark;
        route_file["config_routes"] = entries;
        //打开目标文件
        ofstream file(file_path);
        //若文件打开失败
        if (!file.is_open())
        {
            logger.error("Config_Loader::路由文件写回失败");
            logger.error("失败路径:{}", detail::path_to_string(file_path));
            return false;
        }
        //写入路由文件内容
        file << route_file.dump(4);
        //返回写回成功
        return true;
    }

    //单份路由文件处理（按可选过滤条件处理其中条目；deliver 表示是否投递配置）
    bool Config_Loader::route_file_process(const Config_Content& content, const path& file_path,
        const std::string& only_object, const std::vector<std::string>& only_files,
        bool deliver)
    {
        //顶层脏标记与路由条目
        json mark = json();
        json entries = json();
        //读取路由文件
        route_file_read(file_path, mark, entries);

        //若路由条目格式非法
        if (!entries.is_array())
        {
            logger.error("Config_Loader::当前路由文件内容格式非法");
            return false;
        }

        //本轮是否真正投递过配置
        bool delivered = false;

        //逐条读取路由条目
        for (auto& entry : entries)
        {
            //若目标字段无效
            if(!detail::field_check<string>(entry,"object") ||
               !detail::field_check<string>(entry,"config_path"))
            {
                logger.error("Config_Loader::目标字段无效");
                logger.error("Config_Loader::可能原因如下：");
                logger.error("<1>，目标字段不存在：\"object\" and \"config_path\"");
                logger.error("<2>，目标字段非所需字符串格式");
                logger.error("<3>，目标字段无实际内容");
                continue;
            }

            //取条目事件类别（缺省回落 Config）
            const string event_category = detail::field_check<string>(entry, "event_category")
                ? entry["event_category"].get<string>() : "Config";
            //取条目事件标签（缺省回落 Load）
            const string event_tag = detail::field_check<string>(entry, "event_tag")
                ? entry["event_tag"].get<string>() : "Load";
            //若条目事件标识命中配置加载器预定义事件
            if(event_identifier_reserved(event_category,event_tag))
            {
                logger.error("Config_Loader::条目事件标识命中预定义事件，已驳回该条目");
                continue;
            }

            //存储目标对象名称
            const std::string object = entry["object"];
            //存储配置文件路径
            const std::string config_path = entry["config_path"];

            //若存在对象过滤条件且不匹配则跳过
            if (!only_object.empty() && object != only_object)
                continue;
            //若存在文件过滤条件且不匹配则跳过
            if (!only_files.empty() && std::ranges::find(only_files, config_path) == only_files.end())
                continue;

            //若路径安全检查失败则略过该配置文件
            if (!path_safety_check(content, config_path))
                continue;

            //推导对象自身目录对（路由文件所在目录 + 配置文件所在目录）
            const Config_Content object_content(
                file_path.parent_path().lexically_relative(base_dir_get()),
                detail::string_to_path(config_path).parent_path());
            //记录对象配置目录与配置文件条目（内存缓存的唯一写入点）
            config_routes[object].insert(object_content);
            config_files[object][config_path] = entry;

            //若非投递模式则略过投递
            if (!deliver)
                continue;

            //接收者存在性检查（未接入则取消其配置信息传递）
            if (!receiver_check(object))
            {
                logger.warn("Config_Loader::接收者尚未接入事件中转站，已取消配置信息传递: {}", object);
                continue;
            }

            //处理文件并获取投递标记
            delivered = config_file_process(config_path,entry,object);
            //投递成功后刷新内存缓存副本：缓存里存的是投递前的条目快照，脏标记只写进了
            //循环内的 entry（并随路由文件落盘），不回写会让同一进程内再次加载重复投递
            if (delivered)
                config_files[object][config_path] = entry;
        }

        //仅当本轮真正投递过配置才回写路由文件（仅缓存路由不加脏标记）
        if (delivered)
            route_file_write(file_path, mark_make(detail::path_to_string(file_path)), entries);

        //返回本轮是否投递过配置
        return delivered;
    }

    //单份路由文件卸载（按可选过滤条件抹除条目标记与顶层标记，并移除对应配置文件缓存）
    bool Config_Loader::route_file_unload(const Config_Content& content, const path& file_path,
        const std::string& only_object, const json& only_files)
    {
        //顶层脏标记与路由条目
        json mark = json();
        json entries = json();
        //读取路由文件
        route_file_read(file_path, mark, entries);

        //若路由条目格式非法
        if (!entries.is_array())
        {
            logger.error("Config_Loader::当前路由文件内容格式非法");
            return false;
        }

        //本轮是否真正改动过路由条目
        bool changed = false;

        //逐条读取路由条目
        for (auto& entry : entries)
        {
            //若目标字段无效
            if (!detail::field_check<string>(entry, "object") ||
                !detail::field_check<string>(entry, "config_path"))
            {
                logger.error("Config_Loader::目标字段无效");
                continue;
            }

            //存储目标对象名称
            const std::string object = entry["object"];
            //存储配置文件路径
            const std::string config_path = entry["config_path"];

            //若存在对象过滤条件且不匹配则跳过
            if (!only_object.empty() && object != only_object)
                continue;
            //若存在文件过滤条件
            if (!only_files.is_null())
            {
                //判断当前文件是否落在卸载清单内
                bool in_files = false;
                for (const auto& file : only_files)
                {
                    //若清单项为字符串且与当前文件一致则命中
                    if (file.is_string() && file.get<std::string>() == config_path)
                    {
                        in_files = true;
                        break;
                    }
                }
                //若未命中卸载清单则跳过
                if (!in_files)
                    continue;
            }

            //若配置文件不在本目录对的配置目录下则跳过（卸载不要求文件仍存在）
            if (!path_prefix_check(content.config, detail::string_to_path(config_path).parent_path()))
                continue;

            //移除对象配置文件缓存
            const auto file_caches = config_files.find(object);
            //若目标对象存在配置文件缓存
            if (file_caches != config_files.end())
            {
                file_caches->second.erase(config_path);
                //若该对象已无配置文件记录则删除对象条目
                if (file_caches->second.empty())
                    config_files.erase(file_caches);
            }

            //抹除条目脏标记（使同进程内重新加载不再被条目脏标记短路）
            if (entry.contains("dirty_mark"))
            {
                entry.erase("dirty_mark");
                changed = true;
            }
        }

        //仅当本轮真正改动过才回写路由文件（以空顶层标记抹除文件头脏标记）
        if (changed)
            route_file_write(file_path, json(), entries);

        //返回本轮是否改动过路由条目
        return changed;
    }

}
