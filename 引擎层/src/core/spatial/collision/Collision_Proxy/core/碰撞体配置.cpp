#include "../局部命名空间使用.h"
#include "src/tools/Logging/日志系统.h"

namespace engine
{
	//文件内部辅助设施（不对外暴露）
	namespace
	{
		//无符号整数字段读取
		bool number_read(const json& config, const string& field, uint64_t& receiver)
		{
			//字段存在性与类型检查
			if (!config.is_object() || !config.contains(field) ||
				!config[field].is_number_integer())
				return false;
			//读取字段内容
			receiver = config[field].get<uint64_t>();
			return true;
		}
	}

	//碰撞体设置分发
	bool Collision_Proxy::collider_set_dispatch(const uint64_t collider_ID,
		const std::function<bool(Collision_Region&)>& setter)
	{
		//取该编号的全部登记项
		auto range = collider_mapping.equal_range(collider_ID);
		//若该编号未被任何空间持有
		if (range.first == range.second)
		{
			Log::warn("Collision_Proxy::待设置碰撞体({})不存在", collider_ID);
			return false;
		}

		//是否存在设置成功的空间
		bool any_set = false;
		//逐个对持有该编号的空间施加设置
		for (auto it = range.first; it != range.second; ++it)
		{
			//查找目标碰撞空间
			Collision_Region* target = region_seek(it->second);
			//若目标碰撞空间不存在则跳过
			if (!target)
				continue;
			//施加设置
			if (setter(*target))
				any_set = true;
		}

		return any_set;
	}

	//碰撞体配置应用
	void Collision_Proxy::collider_config_apply(const string& region, uint64_t collider_ID,
		const json& config)
	{
		//查找目标碰撞空间
		Collision_Region* target = region_seek(region);
		//若目标碰撞空间不存在
		if (!target)
		{
			Log::warn("Collision_Proxy::待设置碰撞体的碰撞空间({})不存在", region);
			return;
		}

		//若配置中带有几何体
		if (config.contains("geometry"))
		{
			//几何配置（补齐碰撞体编号）
			json geometry;

			//对象形式：几何对象本体即为单个几何体（旧约定：位置与旋转写在几何对象内）
			if (config["geometry"].is_object())
			{
				geometry = config["geometry"];
				//写入目标碰撞体编号
				geometry["collider_ID"] = collider_ID;
			}
			//数组形式：作为几何体集合收容（基准位置与基准旋转写在配置顶层）
			else if (config["geometry"].is_array())
			{
				//写入目标碰撞体编号
				geometry["collider_ID"] = collider_ID;
				//收容几何体集合
				geometry["geometries"] = config["geometry"];
				//合并顶层基准位置与基准旋转
				if (config.contains("position"))
					geometry["position"] = config["position"];
				if (config.contains("rotation"))
					geometry["rotation"] = config["rotation"];
			}
			//其余形式视为未提供有效几何体
			else
				geometry = json{};

			//有效几何配置才下发
			if (geometry.is_object() && !geometry.empty())
			{
				//设置几何体
				if (!target->collider_set(geometry))
					Log::warn("Collision_Proxy::碰撞体({})几何体设置失败", collider_ID);
			}
		}

		//若配置中带有检测方式
		if (config.contains("detection_mode"))
		{
			//检测方式
			Detection_Mode detection_mode;
			//检测方式字段检查
			if (config["detection_mode"].is_object() &&
				config["detection_mode"].contains("is_swept_volume") &&
				config["detection_mode"]["is_swept_volume"].is_boolean())
			{
				//写入扫掠检测标记
				detection_mode.is_swept_volume =
					config["detection_mode"]["is_swept_volume"].get<bool>();
				//写入离散检测步长
				if (config["detection_mode"].contains("step_length") &&
					config["detection_mode"]["step_length"].is_number_integer())
					detection_mode.step_length =
						config["detection_mode"]["step_length"].get<uint32_t>();
				//设置检测方式
				collider_set(collider_ID, detection_mode);
			}
			else
				Log::warn("Collision_Proxy::碰撞体({})字段(detection_mode)非法", collider_ID);
		}

		//若配置中带有豁免标记
		if (config.contains("exemption_flag"))
		{
			//豁免标记
			uint64_t exemption_flag = 0;
			//读取豁免标记
			if (number_read(config, "exemption_flag", exemption_flag))
				collider_set(collider_ID, exemption_flag);
			else
				Log::warn("Collision_Proxy::碰撞体({})字段(exemption_flag)非法", collider_ID);
		}
	}
}
