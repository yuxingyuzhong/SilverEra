#pragma once
//预编译头
#include "Engine/EngineFoundation/common/前置头文件包含.h"
//获取 JSON 解析外部库
#include <nlohmann/json.hpp>
//获取数据校验工具
#include "Engine/EngineFoundation/src/detail/methods/package/数据校验工具.h"
//获取自有几何类型
#include "src/collision/Common/几何类型.h"

namespace engine
{
	//碰撞配置字段读取（统一基于 EngineFoundation 的字段校验工具）
	namespace collision_detail
	{
		//字符串字段读取
		inline bool text_read(const nlohmann::json& config, const std::string& field,
			std::string& receiver)
		{
			//字段存在性、类型与非空检查
			if (!detail::field_check<std::string>(config, field))
				return false;
			//读取字段内容
			receiver = config[field].get<std::string>();
			return true;
		}

		//无符号整数字段读取
		inline bool number_read(const nlohmann::json& config, const std::string& field,
			uint64_t& receiver)
		{
			//字段存在性与整数类型检查
			if (!detail::field_check<uint64_t>(config, field))
				return false;
			//读取字段内容
			receiver = config[field].get<uint64_t>();
			return true;
		}

		//浮点字段读取（兼容整数写法）
		inline bool scalar_read(const nlohmann::json& config, const std::string& field,
			double& receiver)
		{
			//优先按浮点读取
			if (detail::field_check<double>(config, field))
			{
				receiver = config[field].get<double>();
				return true;
			}

			//兼容整数字段
			if (detail::field_check<int64_t>(config, field))
			{
				receiver = static_cast<double>(config[field].get<int64_t>());
				return true;
			}

			return false;
		}

		//三元数组字段读取
		inline bool vector_read(const nlohmann::json& config, const std::string& field,
			Vector3& receiver)
		{
			//字段存在性与数组内容检查
			if (!detail::field_check<std::vector<double>>(config, field))
				return false;

			//读取数组内容
			std::vector<double> values = config[field].get<std::vector<double>>();
			//数组长度检查
			if (values.size() != 3)
				return false;

			//写入矢量
			receiver.setValue(values[0], values[1], values[2]);
			return true;
		}

		//四元数组字段读取
		inline bool quaternion_read(const nlohmann::json& config, const std::string& field,
			Quaternion& receiver)
		{
			//字段存在性与数组内容检查
			if (!detail::field_check<std::vector<double>>(config, field))
				return false;

			//读取数组内容
			std::vector<double> values = config[field].get<std::vector<double>>();
			//数组长度检查
			if (values.size() != 4)
				return false;

			//写入四元数（按 x、y、z、w 顺序解释）
			receiver.setValue(values[0], values[1], values[2], values[3]);
			return true;
		}
	}
}
