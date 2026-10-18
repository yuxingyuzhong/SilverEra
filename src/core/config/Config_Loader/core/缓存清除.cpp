#include "src/core/config/Config_Loader/局部命名空间使用.h"
#include "src/tools/Logging/日志系统运行包.h"

//引擎命名空间
namespace engine
{
    //可信根目录缓存清除（按目录前缀清除该根下的对象缓存，并落盘抹除对应条目标记）
    void Config_Loader::root_cache_clear(const Config_Content& root)
    {
        //收集待卸载的「对象 × 目录对」命中集合
        //须在清除登记前收集：登记一旦清除，定位路由文件所需信息即丢失
        vector<std::pair<string, Config_Content>> hits{};
        //遍历对象配置目录记录
        for (const auto& cache : config_routes)
        {
            //遍历该对象已登记的配置目录
            for (const auto& content : cache.second)
            {
                //若路由目录与配置目录均以可信根对应目录为前缀
                if (path_prefix_check(root.route, content.route) &&
                    path_prefix_check(root.config, content.config))
                    //记录该「对象 × 目录对」命中
                    hits.push_back({ cache.first, content });
            }
        }

        //逐份命中目录对落盘抹除该对象的条目标记
        //与 Route/Unload 同径，使同一可信根在进程内卸载后可重新加载重建
        for (const auto& hit : hits)
            route_unload(hit.second, hit.first);

        //清除对象配置目录缓存
        for (auto it = config_routes.begin(); it != config_routes.end();)
        {
            //剔除位于该可信根目录下的配置目录
            std::erase_if(it->second, [&root](const Config_Content& content)
                {
                    //路由目录与配置目录均需以可信根对应目录为前缀
                    return path_prefix_check(root.route, content.route) &&
                        path_prefix_check(root.config, content.config);
                });
            //若该对象已无配置目录记录则删除对象条目
            if (it->second.empty())
                it = config_routes.erase(it);
            else
                ++it;
        }

        //清除对象配置文件缓存
        for (auto it = config_files.begin(); it != config_files.end();)
        {
            //剔除位于该可信根配置目录下的配置文件
            std::erase_if(it->second, [&root](const auto& pair)
                {
                    //配置文件路径需以可信根配置目录为前缀
                    return path_prefix_check(root.config, detail::string_to_path(pair.first));
                });
            //若该对象已无配置文件记录则删除对象条目
            if (it->second.empty())
                it = config_files.erase(it);
            else
                ++it;
        }
    }

    //配置路由卸载（事件指定配置目录）
    void Config_Loader::route_unload(const Config_Content& content, const std::string& object)
    {
        //扫描目录记录
        vector<string> route_files{};
        //扫描路由文件（绝对路径）
        content_scan(detail::path_to_string(content.route), route_files);

        //若未扫描到可用路由文件
        if (route_files.empty())
        {
            logger.error("Config_Loader::未检测到任何路由文件");
            return;
        }

        //逐份卸载路由文件（抹除条目标记与顶层标记，并移除该对象的配置文件缓存）
        for (const auto& route_file : route_files)
        {
            //路由文件读取路径转换
            const path file_path = detail::string_to_path(route_file);
            //执行路由文件卸载（保留配置目录登记，使同目录对可再次加载重建）
            route_file_unload(content, file_path, object, json());
        }
    }

    //配置卸载（事件指定对象与可选配置文件清单）
    void Config_Loader::config_unload(const std::string object, const json& request)
    {
        //查找目标对象的配置文件缓存
        auto file_caches = config_files.find(object);
        //若目标对象尚无配置记录
        if (file_caches == config_files.end())
        {
            logger.error("Config_Loader::目标对象尚无配置记录: {}", object);
            return;
        }

        //事件指定的卸载文件清单（为空则卸载全部）
        json only_files = json();
        //若事件指定卸载文件清单(存在容器非空检查)
        if (detail::field_check<vector<string>>(request, "files"))
            only_files = request["files"];

        //若未指定文件清单则全量卸载
        if (only_files.is_null())
            //清除该对象的全部配置文件缓存
            config_files.erase(file_caches);
        //若指定了文件清单
        else
        {
            //按清单清除指定配置文件缓存
            for (const auto& file : only_files)
            {
                //若清单项为字符串
                if (file.is_string())
                    file_caches->second.erase(file.get<std::string>());
            }
            //若该对象已无配置文件记录则删除对象条目
            if (file_caches->second.empty())
                config_files.erase(file_caches);
        }

        //查找目标对象的配置目录缓存（用于定位路由文件）
        const auto route_caches = config_routes.find(object);
        //若目标对象尚无可定位的配置目录缓存
        if (route_caches == config_routes.end() || route_caches->second.empty())
        {
            logger.warn("Config_Loader::目标对象无配置目录缓存，仅清除内存缓存: {}", object);
            return;
        }

        //逐份配置目录定位路由文件并抹除该对象的条目标记（保留配置目录登记）
        for (const auto& content : route_caches->second)
        {
            //扫描目录记录
            vector<string> route_files{};
            //扫描路由文件（绝对路径）
            content_scan(detail::path_to_string(content.route), route_files);
            //逐份卸载路由文件
            for (const auto& route_file : route_files)
                route_file_unload(content, detail::string_to_path(route_file), object, only_files);
        }
    }
}
