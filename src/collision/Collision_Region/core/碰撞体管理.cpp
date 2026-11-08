#include "../局部命名空间使用.h"
//获取碰撞体后端声明
#include "src/collision/Collider/backend/碰撞体后端.h"
//获取日志系统
#include "Engine/EngineFoundation/src/log/日志系统运行包.h"
//获取逻辑帧操作工具
#include "Engine/EngineSystem/common/runtime/逻辑帧.h"
//获取碰撞配置字段读取工具
#include "src/collision/Common/字段读取.h"

namespace engine
{
	//引入字段读取工具
	using namespace collision_detail;

	//碰撞体构建
	uint64_t Collision_Region::collider_build(void)
	{
		//分配碰撞体编号
		uint64_t collider_ID = ID_allocator.get();
		//跳过已被接管的编号（空间间转移会占用本空间的编号）
		while (mapping.count(collider_ID))
			collider_ID = ID_allocator.get();

		//登记碰撞体
		Collider& collider = mapping[collider_ID];
		//写入编号
		collider.ID = collider_ID;
		//写入后端身份标签
		collider.backend->tag_set(collider_ID, false);
		//返回碰撞体编号
		return collider_ID;
	}

	//碰撞体接管(按指定编号登记，供空间间转移使用)
	bool Collision_Region::collider_adopt(uint64_t collider_ID)
	{
		//若该编号已被占用
		if (mapping.count(collider_ID))
		{
			logger.warn("Collision_Region::待接管碰撞体编号({})已被占用", collider_ID);
			return false;
		}

		//登记碰撞体
		Collider& collider = mapping[collider_ID];
		//写入编号
		collider.ID = collider_ID;
		//写入后端身份标签
		collider.backend->tag_set(collider_ID, false);
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
			logger.warn("Collision_Region::待卸载碰撞体({})不存在", collider_ID);
			return false;
		}

		//若目标碰撞体已挂载几何体
		if (target->geometry_mounted())
		{
			//移出碰撞世界
			backend.object_detach(*target->backend);
			//卸载几何体
			target->backend->geometry_clear();
		}

		//注销碰撞体
		mapping.erase(collider_ID);
		//抹除跨越状态记录（避免编号回收后被新碰撞体误继承）
		cross_states.erase(collider_ID);
		//抹除跨越状态脏标记
		cross_dirty.erase(collider_ID);
		//回收碰撞体编号
		ID_allocator.recycle(collider_ID);
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
			logger.warn("Collision_Region::待设置碰撞体({})不存在", collider_ID);
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
			logger.warn("Collision_Region::待设置碰撞体({})不存在", collider_ID);
			return false;
		}

		//写入豁免标记（前后端同步）
		target->exemption_flag = vector;
		target->backend->exemption_flag_set(vector);

		/*
		重挂以重建宽阶段配对
		豁免判定前移到宽阶段，标记变更后旧配对不再自动失效，
		故重新挂载以强制清理并重新生成配对。
		*/
		if (backend.object_contains(*target->backend))
		{
			//摘除后重新挂载
			backend.object_detach(*target->backend);
			backend.object_attach(*target->backend);
		}
		return true;
	}

	//碰撞体设置 —— 几何体添加
	bool Collision_Region::collider_set(nlohmann::json geometry_config)
	{
		//几何配置格式检查
		if (!geometry_config.is_object())
		{
			logger.warn("Collision_Region::几何配置非对象格式");
			return false;
		}
		//目标碰撞体编号
		uint64_t collider_ID = 0;
		//提取目标碰撞体编号
		if (!number_read(geometry_config, "collider_ID", collider_ID))
		{
			logger.warn("Collision_Region::几何配置缺少有效字段(collider_ID)");
			return false;
		}
		//查找目标碰撞体
		Collider* target = collider_seek(collider_ID);
		//若目标碰撞体不存在
		if (!target)
		{
			logger.warn("Collision_Region::待设置碰撞体({})不存在", collider_ID);
			return false;
		}

		//世界变换基准（未指定位置或旋转时沿用当前变换）
		Transform transform = target->backend->world_transform_get();

		/*
		基准位置
		单几何体形式下即为该几何体的世界位置；集合形式下为碰撞体的基准位置，
		各几何体的相对位置由几何体集合元素内的 position 字段描述。
		*/
		if (geometry_config.contains("position"))
		{
			//基准位置
			Vector3 position;
			//读取基准位置
			if (!vector_read(geometry_config, "position", position))
			{
				logger.warn("Collision_Region::碰撞体({})字段(position)非法", collider_ID);
				return false;
			}
			//写入基准位置
			transform.setOrigin(position);
		}
		//基准旋转（语义同基准位置）
		if (geometry_config.contains("rotation"))
		{
			//基准旋转
			Quaternion rotation;
			//读取基准旋转
			if (!quaternion_read(geometry_config, "rotation", rotation))
			{
				logger.warn("Collision_Region::碰撞体({})字段(rotation)非法", collider_ID);
				return false;
			}
			//写入基准旋转
			transform.setRotation(rotation);
		}

		//几何构建（暂存区装配，失败时保留当前几何）
		if (!target->backend->geometry_build(geometry_config, transform))
		{
			logger.warn("Collision_Region::碰撞体({})几何配置非法", collider_ID);
			return false;
		}

		//若目标碰撞体已挂载几何体则先摘除
		if (target->geometry_mounted())
			backend.object_detach(*target->backend);

		//挂载暂存几何
		target->backend->geometry_mount();
		//几何配置留档（供空间间镜像与转移复用）
		target->geometry = std::move(geometry_config);
		//加入碰撞世界
		backend.object_attach(*target->backend);
		//几何变更后标记跨越状态须强制重算（重建/瞬移不作为真实跨越的基准跳过）
		cross_dirty.insert(collider_ID);
		return true;
	}

	//碰撞体位移频率设置(作用频率与生效计时随碰撞体保存于本空间，登记时重新起算生效计时)
	bool Collision_Region::collider_frequency_set(uint64_t collider_ID, uint64_t frequency)
	{
		//查找目标碰撞体
		Collider* target = collider_seek(collider_ID);
		//若目标碰撞体不存在
		if (!target)
		{
			logger.warn("Collision_Region::待设置运动频率的碰撞体({})不存在", collider_ID);
			return false;
		}

		//写入运动作用频率
		target->motion_frequency = frequency;
		//生效计时重新起算(新配置须等待一个完整帧间隔后才生效)
		target->motion_frame = Logics::frame_count_get();
		//清零本帧施加运动
		target->motion_step = Motion();
		target->part_motion_steps.clear();
		return true;
	}

	//碰撞体查询
	bool Collision_Region::collider_find(uint64_t collider_id) const
	{
		//按登记情况判定
		return mapping.count(collider_id) > 0;
	}

	//碰撞体全量获取
	std::vector<uint64_t> Collision_Region::colliders(void) const
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

	//碰撞体离散步长读取
	bool Collision_Region::collider_step_length_get(uint64_t collider_ID, uint32_t& receiver) const
	{
		//查找目标碰撞体
		auto it = mapping.find(collider_ID);
		//若目标碰撞体不存在
		if (it == mapping.end())
			return false;

		//写入离散步长
		receiver = it->second.detection_mode.step_length;
		return true;
	}

	//碰撞体几何部件数读取
	bool Collision_Region::collider_part_count_get(uint64_t collider_ID, uint32_t& receiver) const
	{
		//查找目标碰撞体
		auto it = mapping.find(collider_ID);
		//若目标碰撞体不存在或后端缺失
		if (it == mapping.end() || !it->second.backend)
			return false;

		//写入部件数
		receiver = it->second.backend->part_count();
		return true;
	}

	//碰撞体子部件编号存在性检查
	bool Collision_Region::collider_part_exists(uint64_t collider_ID, uint16_t part_ID) const
	{
		//查找目标碰撞体
		auto it = mapping.find(collider_ID);
		//若目标碰撞体不存在或后端缺失
		if (it == mapping.end() || !it->second.backend)
			return false;

		return it->second.backend->part_exists(part_ID);
	}

	//碰撞体包含性检测
	bool Collision_Region::contains(uint64_t collider_ID) const
	{
		//查找目标碰撞体
		auto it = mapping.find(collider_ID);
		//若目标碰撞体未登记
		if (it == mapping.end())
			return false;

		//交由后端判定对象是否已进入碰撞世界
		return backend.object_contains(*it->second.backend);
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