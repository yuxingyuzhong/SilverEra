#pragma once
//预编译头
#include "common/前置头文件包含.h"
//获取日志系统
#include "src/tools/Logging/日志系统运行包.h"

namespace engine
{
    //辅助工具命名空间
    namespace detail
    {
        //字段有效性检查
        template<typename T>
        inline bool field_check(const nlohmann::json& config, const std::string& field)
        {
            //字段存在性检查
            if (!config.contains(field))
            {
                logger.warn("detail::未包含指定字段: {}", field);
                return false;
            }

            //萃取布尔类型
            if constexpr (std::is_same_v<T, bool>)
            {
                if (!config[field].is_boolean())
                {
                    logger.info("detail::字段 {} 非布尔格式", field);
                    return false;
                }
            }
            //萃取整数类形
            else if constexpr (std::is_integral_v<T>)
            {
                //匹配所有整数类型
                if (!config[field].is_number_integer())
                {
                    logger.info("detail::字段 {} 非整数格式", field);
                    return false;
                }
            }
            //萃取浮点类型
            else if constexpr (std::is_floating_point_v<T>)
            {
                //匹配所有浮点类型
                if (!config[field].is_number_float())
                {
                    logger.info("detail::字段 {} 非浮点数格式", field);
                    return false;
                }
            }
            //萃取字符串类型
            else if constexpr (std::is_same_v<T, std::string>)
            {
                //若字段非字符串
                if (!config[field].is_string())
                {
                    logger.info("detail::字段 {} 类型不匹配", field);
                    return false;
                }
                //若字符串为空
                if (config[field].get_ref<const std::string&>().empty())
                {
                    logger.info("detail::字段 {} 内容为空", field);
                    return false;
                }
            }
            //若为其他类型
            else
            {
                //匹配容器类型
                try
                {
                    config[field].get<T>();
                }
                catch (const nlohmann::json::type_error&)
                {
                    logger.info("detail::字段 {} 类型不匹配", field);
                    return false;
                }

                //非空检查
                if ((config[field].is_array() || config[field].is_object()) &&
                    config[field].empty())
                {
                    logger.info("detail::字段 {} 内容为空", field);
                    return false;
                }
            }

            return true;
        }
    };
}