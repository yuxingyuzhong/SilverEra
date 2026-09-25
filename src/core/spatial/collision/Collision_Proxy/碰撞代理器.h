#pragma once
//预编译头
#include "common/前置头文件包含.h"
//获取引擎环境
#include "src/tools/Engine_Env/引擎环境.h"
//获取路径字符串转化方法
#include "src/tools/Auxi_Algorithm/路径字符串转换.h"
//获取碰撞库依赖封装
#include "../../common/core/依赖库封装.h"
//获取空间系统运行包
#include "../../common/碰撞系统运行包.h"
//获取事件系统运行包
#include "src/core/event/事件系统运行包.h"

namespace engine
{
	//碰撞代理器
	class Collision_Proxy
	{
	private:		
		//模块名称（事件中转站登记名与事件发送者标识）
		inline static const std::string module_name = "Collision_Proxy";

		//碰撞空间集合
		std::unordered_map<std::string, std::unique_ptr<Collision_Region>> regions;
		/*
		碰撞体归属映射
		键为碰撞体编号，值为持有该编号的碰撞空间名称；
		编号由各碰撞空间自行分配，同一编号可被不同空间分别持有，故使用多重映射。
		*/
		std::unordered_multimap<uint64_t, std::string> collider_mapping;
	public:
		//事件终端
		Event_Terminal event_terminal;
	private:
		//事件发送权限密钥
		int64_t acl_key = 0;
	public:		
		//构造函数
		Collision_Proxy();
		//默认析构
		~Collision_Proxy() = default;

		//碰撞空间构建
		bool region_build(const std::string& region);
		//碰撞空间卸载
		bool region_unload(const std::string& region);
		//碰撞空间检测执行
		bool region_detect(const std::string& region);
		//碰撞空间边界设置
		bool region_boundary_set(const std::string& path);
		//碰撞空间活跃性设置
		bool region_state_set(const std::string& region,bool active);

		//碰撞体构建
		std::optional<uint64_t> collider_build(const std::string& region);
		//碰撞体卸载
		bool collider_unload(const uint64_t collider_ID,const std::string& region);
		//碰撞体转移 
		bool collider_transfer(const uint64_t collider_ID, const std::string& region);
		//碰撞体镜像
		bool collider_mirror(const uint64_t collider_ID, const std::string& region);
		//碰撞体设置 —— 位移向量重载
		bool collider_set(const uint64_t collider_ID, const Vector3& vector);
		//碰撞体设置 —— 检测方式重载
		bool collider_set(const uint64_t collider_ID, const Detection_Mode& vector);
		//碰撞体设置 —— 豁免标记重载
		bool collider_set(const uint64_t collider_ID, const uint64_t& vector);
		//事件中转站接入
		void attach(void);

	private:
		//事件处理
		void event_process(std::shared_ptr<event> evt);

		//碰撞空间查找（不存在的返回空指针）
		Collision_Region* region_seek(const std::string& region);
		//碰撞体归属查找（同一编号被多个空间持有时取其中任一个）
		bool collider_owner_seek(const uint64_t collider_ID, std::string& receiver) const;

		//碰撞体编号登记
		void collider_mapping_link(const uint64_t collider_ID, const std::string& region);
		//碰撞体编号注销
		void collider_mapping_unlink(const uint64_t collider_ID, const std::string& region);

		//碰撞体设置分发（对所有持有该编号的空间生效）
		bool collider_set_dispatch(const uint64_t collider_ID,
			const std::function<bool(Collision_Region&)>& setter);
		//碰撞体配置应用（几何体、位移向量、检测方式、豁免标记）
		void collider_config_apply(const std::string& region, uint64_t collider_ID,
			const nlohmann::json& config);

		//事件发布
		bool event_publish(const std::string& tag, const nlohmann::json& payload);
		//配置内容应用（Config/Load 路由指令）
		void config_apply(const nlohmann::json& config);
		//边界配置文件读取
		bool boundary_config_read(const std::string& path, nlohmann::json& receiver) const;
	};
}