#include <gtest/gtest.h>
#include "ECS测试组件.h"

//销毁实体：级联清理其全部组件
TEST(ECS_Lifecycle_Test, 销毁实体级联清理组件)
{
	//世界与实体
	engine::ECS_Store store;
	engine::Entity entity = store.entity_create();
	ASSERT_NE(store.component_add<ecs_test::Position>(entity, 1.0f, 1.0f), nullptr);
	ASSERT_NE(store.component_add<ecs_test::Health>(entity, 10), nullptr);
	EXPECT_EQ(store.component_size<ecs_test::Position>(), 1u);
	EXPECT_EQ(store.component_size<ecs_test::Health>(), 1u);
	//销毁实体
	EXPECT_TRUE(store.entity_destroy(entity));
	EXPECT_EQ(store.component_size<ecs_test::Position>(), 0u);
	EXPECT_EQ(store.component_size<ecs_test::Health>(), 0u);
	EXPECT_EQ(store.entity_size(), 0u);
}

//世界清空：组件与实体全部归零，清空后仍可继续创建
TEST(ECS_Lifecycle_Test, 清空世界)
{
	//世界与实体
	engine::ECS_Store store;
	engine::Entity first = store.entity_create();
	ASSERT_NE(store.component_add<ecs_test::Velocity>(first, 1.0f, 2.0f), nullptr);
	//清空世界
	store.clear();
	EXPECT_EQ(store.entity_size(), 0u);
	EXPECT_EQ(store.component_size<ecs_test::Velocity>(), 0u);
	//清空后句柄失效
	EXPECT_FALSE(store.entity_alive(first));
	//清空后仍可创建
	engine::Entity second = store.entity_create();
	EXPECT_TRUE(store.entity_alive(second));
	EXPECT_NE(second.value, first.value);
}
