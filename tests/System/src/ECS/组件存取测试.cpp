#include <gtest/gtest.h>
#include "ECS测试组件.h"

//组件存取：添加、查询、存在性与数量
TEST(ECS_Component_Test, 添加查询与存在性)
{
	//世界与实体
	engine::ECS_Store store;
	engine::Entity entity = store.entity_create();
	//添加位置组件
	ecs_test::Position* position = store.component_add<ecs_test::Position>(entity, 1.0f, 2.0f);
	ASSERT_NE(position, nullptr);
	EXPECT_FLOAT_EQ(position->x, 1.0f);
	EXPECT_FLOAT_EQ(position->y, 2.0f);
	EXPECT_EQ(position->owner, entity.index());
	//存在性与查询应一致
	EXPECT_TRUE(store.component_has<ecs_test::Position>(entity));
	ASSERT_NE(store.component_get<ecs_test::Position>(entity), nullptr);
	EXPECT_EQ(store.component_size<ecs_test::Position>(), 1u);
}

//同一实体同一组件类型唯一：重复添加返回空指针且不覆盖原值
TEST(ECS_Component_Test, 重复添加不覆盖)
{
	//世界与实体
	engine::ECS_Store store;
	engine::Entity entity = store.entity_create();
	ASSERT_NE(store.component_add<ecs_test::Position>(entity, 3.0f, 4.0f), nullptr);
	//重复添加应失败
	EXPECT_EQ(store.component_add<ecs_test::Position>(entity, 9.0f, 9.0f), nullptr);
	//原值保持
	EXPECT_FLOAT_EQ(store.component_get<ecs_test::Position>(entity)->x, 3.0f);
	EXPECT_EQ(store.component_size<ecs_test::Position>(), 1u);
}

//组件删除：首次返回真，重复删除返回假
TEST(ECS_Component_Test, 删除组件)
{
	//世界与实体
	engine::ECS_Store store;
	engine::Entity entity = store.entity_create();
	ASSERT_NE(store.component_add<ecs_test::Health>(entity, 50), nullptr);
	EXPECT_TRUE(store.component_remove<ecs_test::Health>(entity));
	EXPECT_FALSE(store.component_has<ecs_test::Health>(entity));
	EXPECT_FALSE(store.component_remove<ecs_test::Health>(entity));
	EXPECT_EQ(store.component_size<ecs_test::Health>(), 0u);
}

//失效句柄不可读写组件
TEST(ECS_Component_Test, 失效句柄不可取组件)
{
	//世界与实体
	engine::ECS_Store store;
	engine::Entity entity = store.entity_create();
	ASSERT_NE(store.component_add<ecs_test::Position>(entity, 1.0f, 2.0f), nullptr);
	ASSERT_TRUE(store.entity_destroy(entity));
	EXPECT_EQ(store.component_get<ecs_test::Position>(entity), nullptr);
	EXPECT_FALSE(store.component_has<ecs_test::Position>(entity));
	EXPECT_FALSE(store.component_remove<ecs_test::Position>(entity));
}

//单组件遍历：只访问持有该组件的实体
TEST(ECS_Component_Test, 单组件遍历只访问持有者)
{
	//世界与两个实体
	engine::ECS_Store store;
	engine::Entity holder = store.entity_create();
	engine::Entity other = store.entity_create();
	ASSERT_NE(store.component_add<ecs_test::Position>(holder, 5.0f, 6.0f), nullptr);
	//遍历计数
	int visited = 0;
	store.component_each<ecs_test::Position>([&](engine::Entity entity, ecs_test::Position& position)
	{
		EXPECT_EQ(entity.value, holder.value);
		EXPECT_EQ(position.owner, holder.index());
		visited++;
	});
	EXPECT_EQ(visited, 1);
	//另一个实体不参与遍历
	EXPECT_FALSE(store.component_has<ecs_test::Position>(other));
}
