#include "../局部命名空间使用.h"
#include "Engine/EngineCore/src/tools/Logging/日志系统运行包.h"

//引擎命名空间
namespace engine
{
    //基目录获取
    const path Config_Loader::base_dir_get(void) const
    {
        //返回配置基目录
        return Engine_Env::exe_dir_get();
    }

    //异常信息输出
    bool Config_Loader::error_out(error_code& error_info) const
    {
        //若异常信息不存在
        if (!error_info)
            //返回异常未输出
            return false;
        //若异常信息存在
        else
        {
            //经由日志系统输出异常信息
            logger.error("{}", error_info);
            //清空异常信息
            error_info.clear();
            //返回异常已输出
            return true;
        }
    }

    //路径前缀判定（前缀是否为目标路径的目录前缀）
    bool Config_Loader::path_prefix_check(const path& prefix, const path& target)
    {
        //空前缀不构成有效前缀
        if (prefix.empty())
            return false;

        //逐节比对
        auto prefix_it = prefix.begin();
        auto target_it = target.begin();
        for (; prefix_it != prefix.end(); ++prefix_it, ++target_it)
        {
            //若目标路径短于前缀则不构成前缀
            if (target_it == target.end())
                return false;
            //若当前节不同则不构成前缀
            if (*prefix_it != *target_it)
                return false;
        }

        //所有节均匹配
        return true;
    }

    //文件读取
    void Config_Loader::file_read(const path& file_path, json& receiver) const
    {
        try
        {
            //打开目标文件
            ifstream file(file_path);
            //若文件打开失败
            if (!file.is_open())
            {
                logger.error("Config_Loader::文件打开失败");
                receiver = json();
                return;
            }
            //读取文件内容
            file >> receiver;
        }
        catch (const json::parse_error& e)
        {
            logger.error("Config_Loader::文件格式非法");
            receiver = json();
        }
        catch (const std::exception& e)
        {
            logger.error("Config_Loader::未知错误:");
            logger.error("错误路径如下: {}", detail::path_to_string(file_path));
            logger.error("错误信息如下: {}", e.what());
            receiver = json();
        }
    }

    //目录递归扫描
    void Config_Loader::content_scan(const string& route,vector<string>& receiver) const
    {
        //构建实际扫描目录
        path actual_scan_dir = base_dir_get() / detail::string_to_path(route);
        //异常信息记录
        error_code ec;

        //构建递归查找迭代器
        auto it = recursive_directory_iterator(actual_scan_dir, ec);
        //若迭代器创建出现异常
        if (error_out(ec))
            return;

        //递归查找文件
        for (; it != recursive_directory_iterator(); it.increment(ec))
        {
            //若迭代器创建/递增出现异常
            if (error_out(ec))
                continue;

            //检查文件是否可读取
            if (it->is_regular_file(ec))
                //若可读取则记录文件路径
                receiver.push_back(detail::path_to_string(it->path()));

            //若文件查询过程中发生异常
            if (error_out(ec))
                continue;
        }
    }

    //路径安全检查
    bool Config_Loader::path_safety_check(const Config_Content& content,
        const std::string& config_path) const
    {
        //安全转换中文路径
        path suspect_path = detail::string_to_path(config_path);
        //若待检查路径为空
        if (suspect_path.empty())
        {
            logger.error("Config_Loader::配置路径为空");
            return false;
        }

        //检查路径中是否存在危险跳转符号".."
        for (const auto& part : suspect_path)
        {
            //若检测出跳转符号
            if (part.string() == "..")
            {
                logger.error("Config_Loader::存在非法父目录返回符号");
                return false;
            }
        }

        //逐节比对：标准配置目录必须为待检查路径的前缀
        auto standard = content.config.begin();
        auto suspect = suspect_path.begin();
        for (; standard != content.config.end(); ++standard, ++suspect)
        {
            //若待检查路径短于标准路径则不可能包含标准路径
            if (suspect == suspect_path.end())
            {
                logger.error("Config_Loader::配置路径超出合法范围");
                return false;
            }
            //若当前节不同则路径非法
            if (*standard != *suspect)
            {
                logger.error("Config_Loader::配置路径超出合法范围");
                return false;
            }
        }

        //通用检查配置路径有效性（存在且为可读取的普通文件）
        if (!detail::path_check(base_dir_get() / suspect_path))
            return false;

        //若所有检查均通过
        return true;
    }
}