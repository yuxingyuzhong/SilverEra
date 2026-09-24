//对象基类测试：覆盖默认初值、ID 读写、有效性读写与派生能力
#include <gtest/gtest.h>

//获取对象基类
#include "src/core/object/Object/对象.h"

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