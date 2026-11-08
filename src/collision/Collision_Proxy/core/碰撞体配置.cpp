#include "../局部命名空间使用.h"
#include "Engine/EngineFoundation/src/log/日志系统运行包.h"
//获取碰撞配置字段读取工具
#include "src/collision/Common/字段读取.h"

namespace engine
{
	//引入字段读取工具
	using namespace collision_detail;

	//碰撞体配置应用
	void Collision_Proxy::collider_config_apply(const string& region, uint64_t collider_ID,
		const json& config)
	{
		//查找目标碰撞空间
		Collision_Region* target = backend.region_seek(region);
		//若目标碰撞空间不存在
		if (!target)
		{
			logger.warn("Collision_Proxy::待设置碰撞体的碰撞空间({})不存在", region);
			return;
		}

		//若配置中带有几何体
		if (config.contains(collision_event::field_geometry))
		{
			//几何配置（补齐碰撞体编号）
			json geometry;

			//对象形式：几何对象本体即为单个几何体（旧约定：位置与旋转写在几何对象内）
			if (config[collision_event::field_geometry].is_object())
			{
				geometry = config[collision_event::field_geometry];
				//写入目标碰撞体编号
				geometry[collision_event::field_collider_ID] = collider_ID;
			}
			//数组形式：作为几何体集合收容（基准位置与基准旋转写在配置顶层）
			else if (config[collision_event::field_geometry].is_array())
			{
				//写入目标碰撞体编号
				geometry[collision_event::field_collider_ID] = collider_ID;
				//收容几何体集合
				geometry[collision_event::field_geometries] = config[collision_event::field_geometry];
				//合并顶层基准位置与基准旋转
				if (config.contains(collision_event::field_position))
					geometry[collision_event::field_position] = config[collision_event::field_position];
				if (config.contains(collision_event::field_rotation))
					geometry[collision_event::field_rotation] = config[collision_event::field_rotation];
			}
			//其余形式视为未提供有效几何体
			else
				geometry = json{};

			//有效几何配置才下发
			if (geometry.is_object() && !geometry.empty())
			{
				//设置几何体
				if (target->collider_set(geometry))
				{
					/*
					按子部件编号保留运动登记
					几何重建后已消失的子部件编号不再有效，仅保留仍存在的编号；
					整体运动不受几何重建影响。
					*/
					auto motion_it = motion_events.find(collider_ID);
					if (motion_it != motion_events.end() &&
						motion_it->second.contains(collision_event::field_parts) &&
						motion_it->second[collision_event::field_parts].is_array())
					{
						//保留的部件运动条目
						nlohmann::json kept = nlohmann::json::array();
						//逐条检查子部件编号
						for (const nlohmann::json& item : motion_it->second[collision_event::field_parts])
						{
							//子部件编号
							uint64_t part = 0;
							//无法读取或编号不存在则丢弃
							if (!number_read(item, collision_event::field_part, part) ||
								!target->collider_part_exists(collider_ID,
									static_cast<uint16_t>(part)))
								continue;
							//保留该条目
							kept.push_back(item);
						}
						//写回保留结果
						motion_it->second[collision_event::field_parts] = std::move(kept);
					}
				}
				else
					logger.warn("Collision_Proxy::碰撞体({})几何体设置失败", collider_ID);
			}
		}

		//若配置中带有检测方式
		if (config.contains(collision_event::field_detection_mode))
		{
			//检测方式
			Detection_Mode detection_mode;
			//检测方式字段检查
			if (config[collision_event::field_detection_mode].is_object() &&
				config[collision_event::field_detection_mode].contains(collision_event::field_is_swept_volume) &&
				config[collision_event::field_detection_mode][collision_event::field_is_swept_volume].is_boolean())
			{
				//写入扫掠检测标记
				detection_mode.is_swept_volume =
					config[collision_event::field_detection_mode][collision_event::field_is_swept_volume].get<bool>();
				//写入离散检测步长
				if (config[collision_event::field_detection_mode].contains(collision_event::field_step_length) &&
					config[collision_event::field_detection_mode][collision_event::field_step_length].is_number_integer())
					detection_mode.step_length =
						config[collision_event::field_detection_mode][collision_event::field_step_length].get<uint32_t>();
				//设置检测方式
				collider_set(collider_ID, detection_mode);

				/*
				离散步长被清零时清除不可用的运动登记
				含旋转或部件运动的运动量必须显式提供步数，步长归零后该登记不再可执行。
				*/
				if (detection_mode.step_length == 0)
				{
					//运动事件
					Motion_Event motion_event;
					//若登记存在且含旋转或部件运动
					if (motion_seek(collider_ID, motion_event) &&
						(!motion_event.body.rotation.identity_check() ||
							!motion_event.parts.empty()))
					{
						logger.warn("Collision_Proxy::碰撞体({})离散步长归零，已清除含旋转或部件运动的登记",
							collider_ID);
						motion_events.erase(collider_ID);
					}
				}
			}
			else
				logger.warn("Collision_Proxy::碰撞体({})字段(detection_mode)非法", collider_ID);
		}

		//若配置中带有豁免标记
		if (config.contains(collision_event::field_exemption_flag))
		{
			//豁免标记
			uint64_t exemption_flag = 0;
			//读取豁免标记
			if (number_read(config, collision_event::field_exemption_flag, exemption_flag))
				collider_set(collider_ID, exemption_flag);
			else
				logger.warn("Collision_Proxy::碰撞体({})字段(exemption_flag)非法", collider_ID);
		}
	}
}