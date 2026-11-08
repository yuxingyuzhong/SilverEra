#include <gtest/gtest.h>
#include "ECS测试组件.h"

//命令缓冲：延迟删除与添加按记录顺序提交
TEST(ECS_Command_Test, 延迟增删与销毁)
{
	//世界与实体
	engine::ECS_Store store;
	engine::Entity entity = store.entity_create();
	ASSERT_NE(store.component_add<ecs_test::Position>(entity, 1.0f, 2.0f), nullptr);
	//记录命令
	engine::Command_Buffer buffer;
	buffer.component_remove<ecs_test::Position>(entity);
	buffer.component_add<ecs_test::Health>(entity, ecs_test::Health(30));
	buffer.entity_destroy(entity);
	EXPECT_EQ(buffer.size(), 3u);
	//提交命令
	buffer.flush(store);
	EXPECT_EQ(buffer.size(), 0u);
	EXPECT_FALSE(store.entity_alive(entity));
	EXPECT_EQ(store.component_size<ecs_test::Position>(), 0u);
	EXPECT_EQ(store.component_size<ecs_test::Health>(), 0u);
}

//命令缓冲：遍历中延迟销毁不破坏遍历
TEST(ECS_Command_Test, 遍历中延迟销毁)
{
	//世界与两个实体
	engine::ECS_Store store;
	engine::Entity first = store.entity_create();
	engine::Entity second = store.entity_create();
	ASSERT_NE(store.component_add<ecs_test::Position>(first, 1.0f, 1.0f), nullptr);
	ASSERT_NE(store.component_add<ecs_test::Position>(second, 2.0f, 2.0f), nullptr);
	//遍历中仅记录销毁命令
	engine::Command_Buffer buffer;
	int visited = 0;
	store.component_each<ecs_test::Position>([&](engine::Entity entity, ecs_test::Position&)
	{
		visited++;
		buffer.entity_destroy(entity);
	});
	EXPECT_EQ(visited, 2);
	//提交后实体与组件全部清理
	buffer.flush(store);
	EXPECT_EQ(store.entity_size(), 0u);
	EXPECT_EQ(store.component_size<ecs_test::Position>(), 0u);
}

//命令缓冲：清空后不提交任何命令
TEST(ECS_Command_Test, 清空命令队列)
{
	//世界与实体
	engine::ECS_Store store;
	engine::Entity entity = store.entity_create();
	//记录命令后清空
	engine::Command_Buffer buffer;
	buffer.entity_destroy(entity);
	buffer.clear();
	EXPECT_EQ(buffer.size(), 0u);
	buffer.flush(store);
	//实体仍存活
	EXPECT_TRUE(store.entity_alive(entity));
}
