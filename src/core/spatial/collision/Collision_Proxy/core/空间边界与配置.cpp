#include "../局部命名空间使用.h"
#include "src/tools/Data_Validator/数据校验器.h"
#include "src/tools/Logging/日志系统.h"

namespace engine
{
	//文件内部辅助设施（不对外暴露）
	namespace
	{
		//字符串字段读取
		bool text_read(const json& config, const string& field, string& receiver)
		{
			//字段存在性与类型检查
			if (!config.is_object() || !config.contains(field) || !config[field].is_string())
				return false;
			//读取字段内容
			receiver = config[field].get<string>();
			return !receiver.empty();
		}
	}

	//碰撞空间边界设置
	bool Collision_Proxy::region_boundary_set(const string& path)
	{
		//空间边界配置内容
		json boundary_config;
		//读取空间边界配置
		if (!boundary_config_read(path, boundary_config))
			return false;

		//是否全部空间设置成功
		bool all_set = true;
		//逐个空间设置空间边界
		for (const auto& boundary_data : boundary_config)
		{
			//空间名称
			string region;
			//空间边界网格路径
			string mesh_path;
			//读取空间名称
			if (!text_read(boundary_data, "region", region))
			{
				Log::warn("Collision_Proxy::边界配置缺少有效字段(region)");
				all_set = false;
				continue;
			}
			//读取空间边界网格路径
			if (!text_read(boundary_data, "mesh_path", mesh_path))
			{
				Log::warn("Collision_Proxy::边界配置缺少有效字段(mesh_path)");
				all_set = false;
				continue;
			}

			//查找目标碰撞空间
			Collision_Region* target = region_seek(region);
			//若目标碰撞空间不存在
			if (!target)
			{
				Log::warn("Collision_Proxy::待设置边界的碰撞空间({})不存在", region);
				all_set = false;
				continue;
			}

			//构建空间边界
			if (!target->boundary_build(mesh_path))
			{
				Log::warn("Collision_Proxy::碰撞空间({})边界构建失败({})", region, mesh_path);
				all_set = false;
			}
		}

		return all_set;
	}

	//配置内容应用
	void Collision_Proxy::config_apply(const json& config)
	{
		//配置内容格式检查
		if (!config.is_object() || !config.contains("regions") || !config["regions"].is_array())
		{
			Log::warn("Collision_Proxy::配置缺少有效字段(regions)");
			return;
		}

		//逐个应用空间配置
		for (const auto& region_config : config["regions"])
		{
			//空间名称
			string region;
			//读取空间名称
			if (!text_read(region_config, "region", region))
			{
				Log::warn("Collision_Proxy::空间配置缺少有效字段(region)");
				continue;
			}

			//构建碰撞空间
			region_build(region);
			//查找目标碰撞空间
			Collision_Region* target = region_seek(region);
			//若目标碰撞空间不存在则跳过
			if (!target)
				continue;

			//设置空间活跃性
			if (region_config.contains("active") && region_config["active"].is_boolean())
				region_state_set(region, region_config["active"].get<bool>());

			//设置空间边界
			if (region_config.contains("boundary") && region_config["boundary"].is_string())
			{
				//空间边界网格路径
				string mesh_path = region_config["boundary"].get<string>();
				//构建空间边界
				if (!target->boundary_build(mesh_path))
					Log::warn("Collision_Proxy::碰撞空间({})边界构建失败({})", region, mesh_path);
			}

			//若空间内无碰撞体清单则结束本空间
			if (!region_config.contains("colliders") || !region_config["colliders"].is_array())
				continue;

			//逐个构建并配置碰撞体
			for (const auto& collider_config : region_config["colliders"])
			{
				//在目标空间内构建碰撞体
				optional<uint64_t> collider_ID = collider_build(region);
				//若碰撞体构建失败则跳过
				if (!collider_ID)
					continue;
				//应用碰撞体配置
				collider_config_apply(region, *collider_ID, collider_config);
			}
		}
	}

	//边界配置文件读取
	bool Collision_Proxy::boundary_config_read(const string& path, json& receiver) const
	{
		//边界配置文件路径检查
		if (!Data_Validator::path_check(path))
		{
			Log::warn("Collision_Proxy::边界配置路径不可读取({})", path);
			return false;
		}

		//打开边界配置文件
		std::ifstream file(string_to_path(path));
		//若文件打开失败
		if (!file.is_open())
		{
			Log::warn("Collision_Proxy::边界配置文件打开失败({})", path);
			return false;
		}

		try
		{
			//解析边界配置内容
			file >> receiver;
		}
		catch (const std::exception&)
		{
			Log::warn("Collision_Proxy::边界配置内容非法({})", path);
			return false;
		}

		//边界配置须为空间数组
		if (!receiver.is_array())
		{
			Log::warn("Collision_Proxy::边界配置须为空间数组({})", path);
			return false;
		}

		return true;
	}
}
