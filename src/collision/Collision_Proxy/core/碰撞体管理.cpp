#include "../局部命名空间使用.h"
#include "Engine/EngineFoundation/src/log/日志系统运行包.h"

namespace engine
{
	//碰撞体构建
	optional<uint64_t> Collision_Proxy::collider_build(const string& region)
	{
		//目标碰撞体编号
		uint64_t collider_ID = 0;
		//经后端在目标空间内构建碰撞体
		if (!backend.collider_build(region, collider_ID))
			return nullopt;

		return collider_ID;
	}

	//碰撞体卸载
	bool Collision_Proxy::collider_unload(const uint64_t collider_ID, const string& region)
	{
		return backend.collider_unload(collider_ID, region);
	}

	//碰撞体转移
	bool Collision_Proxy::collider_transfer(const uint64_t collider_ID, const string& region)
	{
		return backend.collider_transfer(collider_ID, region);
	}

	//碰撞体镜像
	bool Collision_Proxy::collider_mirror(const uint64_t collider_ID, const string& region)
	{
		return backend.collider_mirror(collider_ID, region);
	}

	//碰撞体设置 —— 检测方式重载
	bool Collision_Proxy::collider_set(const uint64_t collider_ID, const Detection_Mode& vector)
	{
		//对持有该编号的空间施加检测方式设置
		return backend.collider_set_dispatch(collider_ID, [collider_ID, &vector](
			Collision_Region& region)
			{
				return region.collider_set(collider_ID, vector);
			});
	}

	//碰撞体设置 —— 豁免标记重载
	bool Collision_Proxy::collider_set(const uint64_t collider_ID, const uint64_t& vector)
	{
		//对持有该编号的空间施加豁免标记设置
		return backend.collider_set_dispatch(collider_ID, [collider_ID, &vector](
			Collision_Region& region)
			{
				return region.collider_set(collider_ID, vector);
			});
	}
}