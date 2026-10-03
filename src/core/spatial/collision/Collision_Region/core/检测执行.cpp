#include "../局部命名空间使用.h"
//获取日志系统
#include "Engine/EngineCore/src/tools/Logging/日志系统运行包.h"
//获取引擎环境(逻辑帧计数与逻辑帧率)
#include "Engine/EngineCore/src/tools/Engine_Env/引擎环境.h"

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

		//分段段数读取（未配置步长时视作一段）
		uint32_t step_count_get(const Collider& collider)
		{
			//配置步长
			uint32_t step_count = collider.detection_mode.step_length;
			//步长为零即不拆分
			return (step_count == 0) ? 1 : step_count;
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

		//当前逻辑帧序号
		uint64_t frame_now = Engine_Env::frame_count_get();
		//逻辑帧率（一个逻辑帧周期含有的帧数）
		uint64_t frame_rate = Engine_Env::logic_frames_get();

		//---------- 位移生效门控：按逻辑帧间隔决定本帧是否施加位移 ----------
		for (auto& [collider_ID, collider] : mapping)
		{
			//位移作用频率
			uint64_t frequency = collider.displacement_frequency;
			/*
			未配置频率或帧率不可用时按旧行为处理
			即每次检测都重读最新位移事件并施加全量位移。
			*/
			if (frequency == 0 || frame_rate == 0)
			{
				//位移事件读取回调已注入时重读最新位移事件
				if (displacement_reader)
				{
					//位移向量
					Vector3 displacement;
					//若该编号存在位移事件则重读事件配置内的位移向量
					if (displacement_reader(collider_ID, displacement))
						collider.displacement_vector = displacement;
				}
				//本帧施加量即位移向量本身
				collider.displacement_step = collider.displacement_vector;
				continue;
			}

			//帧间隔（逻辑帧率按因数整除作用频率）
			uint64_t interval = frame_rate / frequency;
			//未达帧间隔时对位移向量不做任何操作（含不重读外界的位移更新）
			if (frame_now - collider.displacement_frame < interval)
			{
				collider.displacement_step.setValue(0.0f, 0.0f, 0.0f);
				continue;
			}

			//记录本次生效的逻辑帧序号
			collider.displacement_frame = frame_now;
			//生效帧重读位移事件（暂存的最新配置至此生效）
			if (displacement_reader)
			{
				//位移向量
				Vector3 displacement;
				//若该编号存在位移事件则重读事件配置内的位移向量
				if (displacement_reader(collider_ID, displacement))
					collider.displacement_vector = displacement;
			}
			//本帧施加量为一个周期总位移的作用频率分之一
			collider.displacement_step = collider.displacement_vector / static_cast<Scalar>(frequency);
		}

		//---------- 扫掠检测：按步长分段推进，收集位移途中命中的碰撞体 ----------
		for (auto& [collider_ID, collider] : mapping)
		{
			//仅处理启用扫掠检测的碰撞体
			if (!collider.detection_mode.is_swept_volume)
				continue;
			//位移已作废的碰撞体不再运动
			if (collider.displacement_invalid)
				continue;
			//本帧施加位移为零时无需扫掠
			if (collider.displacement_step.length2() <= 0)
				continue;

			//分段段数
			uint32_t step_count = step_count_get(collider);
			//单段位移
			Vector3 segment = collider.displacement_step / static_cast<Scalar>(step_count);

			//碰撞体世界变换
			const Transform collider_transform = collider.object.getWorldTransform();
			//逐个几何体部件执行扫掠（仅凸包部件可扫掠）
			for (const Geometry_Part& part : collider.parts)
			{
				//仅处理已挂载且为凸包的部件
				if (!part.shape || !part.shape->isConvex())
					continue;

				//本部件扫掠起点变换（碰撞体世界变换 ∘ 部件相对变换）
				Transform sweep_from = collider_transform * part.local_transform;
				//逐段推进扫掠
				for (uint32_t step = 0; step < step_count; ++step)
				{
					//本段扫掠终点变换
					Transform sweep_to = sweep_from;
					sweep_to.getOrigin() += segment;

					//扫掠命中收集器
					Swept_Collector collector(&collider.object);
					//执行本段凸包扫掠
					backend.world->convexSweepTest(static_cast<const Convex_Shape*>(part.shape.get()),
						sweep_from, sweep_to, collector,
						backend.world->getDispatchInfo().m_allowedCcdPenetration);

					//汇总本段扫掠命中
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

					//推进到本段末端
					sweep_from = sweep_to;
				}
			}
		}

		/*
		与空间边界存在接触的碰撞体编号
		供跨越状态判定使用：接触即视为部分跨越，无接触时再以射线奇偶判定内外。
		分段检测下同一编号可能被多次登记，判定侧按集合成员处理，重复无碍。
		*/
		vector<uint64_t> boundary_contacts;

		//---------- 离散检测：按步长分段平移后求交叉接触 ----------
		//最大分段段数（各碰撞体段数不一，统一按最大段数推进；段数耗尽的碰撞体不再移动）
		uint32_t max_step_count = 1;
		for (const auto& record : mapping)
		{
			//本记录内的碰撞体
			const Collider& collider = record.second;
			//位移已作废或本帧施加位移为零的碰撞体不参与推进
			if (collider.displacement_invalid || collider.displacement_step.length2() <= 0)
				continue;
			//分段段数
			uint32_t step_count = step_count_get(collider);
			//取最大段数
			if (step_count > max_step_count)
				max_step_count = step_count;
		}

		//逐段推进并逐段执行离散碰撞检测
		for (uint32_t step = 0; step < max_step_count; ++step)
		{
			//本段内逐碰撞体就地平移
			for (auto& [collider_ID, collider] : mapping)
			{
				//位移已作废的碰撞体不再运动
				if (collider.displacement_invalid)
					continue;
				//本帧施加位移为零时无需平移
				if (collider.displacement_step.length2() <= 0)
					continue;

				//分段段数
				uint32_t step_count = step_count_get(collider);
				//该碰撞体的段数已耗尽则不再推进
				if (step >= step_count)
					continue;

				//单段位移
				Vector3 segment = collider.displacement_step / static_cast<Scalar>(step_count);
				//就地平移碰撞对象
				Transform transform = collider.object.getWorldTransform();
				transform.setOrigin(transform.getOrigin() + segment);
				collider.object.setWorldTransform(transform);
			}

			//执行本段离散碰撞检测
			backend.world->performDiscreteCollisionDetection();

			//本段碰撞流形数量
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
		}

		//去重后返回检测结果
		pair_unique(pairs);
		//跨越状态判定（位置更新后比对旧状态并收集跨越通知）
		cross_state_update(boundary_contacts);
		return pairs;
	}
}