#pragma once
//预编译头
#include "Engine/EngineFoundation/common/前置头文件包含.h"
//获取 JSON 解析外部库
#include <nlohmann/json.hpp>
//获取自有几何类型
#include "../../Common/几何类型.h"
//获取运动量类型
#include "../../Common/运动类型.h"

namespace engine
{
	//向后声明内部访问器
	struct Collider_Backend_Access;

	/*
	碰撞体后端
	负责碰撞对象与几何形状的全部装配与持有，前端只经本类的公开方法接触几何与变换；
	实现体在堆上分配，句柄移动不改变碰撞对象地址，故碰撞库回调挂载的自指针始终有效。
	*/
	class Collider_Backend
	{
	public:
		//构造函数
		Collider_Backend();
		//析构函数（若仍挂载于碰撞空间则自动摘除）
		~Collider_Backend();

		//禁止拷贝
		Collider_Backend(const Collider_Backend&) = delete;
		Collider_Backend& operator=(const Collider_Backend&) = delete;
		//移动构造/赋值（实现体地址不变）
		Collider_Backend(Collider_Backend&& other) noexcept;
		Collider_Backend& operator=(Collider_Backend&& other) noexcept;

		//身份标签设置（碰撞体编号与是否空间边界）
		void tag_set(uint64_t collider_ID, bool is_boundary);
		//身份标签读取
		uint64_t tag_ID(void) const;
		//空间边界判定
		bool boundary_check(void) const;

		/*
		几何构建（暂存）
		按几何配置在暂存区装配部件与复合形状，不触碰当前几何；
		失败时保留当前几何不变，成功后再由 geometry_mount 替换。
		*/
		bool geometry_build(const nlohmann::json& geometry_config, const Transform& base_transform);
		//几何挂载（把暂存区装配结果替换为当前几何并写入世界变换）
		bool geometry_mount(void);
		//几何清空（同时清空当前与暂存）
		void geometry_clear(void);
		//几何挂载状态
		bool geometry_mounted(void) const;
		//几何部件数量
		uint32_t part_count(void) const;
		//子部件编号存在性检查
		bool part_exists(uint16_t part_ID) const;

		//豁免标记设置（供宽阶段过滤读取）
		void exemption_flag_set(uint64_t flag);
		//豁免标记读取
		uint64_t exemption_flag_get(void) const;
		//子部件对称忽略对设置（重复方向自动归一）
		bool part_ignore_set(const std::vector<std::pair<uint16_t, uint16_t>>& pairs);

		//根变换写入（并刷新全部子部件世界变换）
		void world_transform_set(const Transform& transform);
		//根变换读取
		Transform world_transform_get(void) const;
		//根变换平移推进
		void origin_advance(const Vector3& segment);
		//整体运动推进（绕自身原点旋转后按世界轴平移）
		bool motion_advance(const Motion& motion);
		//部件运动推进（按子部件编号在碰撞体局部空间内绕部件原点旋转后按局部轴平移）
		bool part_motion_advance(uint16_t part_ID, const Motion& motion);
		//世界包围盒读取（全部子部件并集，未挂载几何时返回零值）
		void aabb_get(Vector3& aabb_min, Vector3& aabb_max) const;
		//静态对象标记设置
		void static_flag_set(bool is_static);
	private:
		//实现体
		struct Impl;
		std::unique_ptr<Impl> impl;

		//仅允许内部访问器接触实现体
		friend struct Collider_Backend_Access;
	};
}
