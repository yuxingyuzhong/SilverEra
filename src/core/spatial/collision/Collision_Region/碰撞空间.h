#pragma once
//预编译头
#include "common/前置头文件包含.h"
//获取碰撞库依赖封装
#include "../../common/core/依赖库封装.h"
//获取碰撞体定义
#include "../Collider/碰撞体.h"
//获取数值分配器
#include "src/tools/Number_Allocator/数值分配器.h"

namespace engine
{
	//碰撞结果
	struct Collision_Result
	{
		//碰撞体A
		uint64_t collider_A = 0;
		//碰撞体B
		uint64_t collider_B = 0;
	};

	//碰撞体相对碰撞空间的跨越状态
	enum class cross_state
	{
		//完全位于碰撞空间内
		inside,
		//部分位于碰撞空间内(跨越边界)
		crossing,
		//完全位于碰撞空间外
		outside
	};

	//跨越通知(碰撞体相对碰撞空间发生跨越状态转移)
	struct Cross_Notice
	{
		//碰撞体编号
		uint64_t collider_ID = 0;
		//通知类型(部分跨越 cross / 完全回归 return / 完全超出跨越 exit)
		std::string kind;
	};

	//碰撞空间
	struct Collision_Region
	{
	private:
		//碰撞空间名称
		std::string name;
		//碰撞空间活跃性
		bool is_active = false;

		//碰撞体编号分配器
		Number_Allocator ID_allocator;
		//空间边界(静态网格)
		Collider region_boundary;
		//碰撞检测后端
		Collision_Backend backend{};
		//碰撞体映射
		std::unordered_map<uint64_t, Collider> mapping;
		/*
		位移事件读取回调（返回false表示该编号尚无位移事件）
		由碰撞代理器注入，用于在更新碰撞体位置时重读最新位移事件。
		*/
		std::function<bool(uint64_t, Vector3&)> displacement_reader;
		//跨越状态记录（键为碰撞体编号，记录上一次判定所得的跨越状态）
		std::unordered_map<uint64_t, cross_state> cross_states;
		//本帧跨越通知（检测时收集，供碰撞代理器发布后取走）
		std::vector<Cross_Notice> cross_notices;
	public:
		//默认构造
		Collision_Region();
		//含参构造(指定碰撞空间名称)
		explicit Collision_Region(const std::string& region_name);

		//禁止拷贝（backend 含 unique_ptr）
		Collision_Region(const Collision_Region&) = delete;
		Collision_Region& operator=(const Collision_Region&) = delete;

		/*
		默认移动
		注意：碰撞世界内部记录的是碰撞对象地址，而空间边界是直接成员，
		      故向碰撞世界挂入边界之后再按值搬移本结构体会留下悬空指针。
		      按 unique_ptr 搬移（碰撞代理器的做法）不受影响，
		      确需按值搬移时须搬移后重新执行边界构建。
		*/
		Collision_Region(Collision_Region&&) = default;
		Collision_Region& operator=(Collision_Region&&) = default;

		/*
		析构函数
		碰撞体与空间边界都直接持有碰撞对象，而碰撞世界内部记录的是这些对象的地址；
		碰撞体映射先于碰撞世界析构，故须在此先把全部碰撞对象移出碰撞世界，
		否则碰撞世界析构时会解引用已释放的碰撞对象。
		*/
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
		//卸载碰卸载
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
		std::vector<uint64_t> colliders(void);
		//碰撞体接管(按指定编号登记，供空间间转移使用)
		bool collider_adopt(uint64_t collider_ID);
		//碰撞体几何配置读取(编号不存在时返回空配置)
		nlohmann::json collider_geometry(uint64_t collider_ID) const;
		//位移事件读取回调注入(由碰撞代理器注入)
		void displacement_reader_set(std::function<bool(uint64_t, Vector3&)> reader);
		//碰撞体位移作废(碰撞响应判定为停止运动)
		bool collider_displacement_void(uint64_t collider_ID);
		//碰撞体位移改写(碰撞响应判定为继续运动且位移变化)
		bool collider_displacement_replace(uint64_t collider_ID, const Vector3& displacement);

		//执行碰撞检测
		std::optional<std::vector<Collision_Result>> detect(void);
		//碰撞体包含性检测
		bool contains(uint64_t collider_ID) const;
		//跨越通知取走(取出并清空本帧收集的跨越通知)
		std::vector<Cross_Notice> cross_notices_take(void);
	private:
		//碰撞体查找
		Collider* collider_seek(uint64_t collider_ID);

		//浮点字段读取
		bool scalar_read(const nlohmann::json& config, const std::string& field, double& receiver);
		//三元数组字段读取
		bool vector_read(const nlohmann::json& config, const std::string& field, Vector3& receiver);
		//四元数组字段读取
		bool quaternion_read(const nlohmann::json& config, const std::string& field,
			Quaternion& receiver);

		//网格形状构建
		bool mesh_shape_build(const std::string& mesh_path,
			std::unique_ptr<Collision_Shape>& shape_receiver,
			std::unique_ptr<Triangle_Mesh>& mesh_receiver);
		//几何形状构建
		bool shape_build(const nlohmann::json& geometry_config,
			std::unique_ptr<Collision_Shape>& shape_receiver,
			std::unique_ptr<Triangle_Mesh>& mesh_receiver);
		//几何体集合构建(单几何体形式与集合形式统一处理，多部件时装配复合形状)
		bool geometry_build(const nlohmann::json& geometry_config,
			std::vector<Geometry_Part>& parts_receiver,
			std::unique_ptr<Collision_Shape>& compound_receiver);

		//边界点包含性判定(射线奇偶：与边界网格交点数为奇数则点在边界内部)
		bool boundary_point_inside(const Vector3& point) const;
		//跨越状态更新(位置更新后比对旧状态并收集跨越通知)
		void cross_state_update(const std::vector<uint64_t>& boundary_contacts);
	};

}
