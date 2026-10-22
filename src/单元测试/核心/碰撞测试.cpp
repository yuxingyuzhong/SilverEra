/*
碰撞体测试（碰撞体反查与搬移）
（第五十九轮由「碰撞测试.cpp」按夹具拆分；逻辑未改）
*/
#include "碰撞测试辅助.h"

TEST_F(Collider_Test, 默认构造可反查)
{
	//被测碰撞体
	engine::Collider collider;
	//反查应指回自身
	EXPECT_EQ(engine::Collider::recover(&collider.object), &collider);
}

//碰撞体：搬移后自指针重新挂载，源对象自指针被清空
TEST_F(Collider_Test, 搬移后反查有效)
{
	//源碰撞体
	engine::Collider source;
	//写入编号
	source.ID = 7;

	//搬移构造
	engine::Collider target = std::move(source);

	//搬移后的对象应能被反查
	ASSERT_EQ(engine::Collider::recover(&target.object), &target);
	//编号应随对象一并搬移
	EXPECT_EQ(engine::Collider::recover(&target.object)->ID, 7u);
	//源对象自指针应被清空
	EXPECT_EQ(engine::Collider::recover(&source.object), nullptr);
}

//碰撞空间测试夹具