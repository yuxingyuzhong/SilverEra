#include "../../局部命名空间使用.h"
//获取日志系统
#include "Engine/EngineFoundation/src/log/日志系统运行包.h"

namespace engine
{
	//构造函数
	Proxy_Backend::Proxy_Backend()
	{
	}

	//析构函数
	Proxy_Backend::~Proxy_Backend() = default;

	//运动读取回调绑定
	void Proxy_Backend::motion_reader_bind(std::function<bool(uint64_t, Motion_Event&)> reader)
	{
		motion_reader = std::move(reader);
	}

	//碰撞空间构建
	bool Proxy_Backend::region_build(const string& region)
	{
		//若目标碰撞空间已存在
		if (regions.count(region))
		{
			logger.warn("Proxy_Backend::待构建碰撞空间({})已存在", region);
			return false;
		}

		//分配碰撞空间内存
		regions[region].reset(new(nothrow) Collision_Region(region));
		//若内存分配失败
		if (!regions[region])
		{
			logger.error("Proxy_Backend::内存分配失败\n目标碰撞空间({})无法创建", region);
			return false;
		}

		//注入运动读取回调（碰撞空间更新位置时向代理后端重读运动事件）
		if (motion_reader)
			regions[region]->motion_reader_set(motion_reader);
		return true;
	}

	//碰撞空间卸载
	bool Proxy_Backend::region_unload(const string& region)
	{
		//获取目标碰撞空间迭代器
		auto it = regions.find(region);
		//若目标碰撞空间不存在
		if (it == regions.end())
		{
			logger.warn("Proxy_Backend::待卸载碰撞空间({})不存在", region);
			return false;
		}

		//注销该空间持有的全部碰撞体编号
		for (uint64_t collider_ID : it->second->colliders())
			collider_mapping_unlink(collider_ID, region);
		//卸载目标碰撞空间
		regions.erase(it);
		return true;
	}

	//碰撞空间检测执行
	bool Proxy_Backend::region_detect(const string& region, std::vector<Collision_Detail>& details)
	{
		//查找目标碰撞空间
		Collision_Region* target = region_seek(region);
		//若目标碰撞空间不存在
		if (!target)
		{
			logger.warn("Proxy_Backend::待检测碰撞空间({})不存在", region);
			return false;
		}

		//执行碰撞检测
		optional<vector<Collision_Detail>> detected = target->detect_detail();
		//若空间未激活或后端不可用
		if (!detected)
		{
			logger.warn("Proxy_Backend::碰撞空间({})未激活，检测未执行", region);
			return false;
		}

		//输出检测结果
		details = std::move(*detected);
		return true;
	}

	//碰撞空间活跃性设置
	bool Proxy_Backend::region_state_set(const string& region, bool active)
	{
		//查找目标碰撞空间
		Collision_Region* target = region_seek(region);
		//若目标碰撞空间不存在
		if (!target)
		{
			logger.warn("Proxy_Backend::待设置状态的碰撞空间({})不存在", region);
			return false;
		}

		//设置空间活跃性
		target->state_set(active);
		return true;
	}

	//碰撞空间查找
	Collision_Region* Proxy_Backend::region_seek(const string& region) const
	{
		//查找目标碰撞空间
		auto it = regions.find(region);
		//若目标碰撞空间不存在
		if (it == regions.end())
			return nullptr;

		return it->second.get();
	}

	//碰撞空间内碰撞体编号全量获取
	std::vector<uint64_t> Proxy_Backend::region_colliders(const string& region) const
	{
		//查找目标碰撞空间
		Collision_Region* target = region_seek(region);
		//若目标碰撞空间不存在则返回空集合
		if (!target)
			return vector<uint64_t>();

		return target->colliders();
	}

	//碰撞体归属查找
	bool Proxy_Backend::collider_owner_seek(const uint64_t collider_ID, string& receiver) const
	{
		//查找该编号的全部登记项
		auto it = collider_mapping.find(collider_ID);
		//若该编号未被任何空间持有
		if (it == collider_mapping.end())
			return false;

		//写入该编号的持有空间名称
		receiver = it->second;
		return true;
	}

	//碰撞体编号登记
	void Proxy_Backend::collider_mapping_link(const uint64_t collider_ID, const string& region)
	{
		collider_mapping.emplace(collider_ID, region);
	}

	//碰撞体编号注销
	void Proxy_Backend::collider_mapping_unlink(const uint64_t collider_ID, const string& region)
	{
		//取该编号的全部登记项
		auto range = collider_mapping.equal_range(collider_ID);
		//逐个注销属于目标空间的登记项
		for (auto it = range.first; it != range.second;)
		{
			//若登记项属于目标空间则删除
			if (it->second == region)
				it = collider_mapping.erase(it);
			else
				++it;
		}
	}
}