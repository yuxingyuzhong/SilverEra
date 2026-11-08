#include "../../局部命名空间使用.h"
//获取碰撞体后端声明
#include "src/collision/Collider/backend/碰撞体后端.h"
//获取碰撞体后端实现体与访问器
#include "src/collision/Collider/backend/碰撞体后端_内部.h"
//获取几何类型转换
#include "src/collision/Common/数学转换.h"
//获取碰撞空间后端声明（析构自动摘除需要）
#include "src/collision/Collision_Region/backend/空间后端.h"
//获取数据校验器
#include "Engine/EngineFoundation/src/detail/methods/package/数据校验工具.h"
//获取日志系统
#include "Engine/EngineFoundation/src/log/日志系统运行包.h"
//获取网格加载器
#include "Engine/EngineFoundation/src/loader/assets/Mesh_Loader/网格加载器.h"
//获取碰撞配置字段读取工具
#include "src/collision/Common/字段读取.h"

namespace engine
{
	//引入字段读取工具
	using namespace collision_detail;

	//实现体定义（沿用内部头的通用实现体，供公开句柄持有）
	struct Collider_Backend::Impl : Collider_Backend_Impl
	{
	};

	//文件内部辅助设施（不对外暴露）
	namespace
	{
		//子部件编号上限（0xFFFF 保留为未配置哨兵）
		constexpr uint64_t part_ID_limit = 0xFFFF;

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
				logger.warn("Collider_Backend::内存分配失败");
				return false;
			}
		}

		//网格形状构建
		bool mesh_shape_build(const string& mesh_path,
			std::unique_ptr<btCollisionShape>& shape_receiver,
			std::unique_ptr<btTriangleMesh>& mesh_receiver)
		{
			//网格数据
			Mesh_Data mesh_data;
			//加载网格文件
			if (!Mesh_Loader::load_obj(mesh_path, mesh_data))
				return false;

			//分配三角形网格内存（32 位索引 + 顶点坐标）
			if (!memory_malloc<btTriangleMesh>(mesh_receiver, true, true))
				return false;

			//预留顶点与索引容量（顶点已按组合去重）
			mesh_receiver->preallocateVertices(static_cast<int>(mesh_data.vertices.size()));
			mesh_receiver->preallocateIndices(static_cast<int>(mesh_data.indices.size()));

			/*
			写入顶点坐标
			关闭查重时按调用顺序追加顶点并返回其顺序下标，
			故按顶点数组顺序写入后，顶点数组下标与网格数据下标一一对应。
			*/
			for (const Vertex& vertex : mesh_data.vertices)
				mesh_receiver->findOrAddVertex(conversion::to_bt(Vector3(vertex.position[0],
					vertex.position[1], vertex.position[2])), false);
			//写入三角面索引（顶点须先行写入）
			for (size_t i = 0; i + 2 < mesh_data.indices.size(); i += 3)
				mesh_receiver->addTriangleIndices(mesh_data.indices[i], mesh_data.indices[i + 1],
					mesh_data.indices[i + 2]);

			//分配BVH三角形网格形状（数据源由调用方一并持有）
			if (!memory_malloc<btBvhTriangleMeshShape>(shape_receiver, mesh_receiver.get(), true, true))
			{
				mesh_receiver.reset();
				return false;
			}

			return true;
		}

		//几何形状构建
		bool shape_build(const nlohmann::json& geometry_config,
			std::unique_ptr<btCollisionShape>& shape_receiver,
			std::unique_ptr<btTriangleMesh>& mesh_receiver)
		{
			//形状类型
			string shape_type;
			//形状类型字段检查
			if (!text_read(geometry_config, "type", shape_type))
			{
				logger.warn("Collider_Backend::几何配置缺少有效字段(type)");
				return false;
			}

			//---------- 盒体 ----------
			if (shape_type == "box")
			{
				//盒体半长
				Vector3 half_extent;
				//读取盒体半长
				if (!vector_read(geometry_config, "half_extent", half_extent))
				{
					logger.warn("Collider_Backend::盒体缺少有效字段(half_extent)");
					return false;
				}
				//半长取值检查
				if (half_extent.getX() <= 0 || half_extent.getY() <= 0 || half_extent.getZ() <= 0)
				{
					logger.warn("Collider_Backend::盒体半长非正");
					return false;
				}
				//分配盒体形状
				if (!memory_malloc<btBoxShape>(shape_receiver, conversion::to_bt(half_extent)))
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
					logger.warn("Collider_Backend::球体缺少有效字段(radius)");
					return false;
				}
				//分配球体形状
				if (!memory_malloc<btSphereShape>(shape_receiver, static_cast<Scalar>(radius)))
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
					logger.warn("Collider_Backend::胶囊缺少有效字段(radius)");
					return false;
				}
				//读取胶囊全高
				if (!scalar_read(geometry_config, "height", height) || height <= 0)
				{
					logger.warn("Collider_Backend::胶囊缺少有效字段(height)");
					return false;
				}
				//全高须容纳两端半球
				if (height < 2.0 * radius)
				{
					logger.warn("Collider_Backend::胶囊全高({})不足以容纳两端半球", height);
					return false;
				}
				//分配胶囊形状（沿 Y 轴，子弹库第二参数为圆柱段长度）
				if (!memory_malloc<btCapsuleShape>(shape_receiver, static_cast<Scalar>(radius),
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
					logger.warn("Collider_Backend::圆柱缺少有效字段(radius)");
					return false;
				}
				//读取圆柱全高
				if (!scalar_read(geometry_config, "height", height) || height <= 0)
				{
					logger.warn("Collider_Backend::圆柱缺少有效字段(height)");
					return false;
				}
				//圆柱半长矢量（沿 Y 轴，子弹库以半长描述）
				Vector3 cylinder_extent(static_cast<Scalar>(radius),
					static_cast<Scalar>(height * 0.5), static_cast<Scalar>(radius));
				//分配圆柱形状
				if (!memory_malloc<btCylinderShape>(shape_receiver, conversion::to_bt(cylinder_extent)))
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
					logger.warn("Collider_Backend::圆锥缺少有效字段(radius)");
					return false;
				}
				//读取圆锥全高
				if (!scalar_read(geometry_config, "height", height) || height <= 0)
				{
					logger.warn("Collider_Backend::圆锥缺少有效字段(height)");
					return false;
				}
				//分配圆锥形状（沿 Y 轴）
				if (!memory_malloc<btConeShape>(shape_receiver, static_cast<Scalar>(radius),
					static_cast<Scalar>(height)))
					return false;
			}
			//---------- 网格 ----------
			else if (shape_type == "mesh")
			{
				//网格路径
				string mesh_path;
				//网格路径字段检查
				if (!text_read(geometry_config, "mesh_path", mesh_path))
				{
					logger.warn("Collider_Backend::网格缺少有效字段(mesh_path)");
					return false;
				}
				//构建网格形状
				if (!mesh_shape_build(mesh_path, shape_receiver, mesh_receiver))
				{
					logger.warn("Collider_Backend::网格形状构建失败");
					return false;
				}
			}
			//---------- 未知形状类型 ----------
			else
			{
				logger.warn("Collider_Backend::未知形状类型({})", shape_type);
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
					logger.warn("Collider_Backend::字段(margin)非法");
					shape_receiver.reset();
					mesh_receiver.reset();
					return false;
				}
			}
			//下发碰撞边距（盒体、圆柱、胶囊会自行修正隐式尺寸以保持总体尺寸不变）
			shape_receiver->setMargin(static_cast<Scalar>(margin));
			return true;
		}

		//子部件编号分配（显式编号去重，缺省编号按未占用最小值依次分配）
		bool part_ID_assign(std::vector<Geometry_Part>& parts, std::unordered_set<uint16_t>& used)
		{
			//先收集显式编号
			for (const Geometry_Part& part : parts)
			{
				//缺省编号跳过
				if (part.part_ID == 0xFFFF)
					continue;
				//显式编号冲突
				if (!used.insert(part.part_ID).second)
				{
					logger.warn("Collider_Backend::几何配置子部件编号({})重复", part.part_ID);
					return false;
				}
			}

			//为缺省编号依次分配
			uint16_t next_ID = 0;
			for (Geometry_Part& part : parts)
			{
				//显式编号无需分配
				if (part.part_ID != 0xFFFF)
					continue;
				//跳过已占用编号
				while (used.count(next_ID))
				{
					//若编号耗尽
					if (next_ID >= 0xFFFE)
					{
						logger.warn("Collider_Backend::几何配置子部件编号已耗尽");
						return false;
					}
					++next_ID;
				}
				//写入分配结果
				part.part_ID = next_ID;
				used.insert(next_ID);
			}

			return true;
		}

		//几何体集合装配（单几何体形式与集合形式统一处理，每部件独立碰撞对象）
		bool geometry_assemble(const nlohmann::json& geometry_config,
			std::vector<Geometry_Part>& parts_receiver,
			std::unordered_set<uint64_t>& ignored_receiver)
		{
			//集合形式判定（含几何体数组字段即为几何体集合）
			bool is_geometry_set = geometry_config.contains("geometries") &&
				geometry_config["geometries"].is_array();

			//待构建的几何体部件集合
			std::vector<Geometry_Part> parts;

			//---------- 几何体集合形式：逐个构建带相对变换的部件 ----------
			if (is_geometry_set)
			{
				//逐个处理几何体元素
				for (const auto& item : geometry_config["geometries"])
				{
					//几何体元素格式检查
					if (!item.is_object())
					{
						logger.warn("Collider_Backend::几何体集合含非对象元素");
						return false;
					}

					//几何体部件
					Geometry_Part part;
					//构建该部件的形状与其网格数据源
					if (!shape_build(item, part.shape, part.mesh))
						return false;

					//若指定了子部件编号
					if (item.contains("part_ID"))
					{
						//子部件编号
						uint64_t part_ID = 0;
						//读取子部件编号
						if (!number_read(item, "part_ID", part_ID) || part_ID >= part_ID_limit)
						{
							logger.warn("Collider_Backend::几何体集合元素字段(part_ID)非法");
							return false;
						}
						//写入子部件编号
						part.part_ID = static_cast<uint16_t>(part_ID);
					}

					//若指定了相对位置
					if (item.contains("position"))
					{
						//相对位置
						Vector3 position;
						//读取相对位置
						if (!vector_read(item, "position", position))
						{
							logger.warn("Collider_Backend::几何体集合元素字段(position)非法");
							return false;
						}
						//写入部件相对位置
						part.local_transform.setOrigin(position);
					}
					//若指定了相对旋转
					if (item.contains("rotation"))
					{
						//相对旋转
						Quaternion rotation;
						//读取相对旋转
						if (!quaternion_read(item, "rotation", rotation))
						{
							logger.warn("Collider_Backend::几何体集合元素字段(rotation)非法");
							return false;
						}
						//写入部件相对旋转
						part.local_transform.setRotation(rotation);
					}

					//收容该部件
					parts.push_back(std::move(part));
				}

				//几何体集合不可为空
				if (parts.empty())
				{
					logger.warn("Collider_Backend::几何体集合为空");
					return false;
				}
			}
			//---------- 单几何体形式：视作仅含一个几何体的集合 ----------
			else
			{
				//几何体部件
				Geometry_Part part;
				//构建几何形状与其网格数据源
				if (!shape_build(geometry_config, part.shape, part.mesh))
					return false;
				//单几何体形式下基准位置与朝向由调用方写入根变换，部件相对变换保持单位变换
				if (geometry_config.contains("part_ID"))
				{
					//子部件编号
					uint64_t part_ID = 0;
					//读取子部件编号
					if (!number_read(geometry_config, "part_ID", part_ID) || part_ID >= part_ID_limit)
					{
						logger.warn("Collider_Backend::几何配置字段(part_ID)非法");
						return false;
					}
					//写入子部件编号
					part.part_ID = static_cast<uint16_t>(part_ID);
				}
				parts.push_back(std::move(part));
			}

			//---------- 子部件编号分配与校验 ----------
			std::unordered_set<uint16_t> used_IDs;
			if (!part_ID_assign(parts, used_IDs))
				return false;

			//---------- 子部件对称忽略对 ----------
			std::unordered_set<uint64_t> ignored;
			if (geometry_config.contains("part_ignore"))
			{
				//忽略对必须为数组
				const nlohmann::json& ignore_list = geometry_config["part_ignore"];
				if (!ignore_list.is_array())
				{
					logger.warn("Collider_Backend::字段(part_ignore)非数组");
					return false;
				}
				//逐对读取
				for (const auto& item : ignore_list)
				{
					//单对必须为二元数组
					if (!item.is_array() || item.size() != 2)
					{
						logger.warn("Collider_Backend::字段(part_ignore)含非法元素");
						return false;
					}
					//两侧子部件编号
					uint64_t part_ID[2] = { 0, 0 };
					//逐个读取
					for (size_t i = 0; i < 2; ++i)
					{
						//二元数组元素必须为整数
						if (!item[i].is_number_integer() ||
							item[i].get<int64_t>() < 0)
						{
							logger.warn("Collider_Backend::字段(part_ignore)元素非法");
							return false;
						}
						part_ID[i] = item[i].get<uint64_t>();
					}
					//编号越界
					if (part_ID[0] >= part_ID_limit || part_ID[1] >= part_ID_limit)
					{
						logger.warn("Collider_Backend::字段(part_ignore)编号越界");
						return false;
					}
					//忽略对两侧必须存在
					if (!used_IDs.count(static_cast<uint16_t>(part_ID[0])) ||
						!used_IDs.count(static_cast<uint16_t>(part_ID[1])))
					{
						logger.warn("Collider_Backend::字段(part_ignore)引用不存在的子部件");
						return false;
					}
					//自忽略对无意义
					if (part_ID[0] == part_ID[1])
					{
						logger.warn("Collider_Backend::字段(part_ignore)含自忽略对");
						return false;
					}
					//写入归一化键
					ignored.insert(part_pair_key(static_cast<uint16_t>(part_ID[0]),
						static_cast<uint16_t>(part_ID[1])));
				}
			}

			//---------- 子部件碰撞对象构建 ----------
			for (Geometry_Part& part : parts)
			{
				//分配子部件对象
				part.object.reset(new(std::nothrow) Geometry_Part_Object());
				//若分配失败
				if (!part.object)
				{
					logger.warn("Collider_Backend::子部件碰撞对象分配失败");
					return false;
				}
				//写入子部件编号
				part.object->part_ID = part.part_ID;
				//挂载形状（形状所有权仍由部件持有）
				part.object->object.setCollisionShape(part.shape.get());
			}

			//移交构建结果
			parts_receiver = std::move(parts);
			ignored_receiver = std::move(ignored);
			return true;
		}

		//子部件世界变换刷新（根变换与局部变换合成，并写入身份标签）
		void parts_refresh(Collider_Backend_Impl& impl)
		{
			//逐个部件刷新
			for (Geometry_Part& part : impl.parts)
			{
				//无对象或无形状的部件跳过
				if (!part.object || !part.shape)
					continue;
				//世界变换
				Transform world = impl.root_transform * part.local_transform;
				//写入世界变换
				part.object->object.setWorldTransform(conversion::to_bt(world));
				//写入身份标签
				part.object->collider_ID = impl.collider_ID;
				part.object->part_ID = part.part_ID;
				part.object->owner = &impl;
			}
		}

		//子部件编号映射重建
		void parts_index_rebuild(Collider_Backend_Impl& impl)
		{
			//清空旧映射
			impl.part_index.clear();
			//逐个写入映射
			for (uint32_t index = 0; index < impl.parts.size(); ++index)
				impl.part_index[impl.parts[index].part_ID] = index;
		}
	}

	//实现体获取
	Collider_Backend_Impl& Collider_Backend_Access::impl_get(class Collider_Backend& backend)
	{
		return *backend.impl;
	}

	//实现体获取（只读）
	const Collider_Backend_Impl& Collider_Backend_Access::impl_get(
		const class Collider_Backend& backend)
	{
		return *backend.impl;
	}

	//碰撞对象获取（返回首个子部件对象）
	btCollisionObject& Collider_Backend_Access::object_get(class Collider_Backend& backend)
	{
		return backend.impl->parts.front().object->object;
	}

	//碰撞对象获取（只读）
	const btCollisionObject& Collider_Backend_Access::object_get(const class Collider_Backend& backend)
	{
		return backend.impl->parts.front().object->object;
	}

	//部件集合获取
	std::vector<Geometry_Part>& Collider_Backend_Access::parts_get(class Collider_Backend& backend)
	{
		return backend.impl->parts;
	}

	//构造函数
	Collider_Backend::Collider_Backend()
	{
		//分配实现体内存
		impl.reset(new(nothrow) Impl());
	}

	//析构函数（若仍挂载于碰撞空间则自动摘除）
	Collider_Backend::~Collider_Backend()
	{
		//实现体缺失时无需处理
		if (!impl)
			return;
		//若仍挂载于碰撞空间则通知其摘除
		if (impl->owner)
			impl->owner->object_detach(*this);
	}

	//移动构造（实现体地址保持不变）
	Collider_Backend::Collider_Backend(Collider_Backend&& other) noexcept
	{
		//搬移实现体
		impl = std::move(other.impl);
	}

	//移动赋值（实现体地址保持不变）
	Collider_Backend& Collider_Backend::operator=(Collider_Backend&& other) noexcept
	{
		//若为自身赋值则直接返回
		if (this == &other)
			return *this;
		//先释放自身实现体（析构语义与析构函数一致）
		if (impl && impl->owner)
			impl->owner->object_detach(*this);
		//搬移实现体
		impl = std::move(other.impl);
		return *this;
	}

	//身份标签设置
	void Collider_Backend::tag_set(uint64_t collider_ID, bool is_boundary)
	{
		//实现体缺失时无法设置
		if (!impl)
			return;
		//写入标签
		impl->collider_ID = collider_ID;
		impl->is_boundary = is_boundary;
		//同步到已构建的子部件对象
		parts_refresh(*impl);
	}

	//身份标签读取
	uint64_t Collider_Backend::tag_ID(void) const
	{
		return impl ? impl->collider_ID : 0;
	}

	//空间边界判定
	bool Collider_Backend::boundary_check(void) const
	{
		return impl ? impl->is_boundary : false;
	}

	//豁免标记设置
	void Collider_Backend::exemption_flag_set(uint64_t flag)
	{
		//实现体缺失时无法设置
		if (!impl)
			return;
		//写入豁免标记
		impl->exemption_flag = flag;
	}

	//豁免标记读取
	uint64_t Collider_Backend::exemption_flag_get(void) const
	{
		return impl ? impl->exemption_flag : 0;
	}

	//子部件对称忽略对设置
	bool Collider_Backend::part_ignore_set(const std::vector<std::pair<uint16_t, uint16_t>>& pairs)
	{
		//实现体缺失时无法设置
		if (!impl)
			return false;

		//忽略对集合
		std::unordered_set<uint64_t> ignored;
		//逐对校验
		for (const auto& pair : pairs)
		{
			//两侧子部件必须存在
			if (!impl->part_index.count(pair.first) || !impl->part_index.count(pair.second))
			{
				logger.warn("Collider_Backend::忽略对引用的子部件({}/{})不存在",
					pair.first, pair.second);
				return false;
			}
			//写入归一化键
			ignored.insert(part_pair_key(pair.first, pair.second));
		}

		//写回忽略集合
		impl->part_ignored = std::move(ignored);
		return true;
	}

	//几何构建（暂存）
	bool Collider_Backend::geometry_build(const nlohmann::json& geometry_config,
		const Transform& base_transform)
	{
		//实现体缺失时无法构建
		if (!impl)
			return false;

		//暂存部件与忽略对
		std::vector<Geometry_Part> pending_parts;
		std::unordered_set<uint64_t> pending_ignored;
		//装配几何
		if (!geometry_assemble(geometry_config, pending_parts, pending_ignored))
		{
			//构建失败时清除暂存
			impl->pending_parts.clear();
			impl->pending_part_ignored.clear();
			impl->pending_valid = false;
			return false;
		}

		//写入暂存区
		impl->pending_parts = std::move(pending_parts);
		impl->pending_part_ignored = std::move(pending_ignored);
		impl->pending_transform = base_transform;
		impl->pending_valid = true;
		return true;
	}

	//几何挂载
	bool Collider_Backend::geometry_mount(void)
	{
		//无有效暂存内容时无法挂载
		if (!impl || !impl->pending_valid)
			return false;

		//释放旧几何
		impl->parts.clear();
		impl->part_index.clear();
		impl->part_ignored.clear();
		//替换为暂存几何
		impl->parts = std::move(impl->pending_parts);
		impl->part_ignored = std::move(impl->pending_part_ignored);
		impl->root_transform = impl->pending_transform;
		impl->pending_parts.clear();
		impl->pending_part_ignored.clear();
		impl->pending_valid = false;

		//重建编号映射
		parts_index_rebuild(*impl);
		//刷新子部件世界变换
		parts_refresh(*impl);
		return true;
	}

	//几何清空
	void Collider_Backend::geometry_clear(void)
	{
		//实现体缺失时无需处理
		if (!impl)
			return;
		//释放当前与暂存几何
		impl->parts.clear();
		impl->pending_parts.clear();
		impl->part_index.clear();
		impl->part_ignored.clear();
		impl->pending_part_ignored.clear();
		impl->pending_valid = false;
	}

	//几何挂载状态
	bool Collider_Backend::geometry_mounted(void) const
	{
		return impl && !impl->parts.empty() && impl->parts.front().shape != nullptr;
	}

	//几何部件数量
	uint32_t Collider_Backend::part_count(void) const
	{
		//实现体缺失时无部件
		if (!impl)
			return 0;
		return static_cast<uint32_t>(impl->parts.size());
	}

	//子部件编号存在性检查
	bool Collider_Backend::part_exists(uint16_t part_ID) const
	{
		//实现体缺失时视为不存在
		if (!impl)
			return false;
		return impl->part_index.count(part_ID) > 0;
	}

	//根变换写入
	void Collider_Backend::world_transform_set(const Transform& transform)
	{
		//实现体缺失时无法写入
		if (!impl)
			return;
		//写入根变换
		impl->root_transform = transform;
		//刷新子部件世界变换
		parts_refresh(*impl);
	}

	//根变换读取
	Transform Collider_Backend::world_transform_get(void) const
	{
		//实现体缺失时返回单位变换
		if (!impl)
			return Transform::getIdentity();
		return impl->root_transform;
	}

	//根变换平移推进
	void Collider_Backend::origin_advance(const Vector3& segment)
	{
		//实现体缺失时无法推进
		if (!impl)
			return;
		//推进根变换平移
		impl->root_transform.setOrigin(impl->root_transform.getOrigin() + segment);
		//刷新子部件世界变换
		parts_refresh(*impl);
	}

	//世界包围盒读取（全部子部件并集）
	void Collider_Backend::aabb_get(Vector3& aabb_min, Vector3& aabb_max) const
	{
		//实现体缺失或几何未挂载时返回零值
		if (!impl || impl->parts.empty())
		{
			aabb_min = Vector3();
			aabb_max = Vector3();
			return;
		}

		//并集包围盒
		btVector3 min_bt, max_bt;
		//是否已写入首个部件包围盒
		bool first_part = true;
		//逐个部件求并集
		for (const Geometry_Part& part : impl->parts)
		{
			//无对象或无形状的部件跳过
			if (!part.object || !part.shape)
				continue;
			//单个部件包围盒
			btVector3 part_min, part_max;
			part.shape->getAabb(part.object->object.getWorldTransform(), part_min, part_max);
			//首个有效部件直接写入
			if (first_part)
			{
				min_bt = part_min;
				max_bt = part_max;
				first_part = false;
				continue;
			}
			//后续部件合并
			min_bt.setMin(part_min);
			max_bt.setMax(part_max);
		}

		//转换包围盒
		aabb_min = conversion::to_engine(min_bt);
		aabb_max = conversion::to_engine(max_bt);
	}

	//静态对象标记设置
	void Collider_Backend::static_flag_set(bool is_static)
	{
		//实现体缺失时无法设置
		if (!impl)
			return;
		//逐个部件写入碰撞标记
		for (Geometry_Part& part : impl->parts)
		{
			//无对象的部件跳过
			if (!part.object)
				continue;
			part.object->object.setCollisionFlags(is_static ? btCollisionObject::CF_STATIC_OBJECT : 0);
		}
	}
}
