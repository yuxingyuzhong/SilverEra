#include "../局部命名空间使用.h"
#include "Engine/EngineFoundation/src/log/日志系统运行包.h"

namespace engine
{
	//构造函数
	Collision_Proxy::Collision_Proxy()
	{
		//生成事件发送权限密钥
		acl_key = event_terminal.key().value();
		//绑定运动读取回调（碰撞空间更新位置时向本代理器重读运动事件）
		backend.motion_reader_bind([this](const uint64_t collider_ID, Motion_Event& receiver)
			{
				return this->motion_seek(collider_ID, receiver);
			});
	}

	//碰撞空间构建
	bool Collision_Proxy::region_build(const string& region)
	{
		return backend.region_build(region);
	}

	//碰撞空间卸载
	bool Collision_Proxy::region_unload(const string& region)
	{
		//卸载碰撞空间
		if (!backend.region_unload(region))
			return false;
		//清空该空间的结果事件开关登记
		event_output.erase(region);
		return true;
	}

	//碰撞空间检测执行
	optional<vector<Collision_Detail>> Collision_Proxy::region_detect(const string& region)
	{
		//碰撞详情集合
		vector<Collision_Detail> details;
		//执行碰撞检测（后端负责空间查找与激活判定）
		if (!backend.region_detect(region, details))
			return nullopt;

		//简略碰撞对集合（结果事件使用，精确到子部件并按子部件对去重）
		vector<Collision_Result> pairs;
		//预留容量
		pairs.reserve(details.size());
		//逐个详情提取碰撞对
		for (const Collision_Detail& detail : details)
		{
			//接触点为空时仅保留碰撞体级碰撞对
			if (detail.contacts.empty())
			{
				//碰撞对
				Collision_Result pair;
				pair.collider_A = detail.collider_A;
				pair.collider_B = detail.collider_B;
				pairs.push_back(pair);
				continue;
			}

			//逐个接触点提取子部件级碰撞对
			for (const Collision_Contact& contact : detail.contacts)
			{
				//碰撞对
				Collision_Result pair;
				pair.collider_A = detail.collider_A;
				pair.collider_B = detail.collider_B;
				pair.part_A = contact.part_A;
				pair.part_B = contact.part_B;
				//检查是否已收录同一子部件对
				bool exists = false;
				for (const Collision_Result& existing : pairs)
				{
					//四元组一致即视为重复
					if (existing.collider_A == pair.collider_A &&
						existing.collider_B == pair.collider_B &&
						existing.part_A == pair.part_A &&
						existing.part_B == pair.part_B)
					{
						exists = true;
						break;
					}
				}
				//未收录时追加
				if (!exists)
					pairs.push_back(pair);
			}
		}

		//目标碰撞空间（用于取走本帧跨越通知）
		Collision_Region* target = backend.region_seek(region);
		//跨越通知发布（通知外界碰撞体已首次部分跨越/完全回归/完全超出跨越本空间）
		if (target)
			for (const Cross_Notice& notice : target->cross_notices_take())
			{
				//跨越通知载荷
				json cross_payload;
				//写入空间名称、碰撞体编号与通知类型
				cross_payload[collision_event::field_region] = region;
				cross_payload[collision_event::field_collider_ID] = notice.collider_ID;
				cross_payload[collision_event::field_state] = notice.kind;
				//写入转移前状态、判定位置、逻辑帧与通知序号
				cross_payload[collision_event::field_previous_state] =
					cross_state_text(notice.previous);
				cross_payload[collision_event::field_position] = json::array(
					{ notice.position.getX(), notice.position.getY(), notice.position.getZ() });
				cross_payload[collision_event::field_frame] = notice.frame;
				cross_payload[collision_event::field_sequence] = notice.sequence;
				//发布跨越通知事件
				event_publish(collision_event::region_cross_notice, cross_payload);
			}

		//若该空间启用了简略结果事件
		if (region_event_output_get(region))
		{
			//检测结果载荷
			json payload;
			//写入空间名称与逻辑帧序号（与跨越通知对齐）
			payload[collision_event::field_region] = region;
			payload[collision_event::field_frame] = Logics::frame_count_get();
			//写入碰撞对集合
			payload[collision_event::field_results] = json::array();
			//逐个写入碰撞对
			for (const Collision_Result& result : pairs)
			{
				//单个碰撞对
				json item;
				//写入碰撞体编号与子部件编号
				item[collision_event::field_collider_A] = result.collider_A;
				item[collision_event::field_collider_B] = result.collider_B;
				item[collision_event::field_part_A] = result.part_A;
				item[collision_event::field_part_B] = result.part_B;
				//追加碰撞对
				payload[collision_event::field_results].push_back(item);
			}

			//发布检测结果事件
			event_publish(collision_event::detect_result, payload);
		}

		//返回碰撞详情
		return details;
	}

	//简略碰撞结果事件开关设置
	bool Collision_Proxy::region_event_output_set(const string& region, bool enabled)
	{
		//若目标碰撞空间不存在
		if (!backend.region_seek(region))
		{
			logger.warn("Collision_Proxy::待设置结果事件开关的碰撞空间({})不存在", region);
			return false;
		}

		//写入开关
		event_output[region] = enabled;
		return true;
	}

	//简略碰撞结果事件开关读取
	bool Collision_Proxy::region_event_output_get(const string& region) const
	{
		//查找该空间的开关登记
		auto it = event_output.find(region);
		//未登记时默认开启
		if (it == event_output.end())
			return true;

		return it->second;
	}

	//碰撞空间活跃性设置
	bool Collision_Proxy::region_state_set(const string& region, bool active)
	{
		return backend.region_state_set(region, active);
	}
}