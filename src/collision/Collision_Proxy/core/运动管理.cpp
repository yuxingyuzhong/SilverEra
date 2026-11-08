#include "../局部命名空间使用.h"
#include "Engine/EngineFoundation/src/log/日志系统运行包.h"
//获取碰撞配置字段读取工具
#include "src/collision/Common/字段读取.h"

namespace engine
{
	//引入字段读取工具
	using namespace collision_detail;

	//文件内部辅助设施（不对外暴露）
	namespace
	{
		//运动量读取（平移与旋转均可选，缺省为零平移 + 单位旋转）
		bool motion_read(const json& config, Motion& receiver)
		{
			//运动量对象格式检查
			if (!config.is_object())
				return false;

			//运动量
			Motion motion;
			//平移字段存在时读取
			if (config.contains(collision_event::field_translation))
			{
				//读取平移
				if (!vector_read(config, collision_event::field_translation, motion.translation))
					return false;
			}
			//旋转字段存在时读取
			if (config.contains(collision_event::field_rotation))
			{
				//四元数
				Quaternion rotation;
				//读取四元数
				if (!quaternion_read(config, collision_event::field_rotation, rotation))
					return false;
				//零四元数非法
				if (rotation.length2() <= 0.0f)
					return false;
				//写入单位化旋转
				motion.rotation = rotation.normalized();
			}

			receiver = motion;
			return true;
		}

		//部件运动集合读取（同一下标重复出现时后者覆盖前者）
		bool parts_read(const json& config, std::vector<Part_Motion>& receiver)
		{
			//部件集合
			std::vector<Part_Motion> parts;
			//逐项读取
			for (const json& item : config)
			{
				//子部件编号
				uint64_t part = 0;
				//读取子部件编号
				if (!number_read(item, collision_event::field_part, part))
					return false;
				//编号超出 16 位范围或为保留哨兵值
				if (part >= 0xFFFF)
					return false;

				//部件运动
				Part_Motion part_motion;
				part_motion.part = static_cast<uint16_t>(part);
				//读取该部件运动量（平移与旋转直接位于部件对象内）
				if (!motion_read(item, part_motion.motion))
					return false;

				//同一下标覆盖旧值
				bool replaced = false;
				for (Part_Motion& existing : parts)
				{
					//下标一致即覆盖
					if (existing.part == part_motion.part)
					{
						existing = part_motion;
						replaced = true;
						break;
					}
				}
				//首次出现时追加
				if (!replaced)
					parts.push_back(part_motion);
			}

			receiver = std::move(parts);
			return true;
		}
	}

	//运动事件登记（保存运动事件配置，同编号的新事件覆盖旧事件）
	void Collision_Proxy::motion_register(const json& config)
	{
		//目标碰撞体编号
		uint64_t collider_ID = 0;
		//读取目标碰撞体编号
		if (!number_read(config, collision_event::field_collider_ID, collider_ID))
		{
			logger.warn("Collision_Proxy::运动事件缺少有效字段(collider_ID)");
			return;
		}

		//整体运动
		Motion motion;
		//是否携带整体运动
		bool has_motion = config.contains(collision_event::field_motion);
		//读取整体运动
		if (has_motion && !motion_read(config[collision_event::field_motion], motion))
		{
			logger.warn("Collision_Proxy::碰撞体({})字段(motion)非法", collider_ID);
			return;
		}

		//部件运动集合
		std::vector<Part_Motion> parts;
		//是否携带部件运动
		bool has_parts = config.contains(collision_event::field_parts);
		//读取部件运动集合
		if (has_parts)
		{
			//部件集合必须为数组
			if (!config[collision_event::field_parts].is_array() ||
				!parts_read(config[collision_event::field_parts], parts))
			{
				logger.warn("Collision_Proxy::碰撞体({})字段(parts)非法", collider_ID);
				return;
			}
		}

		//整体运动与部件运动至少提供一个
		if (!has_motion && !has_parts)
		{
			logger.warn("Collision_Proxy::碰撞体({})运动事件未提供任何运动通道", collider_ID);
			return;
		}

		//离散步长
		uint32_t step_length = 0;
		//读取离散步长（编号不存在时拒绝）
		bool found = backend.collider_set_dispatch(collider_ID,
			[collider_ID, &step_length](Collision_Region& region)
			{
				return region.collider_step_length_get(collider_ID, step_length);
			});
		if (!found)
		{
			logger.warn("Collision_Proxy::运动事件目标碰撞体({})不存在", collider_ID);
			return;
		}

		//旋转或部件运动要求显式步长
		bool needs_steps = (has_motion && !motion.rotation.identity_check()) || has_parts;
		if (needs_steps && step_length == 0)
		{
			logger.warn("Collision_Proxy::碰撞体({})含旋转或部件运动但离散步长为零，事件拒绝",
				collider_ID);
			return;
		}

		//子部件编号存在性检查
		for (const Part_Motion& part_motion : parts)
		{
			//子部件是否存在
			bool exists = false;
			//查询子部件编号
			backend.collider_set_dispatch(collider_ID,
				[collider_ID, &part_motion, &exists](Collision_Region& region)
				{
					exists = region.collider_part_exists(collider_ID, part_motion.part);
					return true;
				});
			//编号不存在则整条拒绝
			if (!exists)
			{
				logger.warn("Collision_Proxy::碰撞体({})子部件编号({})不存在",
					collider_ID, part_motion.part);
				return;
			}
		}

		/*
		运动作用频率
		缺省或非法（为零、逻辑帧率不可用、不整除逻辑帧率）时取零，
		此时碰撞空间按旧行为每次检测都重读并施加全量运动。
		*/
		uint64_t frequency = 0;
		//读取作用频率（可选字段，缺省时不作提示）
		if (config.contains(collision_event::field_frequency))
			number_read(config, collision_event::field_frequency, frequency);
		//逻辑帧率
		uint64_t frame_rate = Logics::logic_frames_get();
		//作用频率合法性检查（作用频率必须为逻辑帧率的因数以保证整除）
		if (frequency != 0 && (frame_rate == 0 || frame_rate % frequency != 0))
		{
			logger.warn("Collision_Proxy::碰撞体({})字段(frequency={})非法，须整除逻辑帧率({})，改按每帧全量运动处理",
				collider_ID, frequency, frame_rate);
			frequency = 0;
		}

		//保存运动事件（同编号的新事件覆盖旧事件，待生效帧重读）
		motion_events[collider_ID] = config;
		//下发作用频率（频率与生效计时随碰撞体保存于碰撞空间，并重新起算生效计时）
		backend.collider_set_dispatch(collider_ID, [collider_ID, frequency](
			Collision_Region& region)
			{
				return region.collider_frequency_set(collider_ID, frequency);
			});
	}

	//运动量查询（供碰撞空间在更新位置时读取最新运动事件）
	bool Collision_Proxy::motion_seek(const uint64_t collider_ID, Motion_Event& receiver) const
	{
		//查找该编号的运动事件
		auto it = motion_events.find(collider_ID);
		//若该编号尚无运动事件
		if (it == motion_events.end())
			return false;

		//运动事件
		Motion_Event event;
		//读取整体运动（缺省为零运动）
		if (it->second.contains(collision_event::field_motion) &&
			!motion_read(it->second[collision_event::field_motion], event.body))
			return false;
		//读取部件运动
		if (it->second.contains(collision_event::field_parts) &&
			!parts_read(it->second[collision_event::field_parts], event.parts))
			return false;

		receiver = std::move(event);
		return true;
	}
}
