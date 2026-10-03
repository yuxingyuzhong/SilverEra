#pragma once
//预编译头
#include "Engine/EngineCore/common/前置头文件包含.h"
//获取路径字符串转换方法
#include "路径字符串转换.h"
//获取日志系统
#include "Engine/EngineCore/src/tools/Logging/日志系统运行包.h"

//通用算法模块
namespace engine
{
	//辅助工具命名空间
	namespace detail
	{
        //路径有效性检查 —— path重载
        inline bool path_check(const std::filesystem::path& target_path)
        {
            //若未解析出有效路径
            if (target_path.begin() == target_path.end())
            {
                logger.info("路径无效");
                return false;
            }

            //异常信息记录
            std::error_code ec;
            //若访问路径不存在或发生系统错误或非可读取文件
            if (!std::filesystem::exists(target_path, ec) || 
                !std::filesystem::is_regular_file(target_path, ec))
            {
                logger.info("访问路径异常\n请自行排查访问路径是否存在及是否可读取");
                //输出异常信息
                logger.info("{}", ec);
                //清空异常信息
                ec.clear();
                //返回检查未通过
                return false;
            }

            //若所有检查均通过
            return true;
        }
        //路径有效性检查 —— string重载
        inline bool path_check(const std::string& target_path)
        {
            //转化为标准路径
            std::filesystem::path suspect_path = string_to_path(target_path);
            //调用path重载
            return path_check(suspect_path);
        }
    }
}
