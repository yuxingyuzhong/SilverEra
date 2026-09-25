#include "../局部命名空间使用.h"
//获取数据校验器
#include "src/tools/Data_Validator/数据校验器.h"
//获取日志系统
#include "src/tools/Logging/日志系统.h"
//获取网格加载器
#include "src/tools/Mesh_Loader/网格加载器.h"

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

		/*
		内存分配
		子弹库的形状类通过 BT_DECLARE_ALIGNED_ALLOCATOR 只声明了抛异常形式的内存分配算子，
		无法使用 new(nothrow)；此处统一包成失败返回，保持本模块的错误处理约定。
		*/
		template <typename Object_Type, typename Receiver_Type, typename... Args>
		bool memory_malloc(unique_ptr<Receiver_Type>& receiver, Args&&... args)
		{
			try
			{
				//构造目标对象
				unique_ptr<Object_Type> object(new Object_Type(std::forward<Args>(args)...));
				//移交所有权
				receiver = std::move(object);
				return true;
			}
			catch (const std::exception&)
			{
				Log::warn("Collision_Region::内存分配失败");
				return false;
			}
		}
	}

	//默认构造
	Collision_Region::Collision_Region() : Collision_Region(string())
	{
	}

	//含参构造(指定碰撞空间名称)
	Collision_Region::Collision_Region(const string& region_name)
	{
		//记录碰撞空间名称
		name = region_name;
		//预分配空间边界编号（编号0恒保留给空间边界）
		region_boundary.ID = ID_allocator.get();
	}

	//析构函数
	Collision_Region::~Collision_Region()
	{
		//若碰撞世界不可用则无需摘除
		if (!backend.world)
			return;

		//摘除空间边界碰撞对象
		if (region_boundary.shape)
			backend.world->removeCollisionObject(&region_boundary.object);
		//摘除全部碰撞体碰撞对象（碰撞体先于碰撞世界析构）
		for (auto& pair : mapping)
			backend.world->removeCollisionObject(&pair.second.object);
	}

	//碰撞空间有效性检查
	bool Collision_Region::valid(void) const
	{
		return backend.valid();
	}

	//碰撞空间状态设置
	void Collision_Region::state_set(bool active)
	{
		is_active = active;
	}

	//碰撞空间状态查看
	bool Collision_Region::state_check() const
	{
		return is_active;
	}

	//边界构建
	bool Collision_Region::boundary_build(const string& mesh_path)
	{
		//空间有效性检查
		if (!valid())
		{
			Log::warn("Collision_Region::碰撞空间后端不可用，空间边界无法构建");
			return false;
		}

		//网格形状与其数据源
		unique_ptr<Collision_Shape> shape;
		unique_ptr<Triangle_Mesh> mesh;
		//构建网格形状
		if (!mesh_shape_build(mesh_path, shape, mesh))
		{
			Log::warn("Collision_Region::空间边界网格构建失败({})", mesh_path);
			return false;
		}

		//若已存在空间边界则先卸载
		boundary_unload();

		//挂载边界形状与网格数据
		region_boundary.mesh = std::move(mesh);
		region_boundary.shape = std::move(shape);
		region_boundary.object.setCollisionShape(region_boundary.shape.get());
		//空间边界恒为静态对象
		region_boundary.object.setCollisionFlags(Static_Object_Flag);
		//空间边界置于原点
		region_boundary.object.setWorldTransform(Transform::getIdentity());
		//空间边界加入碰撞世界
		backend.world->addCollisionObject(&region_boundary.object);
		return true;
	}

	//边界卸载
	void Collision_Region::boundary_unload(void)
	{
		//若空间边界尚未构建
		if (!region_boundary.shape)
			return;

		//摘除空间边界碰撞对象
		backend.world->removeCollisionObject(&region_boundary.object);
		//卸载空间边界形状与网格数据
		region_boundary.object.setCollisionShape(nullptr);
		region_boundary.shape.reset();
		region_boundary.mesh.reset();
	}

	//碰撞体构建
	uint64_t Collision_Region::collider_build(void)
	{
		//分配碰撞体编号
		uint64_t collider_ID = ID_allocator.get();
		//跳过已被接管的编号（空间间转移会占用本空间的编号）
		while (mapping.count(collider_ID))
			collider_ID = ID_allocator.get();

		//登记碰撞体
		mapping[collider_ID].ID = collider_ID;
		//返回碰撞体编号
		return collider_ID;
	}

	//碰撞体接管(按指定编号登记，供空间间转移使用)
	bool Collision_Region::collider_adopt(uint64_t collider_ID)
	{
		//若该编号已被占用
		if (mapping.count(collider_ID))
		{
			Log::warn("Collision_Region::待接管碰撞体编号({})已被占用", collider_ID);
			return false;
		}

		//登记碰撞体
		mapping[collider_ID].ID = collider_ID;
		return true;
	}

	//碰撞体卸载
	bool Collision_Region::collider_unload(uint64_t collider_ID)
	{
		//查找目标碰撞体
		Collider* target = collider_seek(collider_ID);
		//若目标碰撞体不存在
		if (!target)
		{
			Log::warn("Collision_Region::待卸载碰撞体({})不存在", collider_ID);
			return false;
		}

		//若目标碰撞体已挂载形状
		if (target->shape)
		{
			//摘除碰撞对象
			backend.world->removeCollisionObject(&target->object);
			//卸载形状与网格数据
			target->object.setCollisionShape(nullptr);
			target->shape.reset();
			target->mesh.reset();
		}

		//注销碰撞体
		mapping.erase(collider_ID);
		//回收碰撞体编号
		ID_allocator.recycle(collider_ID);
		return true;
	}

	//碰撞体设置 —— 位移向量重载
	bool Collision_Region::collider_set(const uint64_t collider_ID, const Vector3& vector)
	{
		//查找目标碰撞体
		Collider* target = collider_seek(collider_ID);
		//若目标碰撞体不存在
		if (!target)
		{
			Log::warn("Collision_Region::待设置碰撞体({})不存在", collider_ID);
			return false;
		}

		//写入位移向量
		target->displacement_vector = vector;
		return true;
	}

	//碰撞体设置 —— 检测方式重载
	bool Collision_Region::collider_set(const uint64_t collider_ID, const Detection_Mode& vector)
	{
		//查找目标碰撞体
		Collider* target = collider_seek(collider_ID);
		//若目标碰撞体不存在
		if (!target)
		{
			Log::warn("Collision_Region::待设置碰撞体({})不存在", collider_ID);
			return false;
		}

		//写入检测方式
		target->detection_mode = vector;
		return true;
	}

	//碰撞体设置 —— 豁免标记重载
	bool Collision_Region::collider_set(const uint64_t collider_ID, const uint64_t& vector)
	{
		//查找目标碰撞体
		Collider* target = collider_seek(collider_ID);
		//若目标碰撞体不存在
		if (!target)
		{
			Log::warn("Collision_Region::待设置碰撞体({})不存在", collider_ID);
			return false;
		}

		//写入豁免标记
		target->exemption_flag = vector;
		return true;
	}

	//碰撞体设置 —— 几何体添加
	bool Collision_Region::collider_set(nlohmann::json geometry_config)
	{
		//几何配置格式检查
		if (!geometry_config.is_object())
		{
			Log::warn("Collision_Region::几何配置非对象格式");
			return false;
		}
		//目标碰撞体编号字段检查
		if (!Data_Validator::field_check<uint64_t>(geometry_config, "collider_ID"))
		{
			Log::warn("Collision_Region::几何配置缺少有效字段(collider_ID)");
			return false;
		}
		//提取目标碰撞体编号
		uint64_t collider_ID = geometry_config["collider_ID"].get<uint64_t>();
		//查找目标碰撞体
		Collider* target = collider_seek(collider_ID);
		//若目标碰撞体不存在
		if (!target)
		{
			Log::warn("Collision_Region::待设置碰撞体({})不存在", collider_ID);
			return false;
		}

		//构建几何形状与其网格数据源
		unique_ptr<Collision_Shape> shape;
		unique_ptr<Triangle_Mesh> mesh;
		if (!shape_build(geometry_config, shape, mesh))
		{
			Log::warn("Collision_Region::碰撞体({})几何配置非法", collider_ID);
			return false;
		}

		//世界变换基准（未指定位置或旋转时沿用当前变换）
		Transform transform = target->object.getWorldTransform();

		//若指定了初始位置
		if (geometry_config.contains("position"))
		{
			//初始位置
			Vector3 position;
			//读取初始位置
			if (!vector_read(geometry_config, "position", position))
			{
				Log::warn("Collision_Region::碰撞体({})字段(position)非法", collider_ID);
				return false;
			}
			//写入初始位置
			transform.setOrigin(position);
		}
		//若指定了初始旋转
		if (geometry_config.contains("rotation"))
		{
			//初始旋转
			Quaternion rotation;
			//读取初始旋转
			if (!quaternion_read(geometry_config, "rotation", rotation))
			{
				Log::warn("Collision_Region::碰撞体({})字段(rotation)非法", collider_ID);
				return false;
			}
			//写入初始旋转
			transform.setRotation(rotation);
		}

		//若目标碰撞体已挂载形状则先摘除
		if (target->shape)
		{
			//摘除碰撞对象
			backend.world->removeCollisionObject(&target->object);
			//卸载形状与网格数据
			target->object.setCollisionShape(nullptr);
			target->shape.reset();
			target->mesh.reset();
		}

		//挂载几何形状与网格数据
		target->mesh = std::move(mesh);
		target->shape = std::move(shape);
		target->object.setCollisionShape(target->shape.get());
		//写入世界变换
		target->object.setWorldTransform(transform);
		//几何配置留档（供空间间镜像与转移复用）
		target->geometry = geometry_config;
		//加入碰撞世界
		backend.world->addCollisionObject(&target->object);
		return true;
	}

	//碰撞体查询
	bool Collision_Region::collider_find(uint64_t collider_id) const
	{
		//按登记情况判定
		return mapping.count(collider_id) > 0;
	}

	//碰撞体全量获取
	std::vector<uint64_t> Collision_Region::colliders(void)
	{
		//碰撞体编号集合
		vector<uint64_t> collider_IDs;
		//预留容量
		collider_IDs.reserve(mapping.size());
		//逐个收集编号
		for (const auto& [collider_ID, collider] : mapping)
			collider_IDs.push_back(collider_ID);

		return collider_IDs;
	}

	//碰撞体几何配置读取
	nlohmann::json Collision_Region::collider_geometry(uint64_t collider_ID) const
	{
		//查找目标碰撞体
		auto it = mapping.find(collider_ID);
		//若目标碰撞体不存在
		if (it == mapping.end())
			return nlohmann::json{};

		//返回几何配置副本
		return it->second.geometry;
	}

	//碰撞体包含性检测
	bool Collision_Region::contains(uint64_t collider_ID) const
	{
		//查找目标碰撞体
		auto it = mapping.find(collider_ID);
		//若目标碰撞体未登记
		if (it == mapping.end())
			return false;

		//碰撞世界内碰撞对象数量
		int object_count = backend.world->getNumCollisionObjects();
		//逐个比对碰撞世界内的碰撞对象
		for (int i = 0; i < object_count; ++i)
			//若命中目标碰撞对象
			if (backend.world->getCollisionObjectArray()[i] == &it->second.object)
				return true;

		//目标碰撞体尚未进入碰撞世界
		return false;
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

	//碰撞体查找
	Collider* Collision_Region::collider_seek(uint64_t collider_ID)
	{
		//查找目标碰撞体
		auto it = mapping.find(collider_ID);
		//若目标碰撞体不存在
		if (it == mapping.end())
			return nullptr;

		return &it->second;
	}

	//浮点字段读取
	bool Collision_Region::scalar_read(const nlohmann::json& config, const std::string& field,
		double& receiver)
	{
		//浮点格式检查
		if (Data_Validator::field_check<double>(config, field))
			receiver = config[field].get<double>();
		//整数格式检查（整数可作浮点使用）
		else if (Data_Validator::field_check<int64_t>(config, field))
			receiver = static_cast<double>(config[field].get<int64_t>());
		//其余格式一律判为非法
		else
			return false;

		return true;
	}

	//三元数组字段读取
	bool Collision_Region::vector_read(const nlohmann::json& config, const std::string& field,
		Vector3& receiver)
	{
		//数组格式检查
		if (!Data_Validator::field_check<std::vector<double>>(config, field))
			return false;

		//读取数组内容
		vector<double> values = config[field].get<std::vector<double>>();
		//数组长度检查
		if (values.size() != 3)
		{
			Log::warn("Collision_Region::字段 {} 数组长度非三", field);
			return false;
		}

		//写入矢量
		receiver.setValue(values[0], values[1], values[2]);
		return true;
	}

	//四元数组字段读取
	bool Collision_Region::quaternion_read(const nlohmann::json& config, const std::string& field,
		Quaternion& receiver)
	{
		//数组格式检查
		if (!Data_Validator::field_check<std::vector<double>>(config, field))
			return false;

		//读取数组内容
		vector<double> values = config[field].get<std::vector<double>>();
		//数组长度检查
		if (values.size() != 4)
		{
			Log::warn("Collision_Region::字段 {} 数组长度非四", field);
			return false;
		}

		//写入四元数（按 x、y、z、w 顺序解释）
		receiver.setValue(values[0], values[1], values[2], values[3]);
		return true;
	}

	//网格形状构建
	bool Collision_Region::mesh_shape_build(const string& mesh_path,
		std::unique_ptr<Collision_Shape>& shape_receiver,
		std::unique_ptr<Triangle_Mesh>& mesh_receiver)
	{
		//网格数据
		Mesh_Data mesh_data;
		//加载网格文件
		if (!Mesh_Loader::load_obj(mesh_path, mesh_data))
			return false;

		//分配三角形网格内存（32 位索引 + btVector3 顶点）
		if (!memory_malloc<Triangle_Mesh>(mesh_receiver, true, true))
			return false;

		//预留顶点与索引容量
		mesh_receiver->preallocateVertices(static_cast<int>(mesh_data.vertices.size() / 3));
		mesh_receiver->preallocateIndices(static_cast<int>(mesh_data.indices.size()));

		/*
		写入顶点坐标
		findOrAddVertex 在关闭查重时按调用顺序追加顶点并返回其顺序下标，
		故按索引顺序写入后，顶点数组下标与网格数据下标一一对应。
		*/
		for (size_t i = 0; i + 2 < mesh_data.vertices.size(); i += 3)
			mesh_receiver->findOrAddVertex(Vector3(mesh_data.vertices[i], mesh_data.vertices[i + 1],
				mesh_data.vertices[i + 2]), false);
		//写入三角面索引（顶点须先行写入）
		for (size_t i = 0; i + 2 < mesh_data.indices.size(); i += 3)
			mesh_receiver->addTriangleIndices(mesh_data.indices[i], mesh_data.indices[i + 1],
				mesh_data.indices[i + 2]);

		//分配BVH三角形网格形状（数据源由调用方一并持有）
		if (!memory_malloc<Bvh_Triangle_Mesh>(shape_receiver, mesh_receiver.get(), true, true))
		{
			mesh_receiver.reset();
			return false;
		}

		return true;
	}

	//几何形状构建
	bool Collision_Region::shape_build(const nlohmann::json& geometry_config,
		std::unique_ptr<Collision_Shape>& shape_receiver,
		std::unique_ptr<Triangle_Mesh>& mesh_receiver)
	{
		//形状类型字段检查
		if (!Data_Validator::field_check<std::string>(geometry_config, "type"))
		{
			Log::warn("Collision_Region::几何配置缺少有效字段(type)");
			return false;
		}
		//形状类型
		string shape_type = geometry_config["type"].get<std::string>();

		//---------- 盒体 ----------
		if (shape_type == "box")
		{
			//盒体半长
			Vector3 half_extent;
			//读取盒体半长
			if (!vector_read(geometry_config, "half_extent", half_extent))
			{
				Log::warn("Collision_Region::盒体缺少有效字段(half_extent)");
				return false;
			}
			//半长取值检查
			if (half_extent.getX() <= 0 || half_extent.getY() <= 0 || half_extent.getZ() <= 0)
			{
				Log::warn("Collision_Region::盒体半长非正");
				return false;
			}
			//分配盒体形状
			if (!memory_malloc<Box_Shape>(shape_receiver, half_extent))
				return false;
		}
		//---------- 球体 ----------
		else if (shape_type == "sphere")
		{
			//球体半径
			double radius = 0.0;
			//读取球体半径
			if (!scalar_read(geometry_config, "radius", radius) || radius <= 0)
			{
				Log::warn("Collision_Region::球体缺少有效字段(radius)");
				return false;
			}
			//分配球体形状
			if (!memory_malloc<Sphere_Shape>(shape_receiver, static_cast<Scalar>(radius)))
				return false;
		}
		//---------- 胶囊 ----------
		else if (shape_type == "capsule")
		{
			//胶囊半径与全高
			double radius = 0.0, height = 0.0;
			//读取胶囊半径
			if (!scalar_read(geometry_config, "radius", radius) || radius <= 0)
			{
				Log::warn("Collision_Region::胶囊缺少有效字段(radius)");
				return false;
			}
			//读取胶囊全高
			if (!scalar_read(geometry_config, "height", height) || height <= 0)
			{
				Log::warn("Collision_Region::胶囊缺少有效字段(height)");
				return false;
			}
			//全高须容纳两端半球
			if (height < 2.0 * radius)
			{
				Log::warn("Collision_Region::胶囊全高({})不足以容纳两端半球", height);
				return false;
			}
			//分配胶囊形状（沿 Y 轴，子弹库第二参数为圆柱段长度）
			if (!memory_malloc<Capsule_Shape>(shape_receiver, static_cast<Scalar>(radius),
				static_cast<Scalar>(height - 2.0 * radius)))
				return false;
		}
		//---------- 圆柱 ----------
		else if (shape_type == "cylinder")
		{
			//圆柱半径与全高
			double radius = 0.0, height = 0.0;
			//读取圆柱半径
			if (!scalar_read(geometry_config, "radius", radius) || radius <= 0)
			{
				Log::warn("Collision_Region::圆柱缺少有效字段(radius)");
				return false;
			}
			//读取圆柱全高
			if (!scalar_read(geometry_config, "height", height) || height <= 0)
			{
				Log::warn("Collision_Region::圆柱缺少有效字段(height)");
				return false;
			}
			//分配圆柱形状（沿 Y 轴，子弹库以半长描述）
			if (!memory_malloc<Cylinder_Shape>(shape_receiver,
				Vector3(static_cast<Scalar>(radius), static_cast<Scalar>(height * 0.5),
					static_cast<Scalar>(radius))))
				return false;
		}
		//---------- 圆锥 ----------
		else if (shape_type == "cone")
		{
			//圆锥半径与全高
			double radius = 0.0, height = 0.0;
			//读取圆锥半径
			if (!scalar_read(geometry_config, "radius", radius) || radius <= 0)
			{
				Log::warn("Collision_Region::圆锥缺少有效字段(radius)");
				return false;
			}
			//读取圆锥全高
			if (!scalar_read(geometry_config, "height", height) || height <= 0)
			{
				Log::warn("Collision_Region::圆锥缺少有效字段(height)");
				return false;
			}
			//分配圆锥形状（沿 Y 轴）
			if (!memory_malloc<Cone_Shape>(shape_receiver, static_cast<Scalar>(radius),
				static_cast<Scalar>(height)))
				return false;
		}
		//---------- 网格 ----------
		else if (shape_type == "mesh")
		{
			//网格路径字段检查
			if (!Data_Validator::field_check<std::string>(geometry_config, "mesh_path"))
			{
				Log::warn("Collision_Region::网格缺少有效字段(mesh_path)");
				return false;
			}
			//构建网格形状
			if (!mesh_shape_build(geometry_config["mesh_path"].get<std::string>(), shape_receiver,
				mesh_receiver))
			{
				Log::warn("Collision_Region::网格形状构建失败");
				return false;
			}
		}
		//---------- 未知形状类型 ----------
		else
		{
			Log::warn("Collision_Region::未知形状类型({})", shape_type);
			return false;
		}

		//碰撞边距（缺省沿用子弹库默认边距）
		double margin = 0.04;
		//若显式指定了碰撞边距
		if (geometry_config.contains("margin"))
		{
			//读取碰撞边距
			if (!scalar_read(geometry_config, "margin", margin) || margin < 0)
			{
				Log::warn("Collision_Region::字段(margin)非法");
				shape_receiver.reset();
				mesh_receiver.reset();
				return false;
			}
		}
		//下发碰撞边距（盒体、圆柱、胶囊会自行修正隐式尺寸以保持总体尺寸不变）
		shape_receiver->setMargin(static_cast<Scalar>(margin));
		return true;
	}

}