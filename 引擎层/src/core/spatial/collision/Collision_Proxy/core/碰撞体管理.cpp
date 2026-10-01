#include "../局部命名空间使用.h"
#include "src/tools/Logging/日志系统.h"

namespace engine
{
	//碰撞体构建
	optional<uint64_t> Collision_Proxy::collider_build(const string& region)
	{
		//查找目标碰撞空间
		Collision_Region* target = region_seek(region);
		//若目标碰撞空间不存在
		if (!target)
		{
			Log::warn("Collision_Proxy::待构建碰撞体的碰撞空间({})不存在", region);
			return nullopt;
		}

		//在目标空间内构建碰撞体
		uint64_t collider_ID = target->collider_build();
		//登记编号归属
		collider_mapping_link(collider_ID, region);
		return collider_ID;
	}

	//碰撞体卸载
	bool Collision_Proxy::collider_unload(const uint64_t collider_ID, const string& region)
	{
		//查找目标碰撞空间
		Collision_Region* target = region_seek(region);
		//若目标碰撞空间不存在
		if (!target)
		{
			Log::warn("Collision_Proxy::待卸载碰撞体的碰撞空间({})不存在", region);
			return false;
		}

		//在目标空间内卸载碰撞体
		if (!target->collider_unload(collider_ID))
			return false;
		//注销编号归属
		collider_mapping_unlink(collider_ID, region);
		return true;
	}

	//碰撞体转移
	bool Collision_Proxy::collider_transfer(const uint64_t collider_ID, const string& region)
	{
		//查找目标碰撞空间
		Collision_Region* target = region_seek(region);
		//若目标碰撞空间不存在
		if (!target)
		{
			Log::warn("Collision_Proxy::待转移至的碰撞空间({})不存在", region);
			return false;
		}

		//查找编号归属空间
		string owner;
		//若该编号未被任何空间持有
		if (!collider_owner_seek(collider_ID, owner))
		{
			Log::warn("Collision_Proxy::待转移碰撞体({})不存在", collider_ID);
			return false;
		}
		//归属空间即目标空间时无需转移
		if (owner == region)
		{
			Log::warn("Collision_Proxy::待转移碰撞体({})已在目标空间({})内", collider_ID, region);
			return false;
		}

		//查找源碰撞空间
		Collision_Region* source = region_seek(owner);
		//若源碰撞空间不存在
		if (!source)
		{
			Log::warn("Collision_Proxy::碰撞体({})的归属空间({})不存在", collider_ID, owner);
			return false;
		}

		//取源碰撞体的几何配置
		json geometry = source->collider_geometry(collider_ID);

		//在目标空间内按原编号接管（接管失败时源空间尚未改动）
		if (!target->collider_adopt(collider_ID))
		{
			Log::warn("Collision_Proxy::碰撞体({})编号在目标空间({})内冲突，转移中止", collider_ID, region);
			return false;
		}
		//卸载源碰撞体
		if (!source->collider_unload(collider_ID))
		{
			//回滚目标空间内的接管
			target->collider_unload(collider_ID);
			return false;
		}

		//更新编号归属
		collider_mapping_unlink(collider_ID, owner);
		collider_mapping_link(collider_ID, region);

		//无几何配置时仅转移编号登记（尚未设置几何体的占位碰撞体）
		if (geometry.is_null() || geometry.empty())
			return true;

		//在目标空间内按原编号重建几何体
		geometry["collider_ID"] = collider_ID;
		if (!target->collider_set(geometry))
		{
			Log::warn("Collision_Proxy::碰撞体({})几何体在目标空间({})重建失败", collider_ID, region);
			return false;
		}

		return true;
	}

	//碰撞体镜像
	bool Collision_Proxy::collider_mirror(const uint64_t collider_ID, const string& region)
	{
		//查找目标碰撞空间
		Collision_Region* target = region_seek(region);
		//若目标碰撞空间不存在
		if (!target)
		{
			Log::warn("Collision_Proxy::待镜像至的碰撞空间({})不存在", region);
			return false;
		}

		//查找编号归属空间
		string owner;
		//若该编号未被任何空间持有
		if (!collider_owner_seek(collider_ID, owner))
		{
			Log::warn("Collision_Proxy::待镜像碰撞体({})不存在", collider_ID);
			return false;
		}

		//查找源碰撞空间
		Collision_Region* source = region_seek(owner);
		//若源碰撞空间不存在
		if (!source)
		{
			Log::warn("Collision_Proxy::碰撞体({})的归属空间({})不存在", collider_ID, owner);
			return false;
		}

		//取源碰撞体的几何配置
		json geometry = source->collider_geometry(collider_ID);

		//在目标空间内构建全新编号的碰撞体
		optional<uint64_t> mirror_ID = collider_build(region);
		//若碰撞体构建失败
		if (!mirror_ID)
			return false;

		//无几何配置时镜像得到的是占位碰撞体
		if (geometry.is_null() || geometry.empty())
			return true;

		//按新编号写入几何配置
		geometry["collider_ID"] = *mirror_ID;
		if (!target->collider_set(geometry))
		{
			Log::warn("Collision_Proxy::镜像碰撞体({})几何体构建失败", *mirror_ID);
			return false;
		}

		return true;
	}

	//碰撞体设置 —— 检测方式重载
	bool Collision_Proxy::collider_set(const uint64_t collider_ID, const Detection_Mode& vector)
	{
		//对持有该编号的空间施加检测方式设置
		return collider_set_dispatch(collider_ID, [collider_ID, &vector](Collision_Region& region)
			{
				return region.collider_set(collider_ID, vector);
			});
	}

	//碰撞体设置 —— 豁免标记重载
	bool Collision_Proxy::collider_set(const uint64_t collider_ID, const uint64_t& vector)
	{
		//对持有该编号的空间施加豁免标记设置
		return collider_set_dispatch(collider_ID, [collider_ID, &vector](Collision_Region& region)
			{
				return region.collider_set(collider_ID, vector);
			});
	}
}
