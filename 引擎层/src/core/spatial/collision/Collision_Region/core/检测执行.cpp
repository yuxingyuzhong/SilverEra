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

		//---------- 扫掠检测：位移途中命中的碰撞体 ----------
		for (auto& [collider_ID, collider] : mapping)
		{
			//仅处理启用扫掠检测的碰撞体
			if (!collider.detection_mode.is_swept_volume)
				continue;
			//仅处理已挂载且为凸包的形状
			if (!collider.shape || !collider.shape->isConvex())
				continue;
			//位移向量为零时无需扫掠
			if (collider.displacement_vector.length2() <= 0)
				continue;

			//扫掠起点变换
			Transform sweep_from = collider.object.getWorldTransform();
			//扫掠终点变换
			Transform sweep_to = sweep_from;
			sweep_to.getOrigin() += collider.displacement_vector;

			//扫掠命中收集器
			Swept_Collector collector(&collider.object);
			//执行凸包扫掠
			backend.world->convexSweepTest(static_cast<const Convex_Shape*>(collider.shape.get()),
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
				//同组豁免的碰撞对跳过
				if (collider.exemption_flag != 0 &&
					collider.exemption_flag == hit_collider->exemption_flag)
					continue;

				//记录碰撞对
				pairs.push_back(pair_normalize(collider_ID, hit_collider->ID));
			}
		}

		//---------- 离散检测：就地平移后求交叉接触 ----------
		for (auto& [collider_ID, collider] : mapping)
		{
			//位移向量为零时无需平移
			if (collider.displacement_vector.length2() <= 0)
				continue;

			//就地平移碰撞对象
			Transform transform = collider.object.getWorldTransform();
			transform.setOrigin(transform.getOrigin() + collider.displacement_vector);
			collider.object.setWorldTransform(transform);
			//清零位移向量（位移一次性生效）
			collider.displacement_vector.setZero();
		}

		//执行离散碰撞检测
		backend.world->performDiscreteCollisionDetection();

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
			//同组豁免的碰撞对跳过
			if (collider_A->exemption_flag != 0 &&
				collider_A->exemption_flag == collider_B->exemption_flag)
				continue;

			//记录碰撞对
			pairs.push_back(pair_normalize(collider_A->ID, collider_B->ID));
		}

		//去重后返回检测结果
		pair_unique(pairs);
		return pairs;
	}
}
