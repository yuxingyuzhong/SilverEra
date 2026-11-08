#include <gtest/gtest.h>
#include "ECS测试组件.h"

//双组件交集：只访问同时持有两类组件的实体
TEST(ECS_View_Test, 双组件交集遍历)
{
	//世界与三个实体
	engine::ECS_Store store;
	engine::Entity both = store.entity_create();
	engine::Entity only_position = store.entity_create();
	engine::Entity only_velocity = store.entity_create();
	//同时持有位置与速度
	ASSERT_NE(store.component_add<ecs_test::Position>(both, 1.0f, 2.0f), nullptr);
	ASSERT_NE(store.component_add<ecs_test::Velocity>(both, 3.0f, 4.0f), nullptr);
	//只持有位置
	ASSERT_NE(store.component_add<ecs_test::Position>(only_position, 5.0f, 6.0f), nullptr);
	//只持有速度
	ASSERT_NE(store.component_add<ecs_test::Velocity>(only_velocity, 7.0f, 8.0f), nullptr);
	//交集遍历
	int visited = 0;
	store.each<ecs_test::Position, ecs_test::Velocity>(
		[&](engine::Entity entity, ecs_test::Position& position, ecs_test::Velocity& velocity)
		{
			EXPECT_EQ(entity.value, both.value);
			EXPECT_EQ(position.owner, both.index());
			EXPECT_EQ(velocity.owner, both.index());
			visited++;
		});
	EXPECT_EQ(visited, 1);
}

//双组件交集：缺少任一组件都不进入回调
TEST(ECS_View_Test, 缺一类组件不进入交集)
{
	//世界与实体
	engine::ECS_Store store;
	engine::Entity entity = store.entity_create();
	ASSERT_NE(store.component_add<ecs_test::Position>(entity, 9.0f, 9.0f), nullptr);
	//交集遍历
	int visited = 0;
	store.each<ecs_test::Position, ecs_test::Velocity>(
		[&](engine::Entity, ecs_test::Position&, ecs_test::Velocity&)
		{
			visited++;
		});
	EXPECT_EQ(visited, 0);
}

//双组件交集：未注册的组件类型不产生遍历
TEST(ECS_View_Test, 未注册组件不遍历)
{
	//世界
	engine::ECS_Store store;
	//未添加任何组件
	int visited = 0;
	store.each<ecs_test::Position, ecs_test::Velocity>(
		[&](engine::Entity, ecs_test::Position&, ecs_test::Velocity&)
		{
			visited++;
		});
	EXPECT_EQ(visited, 0);
}
