//对象基类测试：覆盖默认初值、经对象池分配后的 ID / 有效性、派生能力与基类指针访问
//说明：Object 的 ID 与有效标记自第九十七轮起改为私有（仅 Object_Pool 可写），
//      故本文件统一经对象池构建来观察写入结果，不再直接调用被移除的公开 setter。
#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

//获取对象基类
#include "Engine/EngineCore/src/core/object/Object/对象.h"
//获取对象池（负责分配 ID 与有效标记）
#include "Engine/EngineCore/src/core/object/Object_Pool/对象池.h"

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

//对象池构建：分配的 ID 可从对象取回，且有效标记为真
TEST_F(Object_Test, 对象池构建后ID与有效性一致)
{
	//对象池
	engine::Object_Pool<engine::Object> pool;
	//构建一个对象并取回其 ID
	uint64_t id = pool.build();
	//对象池分配的 ID 自非零起
	EXPECT_GE(id, 1u);
	//按 ID 查找对象
	auto it = pool.find(id);
	ASSERT_NE(it, pool.end());
	//取回 ID 与有效标记
	EXPECT_EQ(it->ID(), id);
	EXPECT_TRUE(it->valid());
}

//派生能力：派生字段随对象保留，ID 由对象池分配
TEST_F(Object_Test, 派生类字段与基类接口共存)
{
	//派生测试对象（带派生字段）
	Test_Object object;
	object.payload = 99;
	//对象池
	engine::Object_Pool<Test_Object> pool;
	//构建并取回 ID
	uint64_t id = pool.build(object);
	//按 ID 查找对象
	auto it = pool.find(id);
	ASSERT_NE(it, pool.end());
	//基类 ID 与有效性应正确
	EXPECT_EQ(it->ID(), id);
	EXPECT_TRUE(it->valid());
	//派生字段应保留
	EXPECT_EQ(it->payload, 99u);
}

//多对象：各自 ID 唯一且均有效
TEST_F(Object_Test, 多对象ID唯一且有效)
{
	//对象池
	engine::Object_Pool<Test_Object> pool;
	//批量构建两个对象
	std::vector<uint64_t> ids = pool.build(std::vector<Test_Object>{ Test_Object{}, Test_Object{} });
	ASSERT_EQ(ids.size(), 2u);
	//ID 互不相同
	EXPECT_NE(ids[0], ids[1]);
	//两个对象均可按 ID 找到且有效
	for (uint64_t id : ids)
	{
		auto it = pool.find(id);
		ASSERT_NE(it, pool.end());
		EXPECT_TRUE(it->valid());
	}
}

//卸载：目标对象不再可按 ID 找到（有效标记已清除）
TEST_F(Object_Test, 卸载后对象不可查找)
{
	//对象池与一个对象
	engine::Object_Pool<Test_Object> pool;
	uint64_t id = pool.build(Test_Object{});
	ASSERT_NE(pool.find(id), pool.end());
	//卸载目标对象
	pool.unload(id);
	//卸载后不可再找到
	EXPECT_EQ(pool.find(id), pool.end());
	//合法对象数归零
	EXPECT_EQ(pool.size(), 0u);
}

//基类指针：经基类指针读取派生对象的 ID 与有效标记
TEST_F(Object_Test, 基类指针访问派生对象接口)
{
	//对象池与一个对象
	engine::Object_Pool<Test_Object> pool;
	uint64_t id = pool.build(Test_Object{});
	//以基类指针指向池内对象
	auto it = pool.find(id);
	ASSERT_NE(it, pool.end());
	engine::Object* base = &*it;
	//经基类指针读回 ID 与有效标记
	EXPECT_EQ(base->ID(), id);
	EXPECT_TRUE(base->valid());
	//直接经派生对象读取应一致
	EXPECT_EQ(it->ID(), id);
}

//独立性：卸载一个对象不影响另一个对象的 ID 与有效标记
TEST_F(Object_Test, 多对象卸载互不影响)
{
	//对象池与两个对象
	engine::Object_Pool<Test_Object> pool;
	std::vector<uint64_t> ids = pool.build(std::vector<Test_Object>{ Test_Object{}, Test_Object{} });
	ASSERT_EQ(ids.size(), 2u);
	uint64_t keep_id = ids[0];
	uint64_t remove_id = ids[1];
	//卸载第二个对象
	pool.unload(remove_id);
	//第一个对象仍可按 ID 找到且有效
	auto it = pool.find(keep_id);
	ASSERT_NE(it, pool.end());
	EXPECT_EQ(it->ID(), keep_id);
	EXPECT_TRUE(it->valid());
	//第二个对象不可再找到
	EXPECT_EQ(pool.find(remove_id), pool.end());
	//合法对象数剩一个
	EXPECT_EQ(pool.size(), 1u);
}
