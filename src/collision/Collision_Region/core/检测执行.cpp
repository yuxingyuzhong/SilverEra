#include "../局部命名空间使用.h"
//获取碰撞体后端声明
#include "src/collision/Collider/backend/碰撞体后端.h"
//获取日志系统
#include "Engine/EngineFoundation/src/log/日志系统运行包.h"
//获取逻辑帧操作工具
#include "Engine/EngineSystem/common/runtime/逻辑帧.h"

namespace engine
{
	//执行碰撞检测（返回碰撞详情，含接触点/法线/穿透深度）
	std::optional<std::vector<Collision_Detail>> Collision_Region::detect_detail(void)
	{
		//空间无效或未激活时无可检测内容
		if (!valid() || !is_active)
			return std::nullopt;

		//碰撞详情集合
		vector<Collision_Detail> details;
		//刷新碰撞世界内的包围盒（挂入后尚未执行过检测时需要）
		backend.aabbs_refresh();

		//当前逻辑帧序号
		uint64_t frame_now = Logics::frame_count_get();
		//逻辑帧率（一个逻辑帧周期含有的帧数）
		uint64_t frame_rate = Logics::logic_frames_get();

		//---------- 运动生效门控：按逻辑帧间隔决定本帧是否施加运动 ----------
		for (auto& [collider_ID, collider] : mapping)
		{
			//运动作用频率
			uint64_t frequency = collider.motion_frequency;
			/*
			未配置频率或帧率不可用时按旧行为处理
			即每次检测都重读最新运动事件并施加全量运动。
			*/
			if (frequency == 0 || frame_rate == 0)
			{
				//运动事件读取回调已注入时重读最新运动事件
				if (motion_reader)
				{
					//运动事件
					Motion_Event event;
					//若该编号存在运动事件则重读事件配置内的整体与部件运动
					if (motion_reader(collider_ID, event))
					{
						collider.motion = event.body;
						collider.part_motions = std::move(event.parts);
					}
					//无运动事件即停止运动（事件被撤销后不得沿用旧运动）
					else
					{
						collider.motion = Motion();
						collider.part_motions.clear();
					}
				}
				//本帧施加量即运动量本身
				collider.motion_step = collider.motion;
				//部件运动直接按周期总量施加
				collider.part_motion_steps = collider.part_motions;
				continue;
			}

			//帧间隔（逻辑帧率按因数整除作用频率）
			uint64_t interval = frame_rate / frequency;
			//未达帧间隔时对运动量不做任何操作（含不重读外界的运动更新）
			if (frame_now - collider.motion_frame < interval)
			{
				collider.motion_step = Motion();
				collider.part_motion_steps.clear();
				continue;
			}

			//记录本次生效的逻辑帧序号
			collider.motion_frame = frame_now;
			//生效帧重读运动事件（暂存的最新配置至此生效）
			if (motion_reader)
			{
				//运动事件
				Motion_Event event;
				//若该编号存在运动事件则重读事件配置内的整体与部件运动
				if (motion_reader(collider_ID, event))
				{
					collider.motion = event.body;
					collider.part_motions = std::move(event.parts);
				}
				//无运动事件即停止运动（事件被撤销后不得沿用旧运动）
				else
				{
					collider.motion = Motion();
					collider.part_motions.clear();
				}
			}
			//本帧施加量为一个周期总运动的作用频率分之一
			collider.motion_step = collider.motion / static_cast<Scalar>(frequency);
			//部件运动同样按作用频率拆分
			collider.part_motion_steps.clear();
			collider.part_motion_steps.reserve(collider.part_motions.size());
			for (const Part_Motion& part_motion : collider.part_motions)
			{
				//拆分后的部件运动
				Part_Motion step_item;
				step_item.part = part_motion.part;
				step_item.motion = part_motion.motion / static_cast<Scalar>(frequency);
				collider.part_motion_steps.push_back(step_item);
			}
		}

		//---------- 移动体清单：扫掠清单与离散清单 ----------
		//全部移动体（参与离散推进）
		vector<Collider_Move> all_moves;
		//扫掠移动体（仅纯平移且启用扫掠检测者）
		vector<Collider_Move> swept_moves;
		//预留容量
		all_moves.reserve(mapping.size());
		swept_moves.reserve(mapping.size());
		//逐个收集本帧移动的碰撞体
		for (const auto& [collider_ID, collider] : mapping)
		{
			//本帧运动量为空的碰撞体不移动
			if (collider.motion_step.empty() && collider.part_motion_steps.empty())
				continue;

			//移动体描述
			Collider_Move move_item;
			move_item.ID = collider_ID;
			move_item.backend = collider.backend.get();
			move_item.body_step = collider.motion_step;
			move_item.part_steps = collider.part_motion_steps;
			move_item.step_count = collider.detection_mode.step_length;
			//登记离散移动体
			all_moves.push_back(move_item);
			/*
			扫掠清单只收纯平移且无部件运动的移动体：
			旋转会改变形状朝向，部件运动改变形状内部布局，二者均无法用单次扫掠表达。
			*/
			if (collider.detection_mode.is_swept_volume &&
				collider.motion_step.rotation.identity_check() &&
				collider.part_motion_steps.empty())
				swept_moves.push_back(move_item);
		}

		//本帧移动过的碰撞体编号集合（跨越判定缓存用）
		std::unordered_set<uint64_t> moved_IDs;
		//预留容量
		moved_IDs.reserve(all_moves.size());
		//逐个登记移动体编号
		for (const Collider_Move& move : all_moves)
			moved_IDs.insert(move.ID);

		//---------- 扫掠检测：后端内部按段推进 ----------
		//扫掠路径穿过空间边界的碰撞体编号
		vector<uint64_t> swept_contacts;
		backend.swept_collect(swept_moves, details, swept_contacts);

		//---------- 离散检测：后端内部逐段平移与求交 ----------
		//与空间边界存在接触的碰撞体编号（供跨越状态判定）
		vector<uint64_t> boundary_contacts;
		backend.discrete_run(all_moves, details, boundary_contacts);

		/*
		豁免过滤已前移到宽阶段
		被豁免的配对不会进入窄阶段，故此处不再做任何后置过滤。
		*/

		//跨越状态判定（位置更新后比对旧状态并收集跨越通知）
		cross_state_update(boundary_contacts, swept_contacts, moved_IDs);
		return details;
	}

	//执行碰撞检测（返回碰撞对，供内部/兼容调用）
	std::optional<std::vector<Collision_Result>> Collision_Region::detect(void)
	{
		//碰撞详情
		optional<vector<Collision_Detail>> details = detect_detail();
		//空间无效或未激活时直接返回空
		if (!details)
			return std::nullopt;

		//碰撞对集合
		vector<Collision_Result> pairs;
		//预留容量
		pairs.reserve(details->size());
		//逐个详情提取碰撞对（精确到子部件并按子部件对去重）
		for (const Collision_Detail& detail : *details)
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

		return pairs;
	}
}