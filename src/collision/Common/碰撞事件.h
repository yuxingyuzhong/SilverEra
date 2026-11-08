#pragma once
//预编译头
#include "Engine/EngineFoundation/common/前置头文件包含.h"

//游戏引擎命名空间
namespace engine
{
	/*
	碰撞模块事件词汇
	碰撞相关事件均为 category = "Collision"，标签与载荷字段名集中于此，
	避免事件分派侧与事件发布侧各写一份字符串导致漂移。
	*/
	namespace collision_event
	{
		//事件类别
		inline constexpr const char* category = "Collision";

		// ———— 外部 → 本模块 ————

		//碰撞空间构建
		inline constexpr const char* region_build = "RegionBuild";
		//碰撞空间卸载
		inline constexpr const char* region_unload = "RegionUnload";
		//碰撞空间活跃性设置
		inline constexpr const char* region_state = "RegionState";
		//碰撞空间边界设置
		inline constexpr const char* region_boundary = "RegionBoundary";
		//简略碰撞结果事件开关
		inline constexpr const char* region_event_output = "RegionEventOutput";
		//碰撞体构建
		inline constexpr const char* collider_build = "ColliderBuild";
		//碰撞体卸载
		inline constexpr const char* collider_unload = "ColliderUnload";
		//碰撞体转移
		inline constexpr const char* collider_transfer = "ColliderTransfer";
		//碰撞体镜像
		inline constexpr const char* collider_mirror = "ColliderMirror";
		//碰撞体设置
		inline constexpr const char* collider_set = "ColliderSet";
		//运动量
		inline constexpr const char* collider_motion = "ColliderMotion";
		//跨越状态查询
		inline constexpr const char* region_cross_state_query = "RegionCrossStateQuery";

		// ———— 本模块 → 外部 ————

		//碰撞体构建结果
		inline constexpr const char* collider_build_result = "ColliderBuildResult";
		//碰撞空间跨越通知
		inline constexpr const char* region_cross_notice = "RegionCrossNotice";
		//跨越状态查询结果
		inline constexpr const char* region_cross_state_result = "RegionCrossStateResult";
		//简略碰撞结果
		inline constexpr const char* detect_result = "DetectResult";

		// ———— 载荷字段名 ————

		//空间名称
		inline constexpr const char* field_region = "region";
		//碰撞体编号
		inline constexpr const char* field_collider_ID = "collider_ID";
		//空间活跃性
		inline constexpr const char* field_active = "active";
		//结果事件开关
		inline constexpr const char* field_enabled = "enabled";
		//边界配置路径
		inline constexpr const char* field_path = "path";
		//运动量
		inline constexpr const char* field_motion = "motion";
		//运动平移
		inline constexpr const char* field_translation = "translation";
		//部件运动集合
		inline constexpr const char* field_parts = "parts";
		//子部件编号
		inline constexpr const char* field_part = "part";
		//几何体配置中的子部件编号
		inline constexpr const char* field_part_ID = "part_ID";
		//子部件对称忽略对
		inline constexpr const char* field_part_ignore = "part_ignore";
		//运动作用频率
		inline constexpr const char* field_frequency = "frequency";
		//碰撞对集合
		inline constexpr const char* field_results = "results";
		//碰撞体A编号
		inline constexpr const char* field_collider_A = "collider_A";
		//碰撞体B编号
		inline constexpr const char* field_collider_B = "collider_B";
		//碰撞体A 的子部件编号
		inline constexpr const char* field_part_A = "part_A";
		//碰撞体B 的子部件编号
		inline constexpr const char* field_part_B = "part_B";
		//跨越状态
		inline constexpr const char* field_state = "state";
		//转移前的跨越状态
		inline constexpr const char* field_previous_state = "previous_state";
		//逻辑帧序号
		inline constexpr const char* field_frame = "frame";
		//跨越通知序号
		inline constexpr const char* field_sequence = "sequence";
		//查询请求编号
		inline constexpr const char* field_request_ID = "request_ID";
		//跨越状态集合
		inline constexpr const char* field_states = "states";
		//查询目标空间是否存在
		inline constexpr const char* field_found = "found";
		//几何体配置
		inline constexpr const char* field_geometry = "geometry";
		//几何体集合
		inline constexpr const char* field_geometries = "geometries";
		//基准位置
		inline constexpr const char* field_position = "position";
		//基准旋转
		inline constexpr const char* field_rotation = "rotation";
		//检测方式
		inline constexpr const char* field_detection_mode = "detection_mode";
		//扫掠检测标记
		inline constexpr const char* field_is_swept_volume = "is_swept_volume";
		//离散检测步长
		inline constexpr const char* field_step_length = "step_length";
		//豁免标记
		inline constexpr const char* field_exemption_flag = "exemption_flag";
		//空间配置集合
		inline constexpr const char* field_regions = "regions";
		//碰撞体配置集合
		inline constexpr const char* field_colliders = "colliders";
		//空间边界配置
		inline constexpr const char* field_boundary = "boundary";
		//空间结果事件开关
		inline constexpr const char* field_event_output = "event_output";
	}
}
