//碰撞体测试：前端数据、后端标签、几何装配与移动语义
#include <gtest/gtest.h>

//获取碰撞体（含碰撞体后端句柄）
#include "Engine/EngineSystem/src/collision/Collider/碰撞体.h"
//获取碰撞体后端公开接口
#include "Engine/EngineSystem/src/collision/Collider/backend/碰撞体后端.h"

namespace
{
	//构造盒体几何配置
	nlohmann::json box_geometry(double position_X)
	{
		//几何配置
		nlohmann::json config = nlohmann::json::object();
		//写入形状类型与半长
		config["type"] = "box";
		config["half_extent"] = nlohmann::json::array({ 1.0, 1.0, 1.0 });
		//写入基准位置
		config["position"] = nlohmann::json::array({ position_X, 0.0, 0.0 });
		return config;
	}
}

//碰撞体搬移测试夹具（独立夹具名，避免与其余套件重名）
class Collider_Move_Test : public ::testing::Test
{
};

// ———— 默认取值 ————

//默认编号为零
TEST_F(Collider_Move_Test, 默认编号为零)
{
	//默认构造的碰撞体
	engine::Collider collider;
	//编号初值为零
	EXPECT_EQ(collider.ID, 0u);
}

//默认运动平移为零向量
TEST_F(Collider_Move_Test, 默认运动平移为零)
{
	//默认构造的碰撞体
	engine::Collider collider;
	//三个分量均为零
	EXPECT_FLOAT_EQ(collider.motion.translation.x(), 0.0f);
	EXPECT_FLOAT_EQ(collider.motion.translation.y(), 0.0f);
	EXPECT_FLOAT_EQ(collider.motion.translation.z(), 0.0f);
}

//默认运动为空
TEST_F(Collider_Move_Test, 默认运动为空)
{
	//默认构造的碰撞体
	engine::Collider collider;
	//默认无可施加运动
	EXPECT_TRUE(collider.motion.empty());
	//默认无部件运动
	EXPECT_TRUE(collider.part_motions.empty());
}

//默认检测方式为非扫掠且步长为零
TEST_F(Collider_Move_Test, 默认检测方式)
{
	//默认构造的碰撞体
	engine::Collider collider;
	//非扫掠检测
	EXPECT_FALSE(collider.detection_mode.is_swept_volume);
	//离散步长为零
	EXPECT_EQ(collider.detection_mode.step_length, 0u);
}

//默认豁免标记为零
TEST_F(Collider_Move_Test, 默认豁免标记为零)
{
	//默认构造的碰撞体
	engine::Collider collider;
	//未设置任何豁免分组
	EXPECT_EQ(collider.exemption_flag, 0u);
}

//默认几何配置为空对象
TEST_F(Collider_Move_Test, 默认几何配置为空)
{
	//默认构造的碰撞体
	engine::Collider collider;
	//几何配置尚未写入
	EXPECT_TRUE(collider.geometry.is_null() || collider.geometry.empty());
}

//默认无后端句柄缺失且后端标签为零
TEST_F(Collider_Move_Test, 默认后端标签为零)
{
	//默认构造的碰撞体
	engine::Collider collider;
	//后端应已分配
	ASSERT_NE(collider.backend, nullptr);
	//编号标签为零
	EXPECT_EQ(collider.backend->tag_ID(), 0u);
	//非空间边界
	EXPECT_FALSE(collider.backend->boundary_check());
}

//默认未挂载几何体
TEST_F(Collider_Move_Test, 默认未挂载几何体)
{
	//默认构造的碰撞体
	engine::Collider collider;
	//几何挂载状态为假
	EXPECT_FALSE(collider.geometry_mounted());
}

// ———— 后端标签与几何装配 ————

//后端标签可写入并读回
TEST_F(Collider_Move_Test, 后端标签可写入)
{
	//默认构造的碰撞体
	engine::Collider collider;
	//写入标签
	collider.backend->tag_set(17u, true);
	//标签与边界标记均可读回
	EXPECT_EQ(collider.backend->tag_ID(), 17u);
	EXPECT_TRUE(collider.backend->boundary_check());
}

//盒体几何可构建并挂载
TEST_F(Collider_Move_Test, 盒体几何可挂载)
{
	//默认构造的碰撞体
	engine::Collider collider;
	//构建盒体几何
	ASSERT_TRUE(collider.backend->geometry_build(box_geometry(0.0),
		engine::Transform::getIdentity()));
	//尚未挂载时状态为假
	EXPECT_FALSE(collider.geometry_mounted());
	//挂载几何
	ASSERT_TRUE(collider.backend->geometry_mount());
	//挂载状态为真
	EXPECT_TRUE(collider.geometry_mounted());

	//世界包围盒
	engine::Vector3 aabb_min, aabb_max;
	//读取包围盒
	collider.backend->aabb_get(aabb_min, aabb_max);
	//半长为一的盒体包围盒约为一（含碰撞边距）
	EXPECT_NEAR(aabb_max.x(), 1.0f, 0.1f);
	EXPECT_NEAR(aabb_min.x(), -1.0f, 0.1f);
}

//多部件几何装配为复合形状
TEST_F(Collider_Move_Test, 多部件几何可挂载)
{
	//默认构造的碰撞体
	engine::Collider collider;
	//几何体集合配置
	nlohmann::json config = nlohmann::json::object();
	config["geometries"] = nlohmann::json::array({ box_geometry(-5.0), box_geometry(5.0) });
	//构建并挂载
	ASSERT_TRUE(collider.backend->geometry_build(config, engine::Transform::getIdentity()));
	ASSERT_TRUE(collider.backend->geometry_mount());

	//世界包围盒
	engine::Vector3 aabb_min, aabb_max;
	//读取包围盒
	collider.backend->aabb_get(aabb_min, aabb_max);
	//两个部件分别位于正负五个单位处
	EXPECT_LT(aabb_min.x(), -5.9f);
	EXPECT_GT(aabb_max.x(), 5.9f);
}

//非法几何构建不改变已挂载几何
TEST_F(Collider_Move_Test, 非法几何不改变已挂载几何)
{
	//默认构造的碰撞体
	engine::Collider collider;
	//先挂载合法盒体
	ASSERT_TRUE(collider.backend->geometry_build(box_geometry(0.0),
		engine::Transform::getIdentity()));
	ASSERT_TRUE(collider.backend->geometry_mount());
	//再以非法配置构建
	EXPECT_FALSE(collider.backend->geometry_build(nlohmann::json::object(),
		engine::Transform::getIdentity()));
	//原有几何仍处于挂载状态
	EXPECT_TRUE(collider.geometry_mounted());
}

//几何清空后不再挂载
TEST_F(Collider_Move_Test, 几何清空后不再挂载)
{
	//默认构造的碰撞体
	engine::Collider collider;
	//构建并挂载
	ASSERT_TRUE(collider.backend->geometry_build(box_geometry(0.0),
		engine::Transform::getIdentity()));
	ASSERT_TRUE(collider.backend->geometry_mount());
	//清空几何
	collider.backend->geometry_clear();
	//挂载状态为假
	EXPECT_FALSE(collider.geometry_mounted());
	//包围盒归零
	engine::Vector3 aabb_min, aabb_max;
	collider.backend->aabb_get(aabb_min, aabb_max);
	EXPECT_FLOAT_EQ(aabb_max.x(), 0.0f);
}

//世界变换可写入并推进
TEST_F(Collider_Move_Test, 世界变换可推进)
{
	//默认构造的碰撞体
	engine::Collider collider;
	//写入变换
	engine::Transform transform;
	transform.setOrigin(engine::Vector3(1.0f, 2.0f, 3.0f));
	collider.backend->world_transform_set(transform);
	//读回变换
	engine::Transform read_back = collider.backend->world_transform_get();
	EXPECT_FLOAT_EQ(read_back.getOrigin().x(), 1.0f);
	//平移推进
	collider.backend->origin_advance(engine::Vector3(4.0f, 0.0f, 0.0f));
	//起点应叠加平移
	read_back = collider.backend->world_transform_get();
	EXPECT_FLOAT_EQ(read_back.getOrigin().x(), 5.0f);
}

// ———— 移动语义 ————

//移动构造搬移编号与后端句柄
TEST_F(Collider_Move_Test, 移动构造搬移编号与后端)
{
	//源碰撞体
	engine::Collider source;
	//写入编号与标签
	source.ID = 21u;
	source.backend->tag_set(21u, false);
	//记录后端地址
	engine::Collider_Backend* backend_address = source.backend.get();
	//移动构造
	engine::Collider target = std::move(source);
	//编号随对象一并搬移
	EXPECT_EQ(target.ID, 21u);
	//后端地址不变
	EXPECT_EQ(target.backend.get(), backend_address);
	//标签保持
	EXPECT_EQ(target.backend->tag_ID(), 21u);
	//源对象后端句柄已清空
	EXPECT_EQ(source.backend, nullptr);
}

//移动构造搬移运动与检测配置
TEST_F(Collider_Move_Test, 移动构造搬移运动与检测配置)
{
	//源碰撞体
	engine::Collider source;
	//写入整体运动、部件运动、检测方式、豁免标记
	source.motion.translation = engine::Vector3(1.5f, -2.5f, 3.5f);
	source.motion.rotation = engine::Quaternion::from_axis_angle(
		engine::Vector3(0.0f, 0.0f, 1.0f), 0.5f);
	engine::Part_Motion part_motion;
	part_motion.part = 1u;
	part_motion.motion.translation = engine::Vector3(0.0f, 0.0f, 2.0f);
	source.part_motions.push_back(part_motion);
	source.detection_mode.is_swept_volume = true;
	source.detection_mode.step_length = 5u;
	source.exemption_flag = 0xFFu;
	//移动构造
	engine::Collider target = std::move(source);
	//各字段随对象搬移
	EXPECT_FLOAT_EQ(target.motion.translation.x(), 1.5f);
	EXPECT_FALSE(target.motion.rotation.identity_check());
	ASSERT_EQ(target.part_motions.size(), 1u);
	EXPECT_EQ(target.part_motions[0].part, 1u);
	EXPECT_FLOAT_EQ(target.part_motions[0].motion.translation.z(), 2.0f);
	EXPECT_TRUE(target.detection_mode.is_swept_volume);
	EXPECT_EQ(target.detection_mode.step_length, 5u);
	EXPECT_EQ(target.exemption_flag, 0xFFu);
}

//移动赋值搬移编号并重挂后端标签
TEST_F(Collider_Move_Test, 移动赋值搬移编号与后端)
{
	//源碰撞体
	engine::Collider source;
	//写入编号与标签
	source.ID = 33u;
	source.backend->tag_set(33u, false);
	//记录后端地址
	engine::Collider_Backend* backend_address = source.backend.get();
	//目标碰撞体
	engine::Collider target;
	//移动赋值
	target = std::move(source);
	//编号与后端随赋值搬移
	EXPECT_EQ(target.ID, 33u);
	EXPECT_EQ(target.backend.get(), backend_address);
	EXPECT_EQ(target.backend->tag_ID(), 33u);
	//源对象后端句柄已清空
	EXPECT_EQ(source.backend, nullptr);
}

//自移动赋值保持原有状态
TEST_F(Collider_Move_Test, 自移动赋值保持状态)
{
	//被测碰撞体
	engine::Collider collider;
	//写入编号
	collider.ID = 9u;
	//记录后端地址
	engine::Collider_Backend* backend_address = collider.backend.get();
	//自移动赋值
	collider = std::move(collider);
	//编号与后端均保持
	EXPECT_EQ(collider.ID, 9u);
	EXPECT_EQ(collider.backend.get(), backend_address);
}
