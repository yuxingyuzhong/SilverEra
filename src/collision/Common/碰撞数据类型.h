#pragma once
//预编译头
#include "Engine/EngineFoundation/common/前置头文件包含.h"
//获取自有几何类型
#include "几何类型.h"

namespace engine
{
	//碰撞结果（精确到子部件）
	struct Collision_Result
	{
		//碰撞体A
		uint64_t collider_A = 0;
		//碰撞体B
		uint64_t collider_B = 0;
		//碰撞体A 的子部件编号（0xFFFF 表示无子部件信息）
		uint16_t part_A = 0xFFFF;
		//碰撞体B 的子部件编号（0xFFFF 表示无子部件信息）
		uint16_t part_B = 0xFFFF;
	};

	/*
	单个接触点
	法线朝向固定为「由碰撞体B 指向碰撞体A」，便于前端按 A/B 语义统一处理；
	离散检测没有扫掠时间比例，fraction 恒为一。
	*/
	struct Collision_Contact
	{
		//碰撞体A 的子部件编号（0xFFFF 表示无子部件信息）
		uint16_t part_A = 0xFFFF;
		//碰撞体B 的子部件编号（0xFFFF 表示无子部件信息）
		uint16_t part_B = 0xFFFF;
		//世界接触点（两侧接触位置的中点）
		Vector3 point{ 0.0f,0.0f,0.0f };
		//世界法线（由碰撞体B 指向碰撞体A）
		Vector3 normal{ 0.0f,0.0f,0.0f };
		//穿透深度（分离时为负距离钳零）
		Scalar depth = 0.0f;
		//扫掠命中比例（离散检测恒为一）
		Scalar fraction = 1.0f;
	};

	//碰撞详情（碰撞对及其接触点集合）
	struct Collision_Detail
	{
		//碰撞体A
		uint64_t collider_A = 0;
		//碰撞体B
		uint64_t collider_B = 0;
		//接触点集合（扫掠与离散检测的接触点按出现顺序合并）
		std::vector<Collision_Contact> contacts;
	};

	//碰撞体相对碰撞空间的跨越状态
	enum class Cross_State
	{
		//完全位于碰撞空间内
		inside,
		//部分位于碰撞空间内(跨越边界)
		crossing,
		//完全位于碰撞空间外
		outside
	};

	//跨越状态文本(事件载荷取值)
	inline const char* cross_state_text(const Cross_State state)
	{
		//按状态返回文本
		switch (state)
		{
		case Cross_State::inside:
			return "inside";
		case Cross_State::crossing:
			return "crossing";
		default:
			return "outside";
		}
	}

	//跨越通知(碰撞体相对碰撞空间发生跨越状态转移)
	struct Cross_Notice
	{
		//碰撞体编号
		uint64_t collider_ID = 0;
		//通知类型(部分跨越 cross / 完全回归 return / 完全超出跨越 exit)
		std::string kind;
		//转移前的跨越状态
		Cross_State previous = Cross_State::outside;
		//判定时的世界包围盒中心
		Vector3 position{ 0.0f,0.0f,0.0f };
		//判定时的逻辑帧序号
		uint64_t frame = 0;
		//本空间内单调递增的通知序号
		uint64_t sequence = 0;
	};

	//跨越状态快照条目(known 为假表示尚未建立基准或无几何)
	struct Cross_State_Entry
	{
		//碰撞体编号
		uint64_t collider_ID = 0;
		//跨越状态
		Cross_State state = Cross_State::outside;
		//状态是否已知
		bool known = false;
	};
}
