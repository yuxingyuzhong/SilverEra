#include "../局部命名空间使用.h"
#include "src/tools/Logging/日志系统运行包.h"

//引擎命名空间
namespace engine
{
    //配置加载 —— 对象配置加载
    void Config_Loader::config_load(const std::string& object, const vector<string>& paths)
    {
        //查找目标对象的配置文件缓存
        auto file_caches = config_files.find(object);
        //若目标对象尚无配置记录
        if (file_caches == config_files.end())
        {
            logger.error("Config_Loader::目标对象尚无配置记录: {}", object);
            return;
        }

        //若指定加载文件集非空
        if(!paths.empty())
        {
            //逐份处理路由文件（仅处理指定对象的指定文件）
            for (const auto& path : paths)
            {
                //查找目标文件是否缓存
                auto file_cache = file_caches->second.find(path);
                //若目标文件已缓存
                if (file_cache != file_caches->second.end())
                    config_file_process(path, file_cache->second, object);
                //若目标文件未缓存
                else
                    logger.warn("Config_Loader::目标配置文件未缓存\n请更新配置路由索引再次尝试");
            }
        }
        //若指定加载文件集为空
        else
        {
            //全量加载缓存配置数据
            for (auto& file:file_caches->second)
                config_file_process(file.first,file.second, object);
        }
    }

    //配置路由加载（事件指定配置目录）
    void Config_Loader::route_load(const Config_Content& content,bool delivery,
        const std::string& object)
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

        //逐份处理路由文件
        for (const auto& route_file : route_files)
        {
            //路由文件读取路径转换
            const path file_path = detail::string_to_path(route_file);
            //顶层脏标记与路由条目
            json mark = json();
            json entries = json();
            //读取路由文件（查阅路由文件头）
            route_file_read(file_path, mark, entries);

            //若路由条目格式非法
            if (!entries.is_array())
            {
                logger.error("Config_Loader::当前路由文件内容格式非法");
                continue;
            }
            //计算该路由文件本轮标记
            const uint64_t want_mark = mark_make(detail::path_to_string(file_path));
            //若路由文件头命中则无需重新加载
            if (mark.is_number_unsigned() && mark.get<uint64_t>() == want_mark)
            {
                logger.warn("Config_Loader::事件指定但无需重新加载的路由文件，已跳过: {}",
                    detail::path_to_string(file_path));
                continue;
            }

            //重新解析该路由文件并更新对象配置文件路径缓存
            route_file_process(content, file_path, object, {}, delivery);
        }
    }
}