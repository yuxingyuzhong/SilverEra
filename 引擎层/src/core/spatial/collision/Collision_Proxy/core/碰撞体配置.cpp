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

		//三元数组字段读取
		bool vector_read(const json& config, const string& field, Vector3& receiver)
		{
			//字段存在性检查
			if (!config.is_object() || !config.contains(field))
				return false;

			try
			{
				//读取数组内容
				vector<double> values = config[field].get<vector<double>>();
				//数组长度检查
				if (values.size() != 3)
					return false;
				//写入矢量
				receiver.setValue(values[0], values[1], values[2]);
			}
			catch (const std::exception&)
			{
				//数组内容非法
				return false;
			}

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
		if (config.contains("geometry") && config["geometry"].is_object())
		{
			//几何配置（补齐碰撞体编号）
			json geometry = config["geometry"];
			//写入目标碰撞体编号
			geometry["collider_ID"] = collider_ID;
			//设置几何体
			if (!target->collider_set(geometry))
				Log::warn("Collision_Proxy::碰撞体({})几何体设置失败", collider_ID);
		}

		//若配置中带有位移向量
		if (config.contains("displacement"))
		{
			//位移向量
			Vector3 displacement;
			//读取位移向量
			if (vector_read(config, "displacement", displacement))
				collider_set(collider_ID, displacement);
			else
				Log::warn("Collision_Proxy::碰撞体({})字段(displacement)非法", collider_ID);
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
