//碰撞体测试：覆盖默认取值、自指针挂载、搬移语义与挂载形状选择
#include <gtest/gtest.h>

//获取碰撞体（含碰撞对象与几何形状）
#include "Engine/EngineCore/src/core/spatial/collision/Collider/碰撞体.h"

//碰撞体搬移测试夹具（独立夹具名，避免与碰撞测试内的夹具重名）
class Collider_Move_Test : public ::testing::Test
{
public:
	//以组件顺序构造一个带单个盒体部件的碰撞体
	static void attach_box_part(engine::Collider& collider)
	{
		//单个几何体部件
		engine::Geometry_Part part;
		//挂载盒体形状（半长为一）
		//子弹库形状类只提供对齐分配算子，无法使用 new(nothrow)，与引擎内分配方式保持一致
		part.shape.reset(new engine::Box_Shape(engine::Vector3(1.0f, 1.0f, 1.0f)));
		//并入部件集合
		collider.parts.push_back(std::move(part));
	}
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

//默认位移向量为零向量
TEST_F(Collider_Move_Test, 默认位移向量为零)
{
	//默认构造的碰撞体
	engine::Collider collider;
	//三个分量均为零
	EXPECT_FLOAT_EQ(collider.displacement_vector.x(), 0.0f);
	EXPECT_FLOAT_EQ(collider.displacement_vector.y(), 0.0f);
	EXPECT_FLOAT_EQ(collider.displacement_vector.z(), 0.0f);
}

//默认位移作废标记为假
TEST_F(Collider_Move_Test, 默认位移未作废)
{
	//默认构造的碰撞体
	engine::Collider collider;
	//默认可施加位移
	EXPECT_FALSE(collider.displacement_invalid);
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

// ———— 自指针挂载与反查 ————

//默认构造即挂载自指针，可直接反查
TEST_F(Collider_Move_Test, 默认构造可反查)
{
	//默认构造的碰撞体
	engine::Collider collider;
	//反查应指回自身
	EXPECT_EQ(engine::Collider::recover(&collider.object), &collider);
}

//无几何体时挂载形状为空
TEST_F(Collider_Move_Test, 无几何体时挂载形状为空)
{
	//默认构造的碰撞体
	engine::Collider collider;
	//既无复合形状也无部件
	EXPECT_EQ(collider.mounted_shape(), nullptr);
}

//单部件时挂载形状取部件形状本体
TEST_F(Collider_Move_Test, 单部件挂载形状取本体)
{
	//默认构造的碰撞体
	engine::Collider collider;
	//挂载单个盒体部件
	attach_box_part(collider);
	//挂载形状应为该部件形状本体
	ASSERT_NE(collider.mounted_shape(), nullptr);
	EXPECT_EQ(collider.mounted_shape(), collider.parts.front().shape.get());
}

//存在复合形状时优先于部件形状
TEST_F(Collider_Move_Test, 复合形状优先挂载)
{
	//默认构造的碰撞体
	engine::Collider collider;
	//先挂载一个部件
	attach_box_part(collider);
	//再挂载复合形状
	collider.compound.reset(new engine::Compound_Shape());
	//挂载形状应优先取复合形状
	ASSERT_NE(collider.mounted_shape(), nullptr);
	EXPECT_EQ(collider.mounted_shape(), collider.compound.get());
}

// ———— 移动构造 ————

//移动构造搬移编号
TEST_F(Collider_Move_Test, 移动构造搬移编号)
{
	//源碰撞体
	engine::Collider source;
	//写入编号
	source.ID = 21u;
	//移动构造
	engine::Collider target = std::move(source);
	//编号随对象一并搬移
	EXPECT_EQ(target.ID, 21u);
}

//移动构造重挂自指针
TEST_F(Collider_Move_Test, 移动构造重挂自指针)
{
	//源碰撞体
	engine::Collider source;
	//移动构造
	engine::Collider target = std::move(source);
	//目标对象可反查回目标本体
	EXPECT_EQ(engine::Collider::recover(&target.object), &target);
}

//移动构造清空源自指针
TEST_F(Collider_Move_Test, 移动构造清空源对象自指针)
{
	//源碰撞体
	engine::Collider source;
	//移动构造
	engine::Collider target = std::move(source);
	//源对象自指针被清空，反查返回空
	EXPECT_EQ(engine::Collider::recover(&source.object), nullptr);
}

//移动构造搬移位移向量
TEST_F(Collider_Move_Test, 移动构造搬移位移向量)
{
	//源碰撞体
	engine::Collider source;
	//写入位移向量
	source.displacement_vector = engine::Vector3(1.5f, -2.5f, 3.5f);
	//移动构造
	engine::Collider target = std::move(source);
	//三分量均随对象搬移
	EXPECT_FLOAT_EQ(target.displacement_vector.x(), 1.5f);
	EXPECT_FLOAT_EQ(target.displacement_vector.y(), -2.5f);
	EXPECT_FLOAT_EQ(target.displacement_vector.z(), 3.5f);
}

//移动构造搬移位移作废标记
TEST_F(Collider_Move_Test, 移动构造搬移位移作废标记)
{
	//源碰撞体
	engine::Collider source;
	//置位位移作废标记
	source.displacement_invalid = true;
	//移动构造
	engine::Collider target = std::move(source);
	//作废标记随对象搬移
	EXPECT_TRUE(target.displacement_invalid);
}

//移动构造搬移检测方式
TEST_F(Collider_Move_Test, 移动构造搬移检测方式)
{
	//源碰撞体
	engine::Collider source;
	//写入扫掠检测方式
	source.detection_mode.is_swept_volume = true;
	source.detection_mode.step_length = 5u;
	//移动构造
	engine::Collider target = std::move(source);
	//扫掠标记与步长均随对象搬移
	EXPECT_TRUE(target.detection_mode.is_swept_volume);
	EXPECT_EQ(target.detection_mode.step_length, 5u);
}

//移动构造搬移豁免标记
TEST_F(Collider_Move_Test, 移动构造搬移豁免标记)
{
	//源碰撞体
	engine::Collider source;
	//写入豁免分组
	source.exemption_flag = 0xFFu;
	//移动构造
	engine::Collider target = std::move(source);
	//豁免标记随对象搬移
	EXPECT_EQ(target.exemption_flag, 0xFFu);
}

//移动构造搬移部件集合
TEST_F(Collider_Move_Test, 移动构造搬移部件集合)
{
	//源碰撞体
	engine::Collider source;
	//挂载一个部件
	attach_box_part(source);
	//记录部件形状指针
	engine::Collision_Shape* shape = source.parts.front().shape.get();
	//移动构造
	engine::Collider target = std::move(source);
	//部件随对象搬移且形状指针不变
	ASSERT_EQ(target.parts.size(), 1u);
	EXPECT_EQ(target.parts.front().shape.get(), shape);
}

// ———— 移动赋值 ————

//移动赋值搬移编号并重挂自指针
TEST_F(Collider_Move_Test, 移动赋值重挂自指针)
{
	//源碰撞体
	engine::Collider source;
	//写入编号
	source.ID = 33u;
	//目标碰撞体
	engine::Collider target;
	//移动赋值
	target = std::move(source);
	//编号搬移
	EXPECT_EQ(target.ID, 33u);
	//目标自指针重挂回目标本体
	EXPECT_EQ(engine::Collider::recover(&target.object), &target);
	//源自指针被清空
	EXPECT_EQ(engine::Collider::recover(&source.object), nullptr);
}

//移动赋值搬移位移向量
TEST_F(Collider_Move_Test, 移动赋值搬移位移向量)
{
	//源碰撞体
	engine::Collider source;
	//写入位移向量
	source.displacement_vector = engine::Vector3(-4.0f, 5.0f, 6.0f);
	//目标碰撞体
	engine::Collider target;
	//移动赋值
	target = std::move(source);
	//分量随赋值搬移
	EXPECT_FLOAT_EQ(target.displacement_vector.x(), -4.0f);
	EXPECT_FLOAT_EQ(target.displacement_vector.y(), 5.0f);
	EXPECT_FLOAT_EQ(target.displacement_vector.z(), 6.0f);
}

//自移动赋值：自指针保持有效
TEST_F(Collider_Move_Test, 自移动赋值保持自指针)
{
	//被测碰撞体
	engine::Collider collider;
	//写入编号
	collider.ID = 9u;
	//自移动赋值
	collider = std::move(collider);
	//自指针未被破坏
	EXPECT_EQ(engine::Collider::recover(&collider.object), &collider);
	//编号保持原值
	EXPECT_EQ(collider.ID, 9u);
}

//空自指针反查：返回空指针
TEST_F(Collider_Move_Test, 空自指针反查为空)
{
	//默认构造的碰撞体
	engine::Collider collider;
	//手动清空自指针
	collider.object.setUserPointer(nullptr);
	//反查应返回空
	EXPECT_EQ(engine::Collider::recover(&collider.object), nullptr);
}