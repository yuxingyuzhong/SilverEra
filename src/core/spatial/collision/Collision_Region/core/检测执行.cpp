#include "../局部命名空间使用.h"
//获取日志系统
#include "src/tools/Logging/日志系统.h"

namespace engine
{
	//文件内部辅助设施（不对外暴露）
	namespace
	{
		//扫掠检测命中收集器
		class Swept_Collector : public Swept_Callback
		{
		private:
			//发起扫掠的碰撞对象
			const Collision_Object* caster_object = nullptr;
		public:
			//扫掠命中的碰撞对象集合
			std::vector<const Collision_Object*> hits;

			//构造函数
			explicit Swept_Collector(const Collision_Object* caster) : caster_object(caster)
			{
			}

			//单个命中结果回调
			Scalar addSingleResult(Swept_Result& convex_result, bool normal_in_world_space) override
			{
				//跳过发起扫掠的碰撞对象自身
				if (convex_result.m_hitCollisionObject == caster_object)
					return 1.0f;
				//记录命中对象
				hits.push_back(convex_result.m_hitCollisionObject);
				//固定返回未命中分数，保证收集全部命中而非最近一次
				return 1.0f;
			}
		};

		//碰撞对归一化（较小编号在前，便于判重）
		Collision_Result pair_normalize(const uint64_t collider_ID_A, const uint64_t collider_ID_B)
		{
			//碰撞对
			Collision_Result result;
			//写入归一化编号
			result.collider_A = (collider_ID_A < collider_ID_B) ? collider_ID_A : collider_ID_B;
			result.collider_B = (collider_ID_A < collider_ID_B) ? collider_ID_B : collider_ID_A;
			return result;
		}

		//碰撞对去重
		void pair_unique(std::vector<Collision_Result>& pairs)
		{
			//已出现碰撞对集合
			set<pair<uint64_t, uint64_t>> appeared;
			//去重后的碰撞对集合
			vector<Collision_Result> unique_pairs;
			//逐个判重
			for (const Collision_Result& pair_item : pairs)
			{
				//若已出现过则跳过
				if (!appeared.insert({ pair_item.collider_A, pair_item.collider_B }).second)
					continue;
				//保留首次出现的碰撞对
				unique_pairs.push_back(pair_item);
			}
			//写回去重结果
			pairs.swap(unique_pairs);
		}
	}

	//执行碰撞检测
	std::optional<std::vector<Collision_Result>> Collision_Region::detect(void)
	{
		//空间无效或未激活时无可检测内容
		if (!valid() || !is_active)
			return std::nullopt;

		//碰撞对集合
		vector<Collision_Result> pairs;
		//刷新碰撞世界内的包围盒（挂入后尚未执行过检测时需要）
		backend.world->updateAabbs();

		//---------- 位移重读：按最新位移事件刷新各碰撞体的位移向量 ----------
		for (auto& [collider_ID, collider] : mapping)
		{
			//位移事件读取回调未注入时跳过
			if (!displacement_reader)
				continue;
			//位移向量
			Vector3 displacement;
			//若该编号存在位移事件则重读事件配置内的位移向量
			if (displacement_reader(collider_ID, displacement))
				collider.displacement_vector = displacement;
		}

		//---------- 扫掠检测：位移途中命中的碰撞体 ----------
		for (auto& [collider_ID, collider] : mapping)
		{
			//仅处理启用扫掠检测的碰撞体
			if (!collider.detection_mode.is_swept_volume)
				continue;
			//位移已作废的碰撞体不再运动
			if (collider.displacement_invalid)
				continue;
			//位移向量为零时无需扫掠
			if (collider.displacement_vector.length2() <= 0)
				continue;

			//碰撞体世界变换
			const Transform collider_transform = collider.object.getWorldTransform();
			//逐个几何体部件执行扫掠（仅凸包部件可扫掠）
			for (const Geometry_Part& part : collider.parts)
			{
				//仅处理已挂载且为凸包的部件
				if (!part.shape || !part.shape->isConvex())
					continue;

				//扫掠起点变换（碰撞体世界变换 ∘ 部件相对变换）
				Transform sweep_from = collider_transform * part.local_transform;
				//扫掠终点变换
				Transform sweep_to = sweep_from;
				sweep_to.getOrigin() += collider.displacement_vector;

				//扫掠命中收集器
				Swept_Collector collector(&collider.object);
				//执行凸包扫掠
				backend.world->convexSweepTest(static_cast<const Convex_Shape*>(part.shape.get()),
					sweep_from, sweep_to, collector,
					backend.world->getDispatchInfo().m_allowedCcdPenetration);

				//汇总扫掠命中
				for (const Collision_Object* hit_object : collector.hits)
				{
					//反查命中碰撞体
					const Collider* hit_collider = Collider::recover(hit_object);
					//若无法反查则跳过
					if (!hit_collider)
						continue;
					//空间边界不属于碰撞体，不参与碰撞对
					if (hit_collider == &region_boundary)
						continue;
					//同组豁免的碰撞对跳过
					if (collider.exemption_flag != 0 &&
						collider.exemption_flag == hit_collider->exemption_flag)
						continue;

					//记录碰撞对
					pairs.push_back(pair_normalize(collider_ID, hit_collider->ID));
				}
			}
		}

		//---------- 离散检测：就地平移后求交叉接触 ----------
		for (auto& [collider_ID, collider] : mapping)
		{
			//位移已作废的碰撞体不再运动
			if (collider.displacement_invalid)
				continue;
			//位移向量为零时无需平移
			if (collider.displacement_vector.length2() <= 0)
				continue;

			//就地平移碰撞对象
			Transform transform = collider.object.getWorldTransform();
			transform.setOrigin(transform.getOrigin() + collider.displacement_vector);
			collider.object.setWorldTransform(transform);
		}

		//执行离散碰撞检测
		backend.world->performDiscreteCollisionDetection();

		/*
		与空间边界存在接触的碰撞体编号
		供跨越状态判定使用：接触即视为部分跨越，无接触时再以射线奇偶判定内外。
		*/
		vector<uint64_t> boundary_contacts;
		//碰撞流形数量
		int manifold_count = backend.dispatcher->getNumManifolds();
		//逐个流形提取碰撞对
		for (int i = 0; i < manifold_count; ++i)
		{
			//目标流形
			Persistent_Manifold* manifold = backend.dispatcher->getManifoldByIndexInternal(i);
			//无接触点则跳过
			if (manifold->getNumContacts() == 0)
				continue;

			//反查流形两侧碰撞体
			const Collider* collider_A = Collider::recover(manifold->getBody0());
			const Collider* collider_B = Collider::recover(manifold->getBody1());
			//若任一侧无法反查则跳过
			if (!collider_A || !collider_B)
				continue;

			//与空间边界接触：登记接触并跳过碰撞对（空间边界不属于碰撞体）
			if (collider_A == &region_boundary || collider_B == &region_boundary)
			{
				//接触对中位于空间边界另一侧的碰撞体
				const Collider* contacted = (collider_A == &region_boundary) ? collider_B : collider_A;
				//登记该碰撞体与空间边界的接触
				boundary_contacts.push_back(contacted->ID);
				continue;
			}

			//同组豁免的碰撞对跳过
			if (collider_A->exemption_flag != 0 &&
				collider_A->exemption_flag == collider_B->exemption_flag)
				continue;

			//记录碰撞对
			pairs.push_back(pair_normalize(collider_A->ID, collider_B->ID));
		}

		//去重后返回检测结果
		pair_unique(pairs);
		//跨越状态判定（位置更新后比对旧状态并收集跨越通知）
		cross_state_update(boundary_contacts);
		return pairs;
	}
}
