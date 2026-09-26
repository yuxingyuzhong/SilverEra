#include "../局部命名空间使用.h"
//获取数据校验器
#include "src/tools/Data_Validator/数据校验器.h"
//获取日志系统
#include "src/tools/Logging/日志系统.h"
//获取网格加载器
#include "src/tools/Mesh_Loader/网格加载器.h"

namespace engine
{
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
}
