#include "src/collision/Collision_Region/backend/空间后端.h"
//获取碰撞空间后端实现体
#include "src/collision/Collision_Region/backend/空间后端_内部.h"
//获取碰撞体后端实现体与访问器
#include "src/collision/Collider/backend/碰撞体后端_内部.h"
//获取几何类型转换
#include "src/collision/Common/数学转换.h"
//获取日志系统
#include "Engine/EngineFoundation/src/log/日志系统运行包.h"

namespace engine
{
	//实现体定义（沿用内部头的通用实现体，供公开句柄持有）
	struct Region_Backend::Impl : Region_Backend_Impl
	{
	};

	//文件内部辅助设施（不对外暴露）
	namespace
	{
		//扫掠检测命中记录（保留命中对象与接触信息）
		struct Swept_Hit
		{
			//命中对象
			const btCollisionObject* object = nullptr;
			//世界命中点
			btVector3 point = btVector3(0.0f, 0.0f, 0.0f);
			//世界法线（命中对象表面朝向发起方）
			btVector3 normal = btVector3(0.0f, 0.0f, 0.0f);
			//命中比例
			Scalar fraction = 1.0f;
		};

		//扫掠检测命中收集器
		class Swept_Collector : public btCollisionWorld::ConvexResultCallback
		{
		private:
			//发起扫掠的碰撞对象
			const btCollisionObject* caster_object = nullptr;
		public:
			//扫掠命中记录集合
			std::vector<Swept_Hit> hits;

			//构造函数
			explicit Swept_Collector(const btCollisionObject* caster) : caster_object(caster)
			{
			}

			//单个命中结果回调
			Scalar addSingleResult(btCollisionWorld::LocalConvexResult& convex_result, bool normal_in_world_space) override
			{
				//跳过发起扫掠的碰撞对象自身
				if (convex_result.m_hitCollisionObject == caster_object)
					return 1.0f;

				//命中记录
				Swept_Hit hit;
				hit.object = convex_result.m_hitCollisionObject;
				//命中对象世界变换（局部接触信息转世界空间）
				const btTransform& hit_transform =
					convex_result.m_hitCollisionObject->getWorldTransform();
				//世界命中点
				hit.point = hit_transform * convex_result.m_hitPointLocal;
				//世界法线（只旋转，不平移）
				hit.normal = hit_transform.getBasis() * convex_result.m_hitNormalLocal;
				hit.fraction = convex_result.m_hitFraction;
				//记录命中
				hits.push_back(hit);
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

		//碰撞详情并入（同一碰撞对复用一条详情，接触点按出现顺序追加）
		void detail_merge(std::vector<Collision_Detail>& details, const Collision_Result& pair,
			const Collision_Contact& contact)
		{
			//查找同一碰撞对已有详情
			for (Collision_Detail& detail : details)
			{
				//编号一致即复用
				if (detail.collider_A == pair.collider_A && detail.collider_B == pair.collider_B)
				{
					detail.contacts.push_back(contact);
					return;
				}
			}

			//首次出现时新建详情
			Collision_Detail detail;
			detail.collider_A = pair.collider_A;
			detail.collider_B = pair.collider_B;
			detail.contacts.push_back(contact);
			details.push_back(std::move(detail));
		}

		//分段段数读取（未配置步长时视作一段）
		uint32_t step_count_get(const Collider_Move& move)
		{
			//步长为零即不拆分
			return (move.step_count == 0) ? 1 : move.step_count;
		}
	}

	//构造函数（分配碰撞世界四件套）
	Region_Backend::Region_Backend()
	{
		//分配实现体内存
		impl.reset(new(std::nothrow) Impl());
		//若实现体分配失败则直接返回
		if (!impl)
			return;

		//分配配置
		impl->config.reset(new(std::nothrow) btDefaultCollisionConfiguration());
		//分配窄阶段调度器
		if (impl->config)
			impl->dispatcher.reset(new(std::nothrow) btCollisionDispatcher(impl->config.get()));
		//分配宽阶段
		impl->broadphase.reset(new(std::nothrow) btDbvtBroadphase{});
		//分配碰撞世界
		if (impl->dispatcher && impl->broadphase)
			impl->world.reset(new(std::nothrow) btCollisionWorld(impl->dispatcher.get(),
				impl->broadphase.get(), impl->config.get()));

		//分配失败时释放已分配部件
		if (!impl->config || !impl->dispatcher || !impl->broadphase || !impl->world)
		{
			impl->world.reset();
			impl->broadphase.reset();
			impl->dispatcher.reset();
			impl->config.reset();
			return;
		}

		//安装宽阶段重叠过滤（豁免判定前移到窄阶段之前）
		impl->world->getPairCache()->setOverlapFilterCallback(&impl->overlap_filter);
	}

	//析构函数（先摘除全部已挂载对象，再释放碰撞世界）
	Region_Backend::~Region_Backend()
	{
		//实现体缺失时无需处理
		if (!impl)
			return;

		//逐个摘除已挂载对象并清空其所属空间（不回调本对象，避免析构期重入）
		for (Collider_Backend* backend : impl->attached)
		{
			//获取对象实现体
			Collider_Backend_Impl& inner = Collider_Backend_Access::impl_get(*backend);
			//逐个移出子部件对象
			for (Geometry_Part& part : inner.parts)
			{
				//无子部件对象的部件跳过
				if (!part.object)
					continue;
				//清空所属碰撞体
				part.object->owner = nullptr;
				//移出碰撞世界
				if (impl->world)
					impl->world->removeCollisionObject(&part.object->object);
			}
			//清空所属空间
			inner.owner = nullptr;
		}
		//清空挂载集合
		impl->attached.clear();

		//摘除空间边界
		Collider_Backend_Impl& boundary_inner = Collider_Backend_Access::impl_get(impl->boundary);
		for (Geometry_Part& part : boundary_inner.parts)
		{
			//无子部件对象的部件跳过
			if (!part.object)
				continue;
			//清空所属碰撞体
			part.object->owner = nullptr;
			//移出碰撞世界
			if (impl->world)
				impl->world->removeCollisionObject(&part.object->object);
		}
		//清空空间边界所属空间
		boundary_inner.owner = nullptr;
	}

	//移动构造
	Region_Backend::Region_Backend(Region_Backend&& other) noexcept
	{
		//搬移实现体
		impl = std::move(other.impl);
	}

	//移动赋值
	Region_Backend& Region_Backend::operator=(Region_Backend&& other) noexcept
	{
		//若为自身赋值则直接返回
		if (this == &other)
			return *this;
		//搬移实现体（原实现体由 unique_ptr 释放，析构语义与析构函数一致）
		impl = std::move(other.impl);
		return *this;
	}

	//空间有效性检查
	bool Region_Backend::valid(void) const
	{
		return impl && impl->config && impl->dispatcher && impl->broadphase && impl->world;
	}

	//空间边界构建
	bool Region_Backend::boundary_build(const std::string& mesh_path)
	{
		//空间后端不可用时无法构建
		if (!valid())
		{
			logger.warn("Region_Backend::碰撞空间后端不可用，空间边界无法构建");
			return false;
		}

		//边界几何配置（网格形状置于原点）
		nlohmann::json config = nlohmann::json::object();
		config["type"] = "mesh";
		config["mesh_path"] = mesh_path;
		config["position"] = nlohmann::json::array({ 0.0, 0.0, 0.0 });

		//先构建暂存几何（失败时保留原边界）
		if (!impl->boundary.geometry_build(config, Transform::getIdentity()))
		{
			logger.warn("Region_Backend::空间边界网格构建失败({})", mesh_path);
			return false;
		}

		//若旧边界已挂载则先摘除
		if (impl->boundary.geometry_mounted())
			object_detach(impl->boundary);
		//挂载新边界几何
		impl->boundary.geometry_mount();
		//边界恒为静态对象
		impl->boundary.static_flag_set(true);
		//边界身份标记（编号零恒保留给边界）
		impl->boundary.tag_set(0, true);
		//加入碰撞世界
		object_attach(impl->boundary);
		return true;
	}

	//空间边界卸载
	void Region_Backend::boundary_unload(void)
	{
		//实现体缺失时无需处理
		if (!impl)
			return;
		//若边界尚未挂载则直接返回
		if (!impl->boundary.geometry_mounted())
			return;
		//移出碰撞世界
		object_detach(impl->boundary);
		//卸载边界几何
		impl->boundary.geometry_clear();
	}

	//空间边界挂载状态
	bool Region_Backend::boundary_mounted(void) const
	{
		return impl && impl->boundary.geometry_mounted();
	}

	//对象挂载
	void Region_Backend::object_attach(Collider_Backend& backend)
	{
		//空间后端不可用时无法挂载
		if (!valid())
			return;
		//获取对象实现体
		Collider_Backend_Impl& inner = Collider_Backend_Access::impl_get(backend);
		//已挂载对象不重复加入
		if (inner.owner)
			return;
		//逐个加入子部件对象
		for (Geometry_Part& part : inner.parts)
		{
			//无子部件对象的部件跳过
			if (!part.object)
				continue;
			//写入身份标签
			part.object->collider_ID = inner.collider_ID;
			part.object->part_ID = part.part_ID;
			part.object->owner = &inner;
			//加入碰撞世界
			impl->world->addCollisionObject(&part.object->object);
		}
		//登记所属空间
		inner.owner = this;
		//登记挂载集合
		impl->attached.insert(&backend);
	}

	//对象摘除
	void Region_Backend::object_detach(Collider_Backend& backend)
	{
		//实现体缺失时无需处理
		if (!impl)
			return;
		//获取对象实现体
		Collider_Backend_Impl& inner = Collider_Backend_Access::impl_get(backend);
		//仅摘除本空间挂载的对象
		if (inner.owner != this)
			return;
		//逐个移出子部件对象
		for (Geometry_Part& part : inner.parts)
		{
			//无子部件对象的部件跳过
			if (!part.object)
				continue;
			//移出碰撞世界
			if (impl->world)
				impl->world->removeCollisionObject(&part.object->object);
			//清空所属碰撞体
			part.object->owner = nullptr;
		}
		//清空所属空间
		inner.owner = nullptr;
		//注销挂载集合
		impl->attached.erase(&backend);
	}

	//刷新世界包围盒
	void Region_Backend::aabbs_refresh(void)
	{
		//空间后端不可用时无需刷新
		if (!valid())
			return;
		impl->world->updateAabbs();
	}

	//对象是否已进入碰撞世界
	bool Region_Backend::object_contains(const Collider_Backend& backend) const
	{
		//空间后端不可用时视为未进入
		if (!valid())
			return false;

		//获取对象实现体（登记归属即表示已进入碰撞世界）
		const Collider_Backend_Impl& inner = Collider_Backend_Access::impl_get(backend);
		return inner.owner == this && !inner.parts.empty();
	}

	//扫掠检测收集
	void Region_Backend::swept_collect(const std::vector<Collider_Move>& moves,
		std::vector<Collision_Detail>& details,
		std::vector<uint64_t>& boundary_contacts) const
	{
		//空间后端不可用时无检测内容
		if (!valid())
			return;

		//逐个移动体执行扫掠
		for (const Collider_Move& move : moves)
		{
			//跳过未提供后端、无平移或含旋转/部件运动的移动体
			if (!move.backend || move.body_step.translation.length2() <= 0 ||
				!move.body_step.rotation.identity_check() || !move.part_steps.empty())
				continue;

			//分段段数
			uint32_t step_count = step_count_get(move);
			//单段位移
			Vector3 segment = move.body_step.translation / static_cast<Scalar>(step_count);

			//移动体实现体
			const Collider_Backend_Impl& mover = Collider_Backend_Access::impl_get(*move.backend);
			//移动体根变换
			const Transform collider_transform = move.backend->world_transform_get();
			//逐个几何体部件执行扫掠（仅凸包部件可扫掠）
			for (const Geometry_Part& part : mover.parts)
			{
				//仅处理已挂载且为凸包的部件
				if (!part.shape || !part.shape->isConvex() || !part.object)
					continue;

				//本部件扫掠起点变换（根变换 ∘ 部件相对变换）
				Transform sweep_from = collider_transform * part.local_transform;
				//逐段推进扫掠
				for (uint32_t step = 0; step < step_count; ++step)
				{
					//本段扫掠终点变换
					Transform sweep_to = sweep_from;
					sweep_to.getOrigin() += segment;

					//扫掠命中收集器
					Swept_Collector collector(&part.object->object);
					//执行本段凸包扫掠
					impl->world->convexSweepTest(static_cast<const btConvexShape*>(part.shape.get()),
						conversion::to_bt(sweep_from), conversion::to_bt(sweep_to), collector,
						impl->world->getDispatchInfo().m_allowedCcdPenetration);

					//汇总本段扫掠命中
					for (const Swept_Hit& hit : collector.hits)
					{
						//反查命中的子部件对象
						Geometry_Part_Object* hit_part =
							Collider_Backend_Access::part_recover(hit.object);
						//若无法反查则跳过
						if (!hit_part || !hit_part->owner)
							continue;
						//空间边界不属于碰撞体，只登记穿越编号
						if (hit_part->owner->is_boundary)
						{
							boundary_contacts.push_back(move.ID);
							continue;
						}

						/*
						豁免判定
						同一碰撞体的子部件按对称忽略对跳过；
						不同碰撞体的豁免标记同组非零时跳过。
						*/
						if (hit_part->owner == &mover)
						{
							//同一碰撞体：忽略对命中即跳过
							if (mover.part_ignored.count(
								part_pair_key(part.part_ID, hit_part->part_ID)) > 0)
								continue;
						}
						else if (mover.exemption_flag != 0 &&
							mover.exemption_flag == hit_part->owner->exemption_flag)
							continue;

						//归一化碰撞对
						Collision_Result pair = pair_normalize(move.ID, hit_part->collider_ID);
						//接触点（世界命中点与命中比例）
						Collision_Contact contact;
						//归一化后各侧子部件编号
						contact.part_A = (pair.collider_A == move.ID)
							? part.part_ID : hit_part->part_ID;
						contact.part_B = (pair.collider_A == move.ID)
							? hit_part->part_ID : part.part_ID;
						contact.point = conversion::to_engine(hit.point);
						contact.normal = conversion::to_engine(hit.normal);
						//法线须由碰撞体B 指向碰撞体A；移动体成为归一化B 时翻转
						if (pair.collider_B == move.ID)
							contact.normal = contact.normal * -1.0f;
						//扫掠检测无穿透深度
						contact.depth = 0.0f;
						contact.fraction = hit.fraction;
						//并入碰撞详情
						detail_merge(details, pair, contact);
					}

					//推进到本段末端
					sweep_from = sweep_to;
				}
			}
		}
	}

	//离散检测执行
	void Region_Backend::discrete_run(const std::vector<Collider_Move>& moves,
		std::vector<Collision_Detail>& details, std::vector<uint64_t>& boundary_contacts)
	{
		//空间后端不可用时无检测内容
		if (!valid())
			return;

		//最大分段段数（各移动体段数不一，统一按最大段数推进；段数耗尽的移动体不再移动）
		uint32_t max_step_count = 1;
		for (const Collider_Move& move : moves)
		{
			//分段段数
			uint32_t step_count = step_count_get(move);
			//取最大段数
			if (step_count > max_step_count)
				max_step_count = step_count;
		}

		//逐段推进并逐段执行离散碰撞检测
		for (uint32_t step = 0; step < max_step_count; ++step)
		{
			//本段内逐移动体就地平移
			for (const Collider_Move& move : moves)
			{
				//跳过未提供后端的移动体
				if (!move.backend)
					continue;
				//分段段数
				uint32_t step_count = step_count_get(move);
				//该移动体的段数已耗尽则不再推进
				if (step >= step_count)
					continue;
				//单段整体运动
				Motion segment = move.body_step / static_cast<Scalar>(step_count);
				//就地推进整体运动（绕自身原点旋转并按世界轴平移）
				if (!segment.empty())
					move.backend->motion_advance(segment);
				//逐部件推进局部运动
				for (const Part_Motion& part_step : move.part_steps)
				{
					//单段部件运动
					Motion part_segment = part_step.motion / static_cast<Scalar>(step_count);
					if (!part_segment.empty())
						move.backend->part_motion_advance(part_step.part, part_segment);
				}
			}

			//执行本段离散碰撞检测
			impl->world->performDiscreteCollisionDetection();

			//本段碰撞流形数量
			int manifold_count = impl->dispatcher->getNumManifolds();
			//逐个流形提取碰撞对
			for (int i = 0; i < manifold_count; ++i)
			{
				//目标流形
				btPersistentManifold* manifold = impl->dispatcher->getManifoldByIndexInternal(i);
				//无接触点则跳过
				if (manifold->getNumContacts() == 0)
					continue;

				//反查流形两侧子部件对象
				Geometry_Part_Object* part_A =
					Collider_Backend_Access::part_recover(manifold->getBody0());
				Geometry_Part_Object* part_B =
					Collider_Backend_Access::part_recover(manifold->getBody1());
				//若任一侧无法反查则跳过
				if (!part_A || !part_B || !part_A->owner || !part_B->owner)
					continue;

				//与空间边界接触：登记接触并跳过碰撞对（空间边界不属于碰撞体）
				if (part_A->owner->is_boundary || part_B->owner->is_boundary)
				{
					//接触对中位于空间边界另一侧的对象
					Geometry_Part_Object* contacted =
						part_A->owner->is_boundary ? part_B : part_A;
					//登记该对象与空间边界的接触
					boundary_contacts.push_back(contacted->collider_ID);
					continue;
				}

				//归一化碰撞对
				Collision_Result pair = pair_normalize(part_A->collider_ID, part_B->collider_ID);
				//逐个接触点提取世界位置、法线与穿透深度
				for (int j = 0; j < manifold->getNumContacts(); ++j)
				{
					//接触点
					const btManifoldPoint& contact_point = manifold->getContactPoint(j);
					//接触点（两侧世界位置的中点）
					btVector3 point = (contact_point.m_positionWorldOnA +
						contact_point.m_positionWorldOnB) * 0.5f;
					//世界法线（由流形第二侧指向第一侧）
					btVector3 normal = contact_point.m_normalWorldOnB;
					//法线须由碰撞体B 指向碰撞体A；流形第一侧成为归一化B 时翻转
					if (pair.collider_B == part_A->collider_ID)
						normal = normal * -1.0f;

					//接触记录
					Collision_Contact contact;
					//归一化后各侧子部件编号
					contact.part_A = (pair.collider_A == part_A->collider_ID)
						? part_A->part_ID : part_B->part_ID;
					contact.part_B = (pair.collider_A == part_A->collider_ID)
						? part_B->part_ID : part_A->part_ID;
					contact.point = conversion::to_engine(point);
					contact.normal = conversion::to_engine(normal);
					//穿透深度（分离距离钳零）
					contact.depth = (contact_point.m_distance1 < 0.0f)
						? -contact_point.m_distance1 : 0.0f;
					//离散检测无扫掠比例
					contact.fraction = 1.0f;
					//并入碰撞详情
					detail_merge(details, pair, contact);
				}
			}
		}
	}

	//空间边界包含性判定
	bool Region_Backend::point_inside(const Vector3& point) const
	{
		//空间边界尚未构建时无从判定
		if (!boundary_mounted())
			return false;

		//空间边界包围盒
		Vector3 aabb_min, aabb_max;
		//读取空间边界包围盒
		impl->boundary.aabb_get(aabb_min, aabb_max);
		/*
		探针射线方向
		取三轴均非零的固定方向，避免射线与边界面片共面或平行导致漏检；
		包含性判定只取决于与边界交点数的奇偶性，与射线方向无关。
		*/
		const Vector3 probe_direction = Vector3(1.0f, 0.37f, 0.19f).normalized();
		//射线终点（取包围盒对角线两倍长度，保证射线足以穿出空间边界）
		Vector3 ray_to = point + probe_direction * ((aabb_max - aabb_min).length() * 2.0f + 1.0f);

		//全命中射线回调（收集射线沿途的全部交点）
		btCollisionWorld::AllHitsRayResultCallback callback(conversion::to_bt(point), conversion::to_bt(ray_to));
		//执行射线检测
		impl->world->rayTest(conversion::to_bt(point), conversion::to_bt(ray_to), callback);

		//空间边界对象
		const btCollisionObject& boundary_object = Collider_Backend_Access::object_get(impl->boundary);
		//射线与空间边界的交点数
		int boundary_hits = 0;
		//逐个比对命中对象（子弹库数组无迭代器，按索引遍历）
		for (int i = 0; i < callback.m_collisionObjects.size(); ++i)
			//仅统计空间边界自身的交点
			if (callback.m_collisionObjects[i] == &boundary_object)
				++boundary_hits;

		//交点数为奇数说明探针点位于空间边界内部
		return (boundary_hits % 2) == 1;
	}
}
