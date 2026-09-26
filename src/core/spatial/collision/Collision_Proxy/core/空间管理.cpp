#include "../局部命名空间使用.h"
#include "src/tools/Logging/日志系统.h"

namespace engine
{
	//构造函数
	Collision_Proxy::Collision_Proxy()
	{
		//生成事件发送权限密钥
		acl_key = event_terminal.acl_key_gen();
	}

	//碰撞空间构建
	bool Collision_Proxy::region_build(const string& region)
	{
		//若目标碰撞空间已存在
		if(regions.count(region))
		{
			Log::warn("Collision_Proxy::待构建碰撞空间({})已存在",region);
			return false;
		}
		else
		{
			//分配碰撞空间内存
			regions[region].reset(new(nothrow)Collision_Region(region));
			//若内存分配失败
			if (!regions[region])
			{
				Log::error("Collision_Proxy::内存分配失败\n目标碰撞空间({})无法创建", region);
				return false;
			}
			else
			{
				//注入位移读取回调（碰撞空间更新位置时向本代理器重读位移事件）
				region_displacement_link(*regions[region]);
				return true;
			}
		}
	}

	//碰撞空间卸载
	bool Collision_Proxy::region_unload(const string& region)
	{
		//获取目标碰撞空间迭代器
		auto it = regions.find(region);
		//若目标碰撞空间不存在
		if (it == regions.end())
		{
			Log::warn("Collision_Proxy::待卸载碰撞空间({})不存在", region);
			return false;
		}
		else
		{
			//注销该空间持有的全部碰撞体编号
			for (uint64_t collider_ID : it->second->colliders())
				collider_mapping_unlink(collider_ID, region);
			//卸载目标碰撞空间
			regions.erase(region);
			return true;
		}
	}

	//碰撞空间检测执行
	bool Collision_Proxy::region_detect(const string& region)
	{
		//查找目标碰撞空间
		Collision_Region* target = region_seek(region);
		//若目标碰撞空间不存在
		if (!target)
		{
			Log::warn("Collision_Proxy::待检测碰撞空间({})不存在", region);
			return false;
		}

		//执行碰撞检测
		optional<vector<Collision_Result>> detected = target->detect();
		//若空间未激活或后端不可用
		if (!detected)
		{
			Log::warn("Collision_Proxy::碰撞空间({})未激活，检测未执行", region);
			return false;
		}

		//碰撞响应流程（发布碰撞事件并按回复调整位移）
		collision_protocol(region, *detected);

		//跨越通知发布（通知外界碰撞体已首次部分跨越/完全回归/完全超出跨越本空间）
		for (const Cross_Notice& notice : target->cross_notices_take())
		{
			//跨越通知载荷
			json cross_payload;
			//写入空间名称、碰撞体编号与通知类型
			cross_payload["region"] = region;
			cross_payload["collider_ID"] = notice.collider_ID;
			cross_payload["state"] = notice.kind;
			//发布跨越通知事件
			event_publish("RegionCrossNotice", cross_payload);
		}

		//检测结果载荷
		json payload;
		//写入空间名称
		payload["region"] = region;
		//写入碰撞对集合
		payload["results"] = json::array();
		//逐个写入碰撞对
		for (const Collision_Result& result : *detected)
		{
			//单个碰撞对
			json item;
			//写入碰撞体编号
			item["collider_A"] = result.collider_A;
			item["collider_B"] = result.collider_B;
			//追加碰撞对
			payload["results"].push_back(item);
		}

		//发布检测结果事件
		return event_publish("DetectResult", payload);
	}

	//碰撞空间活跃性设置
	bool Collision_Proxy::region_state_set(const string& region, bool active)
	{
		//查找目标碰撞空间
		Collision_Region* target = region_seek(region);
		//若目标碰撞空间不存在
		if (!target)
		{
			Log::warn("Collision_Proxy::待设置状态的碰撞空间({})不存在", region);
			return false;
		}

		//设置空间活跃性
		target->state_set(active);
		return true;
	}

	//碰撞空间查找
	Collision_Region* Collision_Proxy::region_seek(const string& region)
	{
		//查找目标碰撞空间
		auto it = regions.find(region);
		//若目标碰撞空间不存在
		if (it == regions.end())
			return nullptr;

		return it->second.get();
	}

	//碰撞体归属查找
	bool Collision_Proxy::collider_owner_seek(const uint64_t collider_ID, string& receiver) const
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
	void Collision_Proxy::collider_mapping_link(const uint64_t collider_ID, const string& region)
	{
		collider_mapping.emplace(collider_ID, region);
	}

	//碰撞体编号注销
	void Collision_Proxy::collider_mapping_unlink(const uint64_t collider_ID, const string& region)
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
