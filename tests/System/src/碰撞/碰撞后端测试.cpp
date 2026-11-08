/*
碰撞后端测试（几何形状、挂载选择、空间边界包含性、检测能力）
（段 E 新增：仅经后端公共接口覆盖，不包含任何后端内部头）
*/
#include "碰撞测试辅助.h"
//获取碰撞体后端公开接口
#include "Engine/EngineSystem/src/collision/Collider/backend/碰撞体后端.h"
//获取碰撞空间后端公开接口
#include "Engine/EngineSystem/src/collision/Collision_Region/backend/空间后端.h"

//碰撞后端测试夹具
class Collision_Backend_Test : public ::testing::Test
{
};

//形状配置构造
static nlohmann::json shape_config(const std::string& shape_type)
{
	//形状配置
	nlohmann::json config = nlohmann::json::object();
	//写入形状类型
	config["type"] = shape_type;
	//按类型补齐参数
	if (shape_type == "box")
		config["half_extent"] = nlohmann::json::array({ 1.0, 1.0, 1.0 });
	else if (shape_type == "sphere")
		config["radius"] = 1.0;
	else if (shape_type == "capsule" || shape_type == "cylinder" || shape_type == "cone")
	{
		config["radius"] = 1.0;
		config["height"] = 2.0;
	}
	return config;
}

//几何体：七种形状均可构建并挂载
TEST_F(Collision_Backend_Test, 七种形状均可挂载)
{
	//临时网格路径
	std::string mesh_path = temp_mesh_write();
	//逐个验证盒体、球体、胶囊、圆柱、圆锥
	for (const std::string& shape_type : { "box", "sphere", "capsule", "cylinder", "cone" })
	{
		//碰撞体后端
		engine::Collider_Backend backend;
		//构建几何
		ASSERT_TRUE(backend.geometry_build(shape_config(shape_type),
			engine::Transform::getIdentity()));
		//挂载几何
		ASSERT_TRUE(backend.geometry_mount());
		//挂载状态为真
		EXPECT_TRUE(backend.geometry_mounted());
		//包围盒非零
		engine::Vector3 aabb_min, aabb_max;
		backend.aabb_get(aabb_min, aabb_max);
		EXPECT_GT(aabb_max.x(), 0.5f);
	}

	//网格形状
	nlohmann::json mesh_config = nlohmann::json::object();
	mesh_config["type"] = "mesh";
	mesh_config["mesh_path"] = mesh_path;
	//碰撞体后端
	engine::Collider_Backend mesh_backend;
	//构建并挂载网格
	ASSERT_TRUE(mesh_backend.geometry_build(mesh_config, engine::Transform::getIdentity()));
	ASSERT_TRUE(mesh_backend.geometry_mount());
	EXPECT_TRUE(mesh_backend.geometry_mounted());

	//清理临时网格
	std::error_code ec;
	std::filesystem::remove(engine::detail::exe_dir_get() / engine::detail::string_to_path(mesh_path),
		ec);
}

//几何体：单部件与多部件的挂载选择
TEST_F(Collision_Backend_Test, 单部件与多部件挂载)
{
	//单部件后端
	engine::Collider_Backend single_backend;
	//构建单个盒体并挂载
	ASSERT_TRUE(single_backend.geometry_build(shape_config("box"),
		engine::Transform::getIdentity()));
	ASSERT_TRUE(single_backend.geometry_mount());
	//包围盒约在半长一以内
	engine::Vector3 aabb_min, aabb_max;
	single_backend.aabb_get(aabb_min, aabb_max);
	EXPECT_NEAR(aabb_max.x(), 1.0f, 0.1f);

	//多部件后端
	engine::Collider_Backend multi_backend;
	//两个盒体分别位于正负五个单位处
	nlohmann::json config = nlohmann::json::object();
	nlohmann::json left = box_config(0u, -5.0);
	nlohmann::json right = box_config(0u, 5.0);
	//移除单几何体专用字段，仅保留形状与相对位置
	left.erase("collider_ID");
	right.erase("collider_ID");
	config["geometries"] = nlohmann::json::array({ left, right });
	//构建并挂载
	ASSERT_TRUE(multi_backend.geometry_build(config, engine::Transform::getIdentity()));
	ASSERT_TRUE(multi_backend.geometry_mount());
	//复合包围盒跨越两侧部件
	multi_backend.aabb_get(aabb_min, aabb_max);
	EXPECT_LT(aabb_min.x(), -5.9f);
	EXPECT_GT(aabb_max.x(), 5.9f);
}

//空间边界：包含性判定区分内外
TEST_F(Collision_Backend_Test, 空间边界包含性判定)
{
	//临时网格（半长为一的封闭立方体边界）
	std::string mesh_path = temp_mesh_write();
	//碰撞空间后端
	engine::Region_Backend region_backend;
	//构建空间边界
	ASSERT_TRUE(region_backend.boundary_build(mesh_path));

	//空间内部点
	EXPECT_TRUE(region_backend.point_inside(engine::Vector3(0.0f, 0.0f, 0.0f)));
	//空间外部点
	EXPECT_FALSE(region_backend.point_inside(engine::Vector3(5.0f, 0.0f, 0.0f)));

	//卸载边界后不再判定为内部
	region_backend.boundary_unload();
	EXPECT_FALSE(region_backend.boundary_mounted());
	EXPECT_FALSE(region_backend.point_inside(engine::Vector3(0.0f, 0.0f, 0.0f)));

	//清理临时网格
	std::error_code ec;
	std::filesystem::remove(engine::detail::exe_dir_get() / engine::detail::string_to_path(mesh_path),
		ec);
}

//检测能力：离散执行输出碰撞对，边界接触单独登记
TEST_F(Collision_Backend_Test, 离散检测输出碰撞对)
{
	//碰撞空间后端
	engine::Region_Backend region_backend;
	//两个相互重叠的碰撞体
	engine::Collider_Backend collider_A, collider_B;
	//分别构建盒体并挂载
	ASSERT_TRUE(collider_A.geometry_build(box_config(0u, 0.0),
		engine::Transform::getIdentity()));
	ASSERT_TRUE(collider_A.geometry_mount());
	ASSERT_TRUE(collider_B.geometry_build(box_config(0u, 1.0),
		engine::Transform::getIdentity()));
	ASSERT_TRUE(collider_B.geometry_mount());
	//写入编号标签并加入碰撞世界
	collider_A.tag_set(1u, false);
	collider_B.tag_set(2u, false);
	region_backend.object_attach(collider_A);
	region_backend.object_attach(collider_B);

	//移动体清单（运动为零时仍执行一次求交）
	std::vector<engine::Collider_Move> moves;
	engine::Collider_Move move_A;
	move_A.ID = 1u;
	move_A.backend = &collider_A;
	move_A.body_step.translation = engine::Vector3(0.0f, 0.0f, 0.0f);
	move_A.step_count = 1u;
	moves.push_back(move_A);
	engine::Collider_Move move_B;
	move_B.ID = 2u;
	move_B.backend = &collider_B;
	move_B.body_step.translation = engine::Vector3(0.0f, 0.0f, 0.0f);
	move_B.step_count = 1u;
	moves.push_back(move_B);

	//碰撞详情与边界接触
	std::vector<engine::Collision_Detail> details;
	std::vector<uint64_t> boundary_contacts;
	region_backend.discrete_run(moves, details, boundary_contacts);
	//重叠的两盒体应产生一对碰撞
	ASSERT_EQ(details.size(), 1u);
	EXPECT_EQ(details[0].collider_A, 1u);
	EXPECT_EQ(details[0].collider_B, 2u);
	//应携带至少一个接触点，法线非零且深度非负
	ASSERT_FALSE(details[0].contacts.empty());
	EXPECT_GT(details[0].contacts[0].normal.length2(), 0.0f);
	EXPECT_GE(details[0].contacts[0].depth, 0.0f);
	EXPECT_FLOAT_EQ(details[0].contacts[0].fraction, 1.0f);
	//无空间边界时不产生边界接触
	EXPECT_TRUE(boundary_contacts.empty());
}

//检测能力：扫掠收集按段推进并输出碰撞对
TEST_F(Collision_Backend_Test, 扫掠检测输出碰撞对)
{
	//碰撞空间后端
	engine::Region_Backend region_backend;
	//移动体与静止体
	engine::Collider_Backend mover, target;
	//移动体位于原点，静止体位于正五个单位处
	ASSERT_TRUE(mover.geometry_build(box_config(0u, 0.0), engine::Transform::getIdentity()));
	ASSERT_TRUE(mover.geometry_mount());
	ASSERT_TRUE(target.geometry_build(box_config(0u, 5.0), engine::Transform::getIdentity()));
	ASSERT_TRUE(target.geometry_mount());
	//写入编号标签并加入碰撞世界
	mover.tag_set(1u, false);
	target.tag_set(2u, false);
	region_backend.object_attach(mover);
	region_backend.object_attach(target);

	//移动体沿正 X 方向扫掠四个单位（分两段）
	engine::Collider_Move move_item;
	move_item.ID = 1u;
	move_item.backend = &mover;
	move_item.body_step.translation = engine::Vector3(4.0f, 0.0f, 0.0f);
	move_item.step_count = 2u;
	//碰撞详情与边界穿越编号
	std::vector<engine::Collision_Detail> details;
	std::vector<uint64_t> boundary_contacts;
	region_backend.swept_collect(std::vector<engine::Collider_Move>{ move_item }, details,
		boundary_contacts);
	//扫掠途中与静止体相交
	ASSERT_FALSE(details.empty());
	EXPECT_EQ(details[0].collider_A, 1u);
	EXPECT_EQ(details[0].collider_B, 2u);
	//应携带命中点、法线与命中比例
	ASSERT_FALSE(details[0].contacts.empty());
	EXPECT_GT(details[0].contacts[0].normal.length2(), 0.0f);
	EXPECT_GE(details[0].contacts[0].fraction, 0.0f);
	EXPECT_LE(details[0].contacts[0].fraction, 1.0f);
}

//整体运动：绕自身原点旋转后按世界轴平移
TEST_F(Collision_Backend_Test, 整体运动推进平移与旋转)
{
	//碰撞体后端
	engine::Collider_Backend backend;
	//构建并挂载原点盒体
	ASSERT_TRUE(backend.geometry_build(shape_config("box"), engine::Transform::getIdentity()));
	ASSERT_TRUE(backend.geometry_mount());

	//整体运动（平移一个单位并绕 Z 轴旋转九十度）
	engine::Motion motion;
	motion.translation = engine::Vector3(1.0f, 0.0f, 0.0f);
	motion.rotation = engine::Quaternion::from_axis_angle(engine::Vector3(0.0f, 0.0f, 1.0f),
		3.14159265f / 2.0f);
	ASSERT_TRUE(backend.motion_advance(motion));

	//世界变换的平移已叠加，旋转不再为单位
	engine::Transform world = backend.world_transform_get();
	EXPECT_FLOAT_EQ(world.getOrigin().x(), 1.0f);
	EXPECT_FALSE(world.getRotation().identity_check());

	//盒体旋转后包围盒仍约为半长一
	engine::Vector3 aabb_min, aabb_max;
	backend.aabb_get(aabb_min, aabb_max);
	EXPECT_NEAR(aabb_max.x(), 2.0f, 0.2f);
	EXPECT_NEAR(aabb_max.y(), 1.0f, 0.2f);
}

//部件数量：单部件为一，多部件按几何体集合计数
TEST_F(Collision_Backend_Test, 部件数量读取)
{
	//单部件后端
	engine::Collider_Backend single_backend;
	ASSERT_TRUE(single_backend.geometry_build(shape_config("box"),
		engine::Transform::getIdentity()));
	ASSERT_TRUE(single_backend.geometry_mount());
	EXPECT_EQ(single_backend.part_count(), 1u);

	//多部件后端
	engine::Collider_Backend multi_backend;
	nlohmann::json config = nlohmann::json::object();
	nlohmann::json left = box_config(0u, -5.0);
	nlohmann::json right = box_config(0u, 5.0);
	left.erase("collider_ID");
	right.erase("collider_ID");
	config["geometries"] = nlohmann::json::array({ left, right });
	ASSERT_TRUE(multi_backend.geometry_build(config, engine::Transform::getIdentity()));
	ASSERT_TRUE(multi_backend.geometry_mount());
	EXPECT_EQ(multi_backend.part_count(), 2u);
}

//部件运动：单动一件只改变该部件的局部位置
TEST_F(Collision_Backend_Test, 部件运动只改目标部件)
{
	//多部件后端（两个盒体分别位于正负五个单位处）
	engine::Collider_Backend backend;
	nlohmann::json config = nlohmann::json::object();
	nlohmann::json left = box_config(0u, -5.0);
	nlohmann::json right = box_config(0u, 5.0);
	left.erase("collider_ID");
	right.erase("collider_ID");
	config["geometries"] = nlohmann::json::array({ left, right });
	ASSERT_TRUE(backend.geometry_build(config, engine::Transform::getIdentity()));
	ASSERT_TRUE(backend.geometry_mount());

	//推动第二个部件沿正 Y 方向两个单位
	engine::Motion motion;
	motion.translation = engine::Vector3(0.0f, 2.0f, 0.0f);
	ASSERT_TRUE(backend.part_motion_advance(1u, motion));
	//碰撞体世界变换不动
	EXPECT_FLOAT_EQ(backend.world_transform_get().getOrigin().x(), 0.0f);

	//包围盒随目标部件上移，另一部件保持原位
	engine::Vector3 aabb_min, aabb_max;
	backend.aabb_get(aabb_min, aabb_max);
	EXPECT_GT(aabb_max.y(), 2.9f);
	EXPECT_LT(aabb_min.y(), -0.9f);
	EXPECT_LT(aabb_min.x(), -5.9f);
	EXPECT_GT(aabb_max.x(), 5.9f);
}

//部件运动：部件下标越界被拒绝
TEST_F(Collision_Backend_Test, 部件下标越界被拒绝)
{
	//多部件后端
	engine::Collider_Backend backend;
	nlohmann::json config = nlohmann::json::object();
	nlohmann::json left = box_config(0u, -5.0);
	nlohmann::json right = box_config(0u, 5.0);
	left.erase("collider_ID");
	right.erase("collider_ID");
	config["geometries"] = nlohmann::json::array({ left, right });
	ASSERT_TRUE(backend.geometry_build(config, engine::Transform::getIdentity()));
	ASSERT_TRUE(backend.geometry_mount());

	//越界下标推进失败
	engine::Motion motion;
	motion.translation = engine::Vector3(1.0f, 0.0f, 0.0f);
	EXPECT_FALSE(backend.part_motion_advance(2u, motion));
}

//部件运动：单部件碰撞体的部件运动退化为整体运动
TEST_F(Collision_Backend_Test, 单部件部件运动退化为整体运动)
{
	//单部件后端
	engine::Collider_Backend backend;
	ASSERT_TRUE(backend.geometry_build(shape_config("box"), engine::Transform::getIdentity()));
	ASSERT_TRUE(backend.geometry_mount());

	//对唯一部件推进平移
	engine::Motion motion;
	motion.translation = engine::Vector3(2.0f, 0.0f, 0.0f);
	ASSERT_TRUE(backend.part_motion_advance(0u, motion));

	//单部件无独立局部变换，退化后改写碰撞体世界变换
	EXPECT_FLOAT_EQ(backend.world_transform_get().getOrigin().x(), 2.0f);
}

//子部件编号：显式配置后可查询，按编号推进部件运动
TEST_F(Collision_Backend_Test, 子部件编号存在性与按编号运动)
{
	//多部件后端（两个盒体分别位于正负五个单位处，显式编号三与七）
	engine::Collider_Backend backend;
	nlohmann::json config = nlohmann::json::object();
	nlohmann::json left = box_config(0u, -5.0);
	nlohmann::json right = box_config(0u, 5.0);
	left.erase("collider_ID");
	right.erase("collider_ID");
	left["part_ID"] = 3u;
	right["part_ID"] = 7u;
	config["geometries"] = nlohmann::json::array({ left, right });
	ASSERT_TRUE(backend.geometry_build(config, engine::Transform::getIdentity()));
	ASSERT_TRUE(backend.geometry_mount());

	//编号存在性判定
	EXPECT_TRUE(backend.part_exists(3u));
	EXPECT_TRUE(backend.part_exists(7u));
	EXPECT_FALSE(backend.part_exists(0u));
	EXPECT_FALSE(backend.part_exists(9u));

	//按编号推动右侧子部件沿正 Y 方向两个单位
	engine::Motion motion;
	motion.translation = engine::Vector3(0.0f, 2.0f, 0.0f);
	ASSERT_TRUE(backend.part_motion_advance(7u, motion));
	//不存在的编号被拒绝
	EXPECT_FALSE(backend.part_motion_advance(9u, motion));

	//包围盒随右侧子部件上移
	engine::Vector3 aabb_min, aabb_max;
	backend.aabb_get(aabb_min, aabb_max);
	EXPECT_GT(aabb_max.y(), 2.9f);
	EXPECT_LT(aabb_min.y(), -0.9f);
}

//子部件编号：重复编号整条几何配置被拒
TEST_F(Collision_Backend_Test, 子部件编号重复被拒)
{
	//两个盒体使用同一子部件编号
	engine::Collider_Backend backend;
	nlohmann::json config = nlohmann::json::object();
	nlohmann::json left = box_config(0u, -5.0);
	nlohmann::json right = box_config(0u, 5.0);
	left.erase("collider_ID");
	right.erase("collider_ID");
	left["part_ID"] = 3u;
	right["part_ID"] = 3u;
	config["geometries"] = nlohmann::json::array({ left, right });
	EXPECT_FALSE(backend.geometry_build(config, engine::Transform::getIdentity()));
}

//部件运动：部件旋转改变复合包围盒
TEST_F(Collision_Backend_Test, 部件旋转改变包围盒)
{
	//多部件后端（两个盒体分别位于正负五个单位处）
	engine::Collider_Backend backend;
	nlohmann::json config = nlohmann::json::object();
	nlohmann::json left = box_config(0u, -5.0);
	nlohmann::json right = box_config(0u, 5.0);
	left.erase("collider_ID");
	right.erase("collider_ID");
	config["geometries"] = nlohmann::json::array({ left, right });
	ASSERT_TRUE(backend.geometry_build(config, engine::Transform::getIdentity()));
	ASSERT_TRUE(backend.geometry_mount());

	//让第二个部件绕 Z 轴旋转四十五度（盒体对角线扩大包围盒）
	engine::Motion motion;
	motion.rotation = engine::Quaternion::from_axis_angle(engine::Vector3(0.0f, 0.0f, 1.0f),
		3.14159265f / 4.0f);
	ASSERT_TRUE(backend.part_motion_advance(1u, motion));

	//旋转后该部件包围盒在 X 向扩至约五点四
	engine::Vector3 aabb_min, aabb_max;
	backend.aabb_get(aabb_min, aabb_max);
	EXPECT_GT(aabb_max.x(), 6.2f);
	EXPECT_GT(aabb_max.y(), 1.2f);
}
