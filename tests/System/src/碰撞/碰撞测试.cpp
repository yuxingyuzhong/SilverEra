/*
碰撞体后端挂载测试（挂载、摘除、标签搬移与析构顺序）
（段 C 由「碰撞测试.cpp」改写：原自指针反查改为后端标签与挂载关系断言）
*/
#include "碰撞测试辅助.h"
//获取碰撞空间后端公开接口
#include "Engine/EngineSystem/src/collision/Collision_Region/backend/空间后端.h"
//获取碰撞体后端公开接口
#include "Engine/EngineSystem/src/collision/Collider/backend/碰撞体后端.h"

TEST_F(Collider_Test, 默认未挂载)
{
	//碰撞空间后端
	engine::Region_Backend region_backend;
	//被测碰撞体
	engine::Collider collider;
	//未挂载几何体时后端不认为其进入世界
	EXPECT_FALSE(region_backend.object_contains(*collider.backend));
}

//碰撞体：挂载几何后进入碰撞世界，摘除后退出
TEST_F(Collider_Test, 挂载与摘除)
{
	//碰撞空间后端
	engine::Region_Backend region_backend;
	//被测碰撞体
	engine::Collider collider;
	//构建几何并挂载
	ASSERT_TRUE(collider.backend->geometry_build(box_config(0u, 0.0),
		engine::Transform::getIdentity()));
	ASSERT_TRUE(collider.backend->geometry_mount());

	//加入碰撞世界
	region_backend.object_attach(*collider.backend);
	EXPECT_TRUE(region_backend.object_contains(*collider.backend));

	//移出碰撞世界
	region_backend.object_detach(*collider.backend);
	EXPECT_FALSE(region_backend.object_contains(*collider.backend));
}

//碰撞体：搬移后后端标签保持，挂载关系不受影响
TEST_F(Collider_Test, 搬移后标签保持)
{
	//碰撞空间后端
	engine::Region_Backend region_backend;
	//源碰撞体
	engine::Collider source;
	//写入编号与标签
	source.ID = 7u;
	source.backend->tag_set(7u, false);
	//构建几何并挂载
	ASSERT_TRUE(source.backend->geometry_build(box_config(0u, 0.0),
		engine::Transform::getIdentity()));
	ASSERT_TRUE(source.backend->geometry_mount());
	//加入碰撞世界
	region_backend.object_attach(*source.backend);

	//搬移构造
	engine::Collider target = std::move(source);
	//编号与标签随对象一并搬移
	EXPECT_EQ(target.ID, 7u);
	EXPECT_EQ(target.backend->tag_ID(), 7u);
	//搬移后仍处于挂载状态
	EXPECT_TRUE(region_backend.object_contains(*target.backend));

	//摘除并确认退出世界
	region_backend.object_detach(*target.backend);
	EXPECT_FALSE(region_backend.object_contains(*target.backend));
}

//碰撞体：先于碰撞空间析构时自动摘除，不留下悬空对象
TEST_F(Collider_Test, 碰撞体先析构不崩溃)
{
	//碰撞空间后端
	engine::Region_Backend region_backend;
	//碰撞体位于内层作用域，先于空间后端析构
	{
		//被测碰撞体
		engine::Collider collider;
		//构建几何并挂载
		ASSERT_TRUE(collider.backend->geometry_build(box_config(0u, 0.0),
			engine::Transform::getIdentity()));
		ASSERT_TRUE(collider.backend->geometry_mount());
		//加入碰撞世界
		region_backend.object_attach(*collider.backend);
	}
	//碰撞体析构后碰撞空间仍可正常工作
	EXPECT_TRUE(region_backend.valid());
	//刷新包围盒不应崩溃
	region_backend.aabbs_refresh();
}

//碰撞体：碰撞空间先于碰撞体析构时由空间统一摘除
TEST_F(Collider_Test, 碰撞空间先析构不崩溃)
{
	//碰撞空间后端（置于堆上以便先于碰撞体释放）
	std::unique_ptr<engine::Region_Backend> region_backend(new engine::Region_Backend());
	//被测碰撞体
	engine::Collider collider;
	//构建几何并挂载
	ASSERT_TRUE(collider.backend->geometry_build(box_config(0u, 0.0),
		engine::Transform::getIdentity()));
	ASSERT_TRUE(collider.backend->geometry_mount());
	//加入碰撞世界
	region_backend->object_attach(*collider.backend);
	//先释放碰撞空间
	region_backend.reset();
	//碰撞体随后析构不应崩溃（空间已清空其所属指针）
	SUCCEED();
}
