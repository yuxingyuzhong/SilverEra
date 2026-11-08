#pragma once
//预编译头
#include "Engine/EngineFoundation/common/前置头文件包含.h"
//获取碰撞空间（含碰撞空间后端）
#include "../../Collision_Region/碰撞空间.h"

namespace engine
{
	/*
	碰撞代理器后端
	持有碰撞空间集合与碰撞体编号归属，承接空间间编排（构建、卸载、转移、镜像与批量下发）；
	本类不接触任何碰撞库类型，只经碰撞空间的公开接口工作。
	*/
	class Proxy_Backend
	{
	public:
		//构造函数
		Proxy_Backend();
		//析构函数
		~Proxy_Backend();

		//禁止拷贝
		Proxy_Backend(const Proxy_Backend&) = delete;
		Proxy_Backend& operator=(const Proxy_Backend&) = delete;

		//运动读取回调绑定（新建空间时自动注入）
		void motion_reader_bind(std::function<bool(uint64_t, Motion_Event&)> reader);

		//碰撞空间构建
		bool region_build(const std::string& region);
		//碰撞空间卸载
		bool region_unload(const std::string& region);
		//碰撞空间检测执行（输出碰撞详情）
		bool region_detect(const std::string& region, std::vector<Collision_Detail>& details);
		//碰撞空间活跃性设置
		bool region_state_set(const std::string& region, bool active);
		//碰撞空间查找（不存在的返回空指针）
		Collision_Region* region_seek(const std::string& region) const;
		//碰撞空间内碰撞体编号全量获取
		std::vector<uint64_t> region_colliders(const std::string& region) const;

		//碰撞体构建（编号经出参返回）
		bool collider_build(const std::string& region, uint64_t& collider_ID);
		//碰撞体卸载
		bool collider_unload(uint64_t collider_ID, const std::string& region);
		//碰撞体转移
		bool collider_transfer(uint64_t collider_ID, const std::string& region);
		//碰撞体镜像
		bool collider_mirror(uint64_t collider_ID, const std::string& region);
		//碰撞体设置分发（对所有持有该编号的空间生效）
		bool collider_set_dispatch(uint64_t collider_ID,
			const std::function<bool(Collision_Region&)>& setter) const;
		//碰撞体归属查找（同一编号被多个空间持有时取其中任一个）
		bool collider_owner_seek(uint64_t collider_ID, std::string& receiver) const;
	private:
		//碰撞空间集合
		std::unordered_map<std::string, std::unique_ptr<Collision_Region>> regions;
		/*
		碰撞体归属映射
		键为碰撞体编号，值为持有该编号的碰撞空间名称；
		编号由各碰撞空间自行分配，同一编号可被不同空间分别持有，故使用多重映射。
		*/
		std::unordered_multimap<uint64_t, std::string> collider_mapping;
		//运动事件读取回调
		std::function<bool(uint64_t, Motion_Event&)> motion_reader;

		//碰撞体编号登记
		void collider_mapping_link(const uint64_t collider_ID, const std::string& region);
		//碰撞体编号注销
		void collider_mapping_unlink(const uint64_t collider_ID, const std::string& region);
	};
}
