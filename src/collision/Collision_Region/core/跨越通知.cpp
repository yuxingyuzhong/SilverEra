#include "../局部命名空间使用.h"
//获取碰撞体后端声明
#include "src/collision/Collider/backend/碰撞体后端.h"
//获取碰撞空间后端声明
#include "src/collision/Collision_Region/backend/空间后端.h"
//获取逻辑帧操作工具
#include "Engine/EngineSystem/common/runtime/逻辑帧.h"
//获取无序集合
#include <unordered_set>

namespace engine
{
	//文件内部辅助设施（不对外暴露）
	namespace
	{
		//探针点集合（包围盒中心与八个顶点）
		void probe_points_build(const Vector3& aabb_min, const Vector3& aabb_max,
			std::vector<Vector3>& probes)
		{
			//清空并预留容量
			probes.clear();
			probes.reserve(9);
			//中心探针
			probes.push_back((aabb_min + aabb_max) * 0.5f);
			//八个顶点探针（按三轴内外组合枚举）
			for (uint32_t index = 0; index < 8; ++index)
			{
				//顶点探针
				Vector3 corner;
				corner.setValue((index & 1u) ? aabb_max.getX() : aabb_min.getX(),
					(index & 2u) ? aabb_max.getY() : aabb_min.getY(),
					(index & 4u) ? aabb_max.getZ() : aabb_min.getZ());
				probes.push_back(corner);
			}
		}

		//接触状态判定（九探针内外混合即部分跨越）
		Cross_State contact_state_probe(const Region_Backend& backend, const Vector3& aabb_min,
			const Vector3& aabb_max, std::vector<Vector3>& probes)
		{
			//生成探针点
			probe_points_build(aabb_min, aabb_max, probes);
			//内部探针数
			uint32_t inside_count = 0;
			//逐探针判定
			for (const Vector3& probe : probes)
				//位于边界内部则计数
				if (backend.point_inside(probe))
					++inside_count;
			//全部在内
			if (inside_count == static_cast<uint32_t>(probes.size()))
				return Cross_State::inside;
			//全部在外
			if (inside_count == 0)
				return Cross_State::outside;
			//内外混合即部分跨越
			return Cross_State::crossing;
		}
	}

	//跨越通知取走(取出并清空本帧收集的跨越通知)
	std::vector<Cross_Notice> Collision_Region::cross_notices_take(void)
	{
		//本次取走的跨越通知
		std::vector<Cross_Notice> notices;
		//移交并清空本帧收集
		notices.swap(cross_notices);
		return notices;
	}

	//跨越状态快照(查询用，含尚未建立基准的碰撞体)
	void Collision_Region::cross_state_snapshot(std::vector<Cross_State_Entry>& receiver) const
	{
		//清空接收集合
		receiver.clear();
		//预留容量
		receiver.reserve(mapping.size());
		//逐个碰撞体输出状态
		for (const auto& [collider_ID, collider] : mapping)
		{
			//快照条目
			Cross_State_Entry entry;
			//写入碰撞体编号
			entry.collider_ID = collider_ID;
			//查找状态记录
			auto it = cross_states.find(collider_ID);
			//已有基准且几何已挂载时才视为已知
			if (it != cross_states.end() && collider.geometry_mounted())
			{
				entry.state = it->second;
				entry.known = true;
			}
			//收容条目
			receiver.push_back(entry);
		}
	}

	//跨越状态更新(位置更新后比对旧状态并收集跨越通知)
	void Collision_Region::cross_state_update(const std::vector<uint64_t>& boundary_contacts,
		const std::vector<uint64_t>& swept_contacts,
		const std::unordered_set<uint64_t>& moved_IDs)
	{
		//空间边界尚未构建时无跨越可言，且不记录状态（待边界构建后重新建立基准）
		if (!backend.boundary_mounted())
			return;

		//离散接触集合（去重）
		std::unordered_set<uint64_t> contacts(boundary_contacts.begin(), boundary_contacts.end());
		//扫掠穿越集合（去重）
		std::unordered_set<uint64_t> traversed(swept_contacts.begin(), swept_contacts.end());
		//本帧逻辑帧序号
		uint64_t frame_now = Logics::frame_count_get();
		//探针缓存（逐碰撞体复用）
		std::vector<Vector3> probes;

		//逐个碰撞体判定当前的跨越状态
		for (auto& [collider_ID, collider] : mapping)
		{
			//未挂载几何体的碰撞体不参与跨越判定
			if (!collider.geometry_mounted())
				continue;

			//状态记录
			auto it = cross_states.find(collider_ID);
			//是否与边界接触
			bool touching = contacts.count(collider_ID) > 0;
			//是否发生扫掠穿越
			bool is_traversed = traversed.count(collider_ID) > 0;
			//本帧是否移动
			bool is_moved = moved_IDs.count(collider_ID) > 0;
			//是否被标记为须强制重算
			bool is_dirty = cross_dirty.count(collider_ID) > 0;
			/*
			未移动、未穿越且已有基准时沿用旧状态
			边界更换与几何重建会通过清空或脏标记强制重算；
			未移动者与边界的接触关系不会变化，故不参与重算。
			*/
			if (it != cross_states.end() && !is_traversed && !is_moved && !is_dirty)
				continue;

			//碰撞体世界包围盒
			Vector3 aabb_min, aabb_max;
			collider.backend->aabb_get(aabb_min, aabb_max);
			//判定位置（包围盒中心）
			Vector3 position = (aabb_min + aabb_max) * 0.5f;

			//当前跨越状态
			Cross_State current = Cross_State::outside;
			//与边界接触时用多探针判定，否则用中心射线奇偶判定
			if (touching)
				current = contact_state_probe(backend, aabb_min, aabb_max, probes);
			else
				current = backend.point_inside(position) ? Cross_State::inside : Cross_State::outside;

			//本帧判定已消费脏标记
			cross_dirty.erase(collider_ID);

			//首次判定仅建立基准，不产生通知
			if (it == cross_states.end())
			{
				cross_states[collider_ID] = current;
				continue;
			}

			//转移前状态
			Cross_State previous = it->second;
			//通知类型集合（按发生顺序）
			std::vector<std::string> kinds;
			//本次进入部分跨越（首次接触边界面或由内外状态转入）
			bool enter_crossing = (current == Cross_State::crossing &&
				previous != Cross_State::crossing);
			//发生穿越或首次进入部分跨越时先发跨面通知
			if (is_traversed || enter_crossing)
				kinds.push_back("cross");
			/*
			最终状态迁移通知
			扫掠穿越后最终在外时即使旧状态同为在外，也补发 exit 表示“穿过又离开”。
			*/
			if (current != Cross_State::crossing &&
				(current != previous || (is_traversed && current == Cross_State::outside)))
				kinds.push_back((current == Cross_State::inside) ? "return" : "exit");

			//逐条生成通知
			for (const std::string& kind : kinds)
			{
				//跨越通知
				Cross_Notice notice;
				//写入碰撞体编号、通知类型与转移前状态
				notice.collider_ID = collider_ID;
				notice.kind = kind;
				notice.previous = previous;
				//写入判定位置、逻辑帧与通知序号
				notice.position = position;
				notice.frame = frame_now;
				notice.sequence = ++cross_sequence;
				//收集本帧通知
				cross_notices.push_back(notice);
			}

			//更新状态记录
			it->second = current;
		}

		//清理已不属本空间的碰撞体的状态记录（避免编号回收后被新碰撞体误继承）
		for (auto it = cross_states.begin(); it != cross_states.end();)
		{
			//编号已不在本空间内则抹除记录
			if (!mapping.count(it->first))
				it = cross_states.erase(it);
			else
				++it;
		}
		//清理已不属本空间的碰撞体的脏标记
		for (auto it = cross_dirty.begin(); it != cross_dirty.end();)
		{
			//编号已不在本空间内则抹除标记
			if (!mapping.count(*it))
				it = cross_dirty.erase(it);
			else
				++it;
		}
	}
}
