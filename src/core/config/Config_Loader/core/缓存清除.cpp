#include "../局部命名空间使用.h"
#include "Engine/EngineCore/src/tools/Logging/日志系统运行包.h"

//引擎命名空间
namespace engine
{
    //可信根目录缓存清除（按目录前缀清除该根下的对象缓存）
    void Config_Loader::root_cache_clear(const Config_Content& root)
    {
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
}