#pragma once
//预编译头
#include "Engine/EngineFoundation/common/前置头文件包含.h"
//无序集合（本帧移动集合与缓存失效集合）
#include <unordered_set>
//获取自有几何类型
#include "../Common/几何类型.h"
//获取碰撞数据类型
#include "../Common/碰撞数据类型.h"
//获取碰撞体定义
#include "../Collider/碰撞体.h"
//获取碰撞空间后端
#include "backend/空间后端.h"
//获取数值分配器
#include "Engine/EngineFoundation/src/detail/class/Number_Pool/数值分配器.h"

namespace engine
{
	//碰撞空间
	struct Collision_Region
	{
	private:
		//碰撞空间名称
		std::string name;
		//碰撞空间活跃性
		bool is_active = false;

		//碰撞体编号分配器
		Number_Pool<uint64_t> ID_allocator;
		//碰撞检测后端
		Region_Backend backend{};
		//碰撞体映射
		std::unordered_map<uint64_t, Collider> mapping;
		/*
		运动事件读取回调（返回false表示该编号尚无运动事件）
		由碰撞代理器注入，用于在更新碰撞体位置时重读最新运动事件。
		*/
		std::function<bool(uint64_t, Motion_Event&)> motion_reader;
		//跨越状态记录（键为碰撞体编号，记录上一次判定所得的跨越状态）
		std::unordered_map<uint64_t, Cross_State> cross_states;
		//跨越状态失效集合（几何重建等变更后须强制重算）
		std::unordered_set<uint64_t> cross_dirty;
		//本帧跨越通知（检测时收集，供碰撞代理器发布后取走）
		std::vector<Cross_Notice> cross_notices;
		//跨越通知序号（本空间内单调递增）
		uint64_t cross_sequence = 0;
	public:
		//默认构造
		Collision_Region();
		//含参构造(指定碰撞空间名称)
		explicit Collision_Region(const std::string& region_name);

		//禁止拷贝（backend 含唯一所有权）
		Collision_Region(const Collision_Region&) = delete;
		Collision_Region& operator=(const Collision_Region&) = delete;

		/*
		默认移动
		后端实现体在堆上分配，移动只搬移句柄，碰撞对象与碰撞世界地址均不变，
		故按值搬移后无需重建空间边界。
		*/
		Collision_Region(Collision_Region&&) = default;
		Collision_Region& operator=(Collision_Region&&) = default;

		//析构函数
		~Collision_Region();

		//碰撞空间有效性检查
		bool valid(void) const;
		//碰撞空间状态设置
		void state_set(bool active);
		//碰撞空间状态查看
		bool state_check() const;
		//边界构建
		bool boundary_build(const std::string& mesh_path);
		//边界卸载
		void boundary_unload();

		//碰撞体构建
		uint64_t collider_build(void);
		//碰撞体卸载
		bool collider_unload(uint64_t collider_ID);
		//碰撞体设置 —— 检测方式重载
		bool collider_set(const uint64_t collider_ID, const Detection_Mode& vector);
		//碰撞体设置 —— 豁免标记重载
		bool collider_set(const uint64_t collider_ID, const uint64_t& vector);
		//碰撞体设置 —— 几何体添加
		bool collider_set(nlohmann::json geometry_config);
		//碰撞体查询
		bool collider_find(uint64_t collider_id) const;
		//碰撞体全量获取
		std::vector<uint64_t> colliders(void) const;
		//碰撞体接管(按指定编号登记，供空间间转移使用)
		bool collider_adopt(uint64_t collider_ID);
		//碰撞体几何配置读取(编号不存在时返回空配置)
		nlohmann::json collider_geometry(uint64_t collider_ID) const;
		//碰撞体离散步长读取(编号不存在时返回false)
		bool collider_step_length_get(uint64_t collider_ID, uint32_t& receiver) const;
		//碰撞体几何部件数读取(编号不存在时返回false)
		bool collider_part_count_get(uint64_t collider_ID, uint32_t& receiver) const;
		//碰撞体子部件编号存在性检查
		bool collider_part_exists(uint64_t collider_ID, uint16_t part_ID) const;
		//运动事件读取回调注入(由碰撞代理器注入)
		void motion_reader_set(std::function<bool(uint64_t, Motion_Event&)> reader);
		//碰撞体位移频率设置(作用频率与生效计时随碰撞体保存于本空间，登记时重新起算生效计时)
		bool collider_frequency_set(uint64_t collider_ID, uint64_t frequency);

		//执行碰撞检测（返回碰撞对，供内部/兼容调用）
		std::optional<std::vector<Collision_Result>> detect(void);
		//执行碰撞检测（返回碰撞详情，含接触点/法线/穿透深度）
		std::optional<std::vector<Collision_Detail>> detect_detail(void);
		//碰撞体包含性检测
		bool contains(uint64_t collider_ID) const;
		//跨越通知取走(取出并清空本帧收集的跨越通知)
		std::vector<Cross_Notice> cross_notices_take(void);
		//跨越状态快照(查询用，含尚未建立基准的碰撞体)
		void cross_state_snapshot(std::vector<Cross_State_Entry>& receiver) const;
	private:
		//碰撞体查找
		Collider* collider_seek(uint64_t collider_ID);

		//跨越状态更新(位置更新后比对旧状态并收集跨越通知)
		void cross_state_update(const std::vector<uint64_t>& boundary_contacts,
			const std::vector<uint64_t>& swept_contacts,
			const std::unordered_set<uint64_t>& moved_IDs);
	};

}
