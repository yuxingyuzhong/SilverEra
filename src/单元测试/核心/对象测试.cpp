//对象基类测试：覆盖默认初值、ID 读写、有效性读写与派生能力
#include <gtest/gtest.h>

//获取对象基类
#include "Engine/EngineCore/src/core/object/Object/对象.h"

//派生测试对象
class Test_Object : public engine::Object
{
public:
	//附带字段，供对象池排序投影使用
	uint64_t payload = 0;
};

//对象测试夹具
class Object_Test : public ::testing::Test
{
};

//默认构造：ID 为零、有效标记为假
TEST_F(Object_Test, 默认构造为非法实体)
{
	//默认构造的对象
	engine::Object object;
	//ID 应为零
	EXPECT_EQ(object.ID(), 0u);
	//有效标记应为假
	EXPECT_FALSE(object.valid());
}

//ID 读写：设置后可按原值取回
TEST_F(Object_Test, ID读写一致)
{
	//默认构造的对象
	engine::Object object;
	//设置对象ID
	object.ID_set(12345);
	//取回值应与设置值一致
	EXPECT_EQ(object.ID(), 12345u);
}

//有效性读写：设置后可按原值取回
TEST_F(Object_Test, 有效性读写一致)
{
	//默认构造的对象
	engine::Object object;
	//设置为有效
	object.valid_set(true);
	//取回值应为真
	EXPECT_TRUE(object.valid());
	//再设置为无效
	object.valid_set(false);
	//取回值应为假
	EXPECT_FALSE(object.valid());
}

//派生能力：派生类可直接使用基类的ID与有效性接口
TEST_F(Object_Test, 派生类沿用基类接口)
{
	//派生测试对象
	Test_Object object;
	//沿用基类接口设置ID
	object.ID_set(7);
	//沿用基类接口设置有效标记
	object.valid_set(true);
	//可同时持有派生字段
	object.payload = 99;
	//基类ID应取回
	EXPECT_EQ(object.ID(), 7u);
	//基类有效性应取回
	EXPECT_TRUE(object.valid());
	//派生字段应保留
	EXPECT_EQ(object.payload, 99u);
}

//边界ID：零与64位极大值均可原样读写
TEST_F(Object_Test, ID设置为零与极大值)
{
	//默认构造的对象
	engine::Object object;
	//先设置一个普通ID
	object.ID_set(12345);
	//再改设为零
	object.ID_set(0);
	//零值应能原样取回
	EXPECT_EQ(object.ID(), 0u);
	//设置为64位无符号极大值
	object.ID_set(UINT64_MAX);
	//极大值应能原样取回
	EXPECT_EQ(object.ID(), UINT64_MAX);
}

//有效标记：反复切换后每次都能按设置值取回
TEST_F(Object_Test, 有效标记反复切换保持一致)
{
	//默认构造的对象
	engine::Object object;
	//连续多轮切换
	for (int round = 0; round < 4; round++)
	{
		//置为有效
		object.valid_set(true);
		//此时应取回真
		EXPECT_TRUE(object.valid());
		//置为无效
		object.valid_set(false);
		//此时应取回假
		EXPECT_FALSE(object.valid());
	}
}

//派生能力：两个派生实例的ID与有效标记互不影响
TEST_F(Object_Test, 派生类独立设置ID与有效标记)
{
	//两个派生测试对象
	Test_Object first;
	Test_Object second;
	//分别设置不同的ID
	first.ID_set(11);
	second.ID_set(22);
	//仅将第一个置为有效
	first.valid_set(true);
	second.valid_set(false);
	//两者ID应互不影响
	EXPECT_EQ(first.ID(), 11u);
	EXPECT_EQ(second.ID(), 22u);
	//两者有效标记应互不影响
	EXPECT_TRUE(first.valid());
	EXPECT_FALSE(second.valid());
}

//基类指针：经基类指针访问派生对象的ID与有效标记接口
TEST_F(Object_Test, 基类指针访问派生对象接口)
{
	//派生测试对象
	Test_Object object;
	//以基类指针指向派生对象
	engine::Object* base = &object;
	//经基类指针设置ID
	base->ID_set(4321);
	//经基类指针设置有效标记
	base->valid_set(true);
	//经基类指针应能读回ID
	EXPECT_EQ(base->ID(), 4321u);
	//经基类指针应能读回有效标记
	EXPECT_TRUE(base->valid());
	//直接经派生对象读取应一致
	EXPECT_EQ(object.ID(), 4321u);
}

//默认构造：ID为零且有效标记为假
TEST_F(Object_Test, 默认构造对象ID为零)
{
	//默认构造的对象
	engine::Object object;
	//默认ID应恰为零
	EXPECT_EQ(object.ID(), 0u);
	//默认有效标记应为假
	EXPECT_FALSE(object.valid());
	//另一个默认构造对象的ID同样为零
	engine::Object another;
	EXPECT_EQ(another.ID(), 0u);
}

//ID与有效标记：两者相互独立，互不干扰
TEST_F(Object_Test, ID与有效标记相互独立)
{
	//默认构造的对象
	engine::Object object;
	//仅设置ID，不改变有效标记
	object.ID_set(99);
	//有效标记应仍为默认的假
	EXPECT_FALSE(object.valid());
	//接着设置有效标记，不改变ID
	object.valid_set(true);
	//ID应保持此前设置的值
	EXPECT_EQ(object.ID(), 99u);
	//清除有效标记后ID仍应保持
	object.valid_set(false);
	EXPECT_EQ(object.ID(), 99u);
}