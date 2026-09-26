#include "../局部命名空间使用.h"
//获取日志系统
#include "src/tools/Logging/日志系统.h"

namespace engine
{
	//文件内部辅助设施（不对外暴露）
	namespace
	{
		/*
		探针射线方向
		取三轴均非零的固定方向，避免射线与边界面片共面或平行导致漏检；
		包含性判定只取决于与边界交点数的奇偶性，与射线方向无关。
		*/
		const Vector3 probe_direction = Vector3(1.0f, 0.37f, 0.19f).normalized();
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

	//边界点包含性判定(射线奇偶：与边界网格交点数为奇数则点在边界内部)
	bool Collision_Region::boundary_point_inside(const Vector3& point) const
	{
		//空间边界尚未构建时无从判定
		if (!region_boundary.mounted_shape())
			return false;

		//空间边界包围盒
		Vector3 aabb_min, aabb_max;
		//读取空间边界包围盒
		region_boundary.mounted_shape()->getAabb(region_boundary.object.getWorldTransform(),
			aabb_min, aabb_max);
		//射线终点（取包围盒对角线两倍长度，保证射线足以穿出空间边界）
		Vector3 ray_to = point + probe_direction * ((aabb_max - aabb_min).length() * 2.0f + 1.0f);

		//全命中射线回调（收集射线沿途的全部交点）
		AllHits_Ray_Callback callback(point, ray_to);
		//执行射线检测
		backend.world->rayTest(point, ray_to, callback);

		//射线与空间边界的交点数
		int boundary_hits = 0;
		//逐个比对命中对象（子弹库数组无迭代器，按索引遍历）
		for (int i = 0; i < callback.m_collisionObjects.size(); ++i)
		{
			//仅统计空间边界自身的交点
			if (callback.m_collisionObjects[i] == &region_boundary.object)
				++boundary_hits;
		}

		//交点数为奇数说明探针点位于空间边界内部
		return (boundary_hits % 2) == 1;
	}

	//跨越状态更新(位置更新后比对旧状态并收集跨越通知)
	void Collision_Region::cross_state_update(const std::vector<uint64_t>& boundary_contacts)
	{
		//空间边界尚未构建时无跨越可言，且不记录状态（待边界构建后重新建立基准）
		if (!region_boundary.mounted_shape())
			return;

		//逐个碰撞体判定当前的跨越状态
		for (auto& [collider_ID, collider] : mapping)
		{
			//未挂载几何体的碰撞体不参与跨越判定
			if (!collider.mounted_shape())
				continue;

			//当前跨越状态
			cross_state current = cross_state::outside;
			//与空间边界存在接触则判定为部分跨越
			if (std::find(boundary_contacts.begin(), boundary_contacts.end(), collider_ID)
				!= boundary_contacts.end())
				current = cross_state::crossing;
			else
			{
				//碰撞体世界包围盒
				Vector3 aabb_min, aabb_max;
				//读取碰撞体世界包围盒
				collider.mounted_shape()->getAabb(collider.object.getWorldTransform(),
					aabb_min, aabb_max);
				//探针点（取包围盒中心）
				Vector3 probe = (aabb_min + aabb_max) * 0.5f;
				//无接触时以射线奇偶判定探针点在边界内还是边界外
				current = boundary_point_inside(probe) ? cross_state::inside : cross_state::outside;
			}

			//查找该碰撞体的跨越状态记录
			auto it = cross_states.find(collider_ID);
			//首次判定仅建立基准，不产生通知
			if (it == cross_states.end())
			{
				cross_states[collider_ID] = current;
				continue;
			}
			//状态未转移则无需通知
			if (it->second == current)
				continue;

			//跨越通知
			Cross_Notice notice;
			//写入碰撞体编号
			notice.collider_ID = collider_ID;
			//按转移后的状态命名（首次部分跨越/完全回归/完全超出跨越）
			notice.kind = (current == cross_state::crossing) ? "cross"
				: (current == cross_state::inside) ? "return" : "exit";
			//收集本帧通知
			cross_notices.push_back(notice);
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
	}
}