#include <gtest/gtest.h>
#include "ECS测试组件.h"

//实体句柄：打包与解析
TEST(ECS_Entity_Test, 句柄打包解析)
{
	//构造指定槽位与代际的句柄
	engine::Entity entity = engine::entity_make(7, 3);
	//槽位与代际应可还原
	EXPECT_EQ(entity.index(), 7u);
	EXPECT_EQ(entity.generation(), 3u);
	EXPECT_FALSE(entity.empty());
	//默认句柄为空
	EXPECT_TRUE(engine::Entity{}.empty());
}

//实体创建：返回互不相同的有效句柄
TEST(ECS_Entity_Test, 创建返回唯一句柄)
{
	//世界
	engine::ECS_Store store;
	//创建两个实体
	engine::Entity first = store.entity_create();
	engine::Entity second = store.entity_create();
	EXPECT_FALSE(first.empty());
	EXPECT_NE(first.value, second.value);
	EXPECT_TRUE(store.entity_alive(first));
	EXPECT_TRUE(store.entity_alive(second));
	EXPECT_EQ(store.entity_size(), 2u);
}

//实体销毁：句柄立即失效且重复销毁返回假
TEST(ECS_Entity_Test, 销毁后句柄失效)
{
	//世界
	engine::ECS_Store store;
	//创建一个实体
	engine::Entity entity = store.entity_create();
	EXPECT_TRUE(store.entity_destroy(entity));
	EXPECT_FALSE(store.entity_alive(entity));
	EXPECT_FALSE(store.entity_destroy(entity));
	EXPECT_EQ(store.entity_size(), 0u);
}

//槽位复用：新实体占用旧槽位但代际不同，旧句柄仍失效
TEST(ECS_Entity_Test, 槽位复用后代际变化)
{
	//世界
	engine::ECS_Store store;
	//创建并销毁首个实体
	engine::Entity first = store.entity_create();
	EXPECT_TRUE(store.entity_destroy(first));
	//再次创建应复用槽位
	engine::Entity second = store.entity_create();
	EXPECT_EQ(second.index(), first.index());
	EXPECT_NE(second.generation(), first.generation());
	EXPECT_FALSE(store.entity_alive(first));
	EXPECT_TRUE(store.entity_alive(second));
}
