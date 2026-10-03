#include "../局部命名空间使用.h"
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
        if (detail::field_check<json>(route_file,"dirty_mark"))
            mark = route_file["dirty_mark"];
        //若路由条目数组有效则取出
        if(detail::field_check<json>(route_file, "config_routes"))
            entries = route_file["config_routes"];
        //若无效则授予空值
        else
            entries = json();
    }

    //路由文件写回（刷新顶层脏标记与条目内脏标记）
    bool Config_Loader::route_file_write(const path& file_path, uint64_t mark,
        const json& entries) const
    {
        //组装路由文件内容
        json route_file;
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
}