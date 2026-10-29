/*
碰撞区域测试（空间状态 / 碰撞体编号与构建 / 几何与位移检测 / 边界与跨越 / 形状字段校验）
（第五十九轮由「碰撞测试.cpp」按夹具拆分；逻辑未改）
*/
#include "碰撞测试辅助.h"

TEST_F(Collision_Region_Test, 空间状态与后端有效性)
{
	//碰撞检测后端应可用
	EXPECT_TRUE(region.valid());
	//默认非激活
	EXPECT_FALSE(region.state_check());
	//设置激活
	region.state_set(true);
	EXPECT_TRUE(region.state_check());
}

//碰撞空间：空间边界固定占用编号零，碰撞体编号自一起依次分配
TEST_F(Collision_Region_Test, 碰撞体编号自一起分配)
{
	//第一个碰撞体编号为一
	EXPECT_EQ(region.collider_build(), 1u);
	//后续编号依次递增
	EXPECT_EQ(region.collider_build(), 2u);
}

//碰撞空间：碰撞体构建后可查询，挂载几何体后进入碰撞世界
TEST_F(Collision_Region_Test, 碰撞体构建与包含性)
{
	//构建碰撞体
	const uint64_t collider_ID = region.collider_build();

	//编号应已登记
	EXPECT_TRUE(region.collider_find(collider_ID));
	//尚未挂载几何体，不应进入碰撞世界
	EXPECT_FALSE(region.contains(collider_ID));
	//全量编号应包含该编号
	EXPECT_EQ(region.colliders().size(), 1u);

	//设置几何体
	ASSERT_TRUE(region.collider_set(box_config(collider_ID, 0.0)));
	//挂载几何体后应进入碰撞世界
	EXPECT_TRUE(region.contains(collider_ID));
	//几何配置应被留档
	EXPECT_FALSE(region.collider_geometry(collider_ID).empty());
}

//碰撞空间：各类非法几何配置一律被拒且不改变碰撞体状态
TEST_F(Collision_Region_Test, 非法几何配置被拒)
{
	//构建碰撞体
	const uint64_t collider_ID = region.collider_build();

	//空对象配置
	EXPECT_FALSE(region.collider_set(nlohmann::json::object()));
	//非对象配置
	EXPECT_FALSE(region.collider_set(nlohmann::json::array({ 1, 2, 3 })));
	//缺少形状类型
	nlohmann::json no_type = nlohmann::json::object();
	no_type["collider_ID"] = collider_ID;
	EXPECT_FALSE(region.collider_set(no_type));
	//未知形状类型
	nlohmann::json unknown_type = no_type;
	unknown_type["type"] = "多面体";
	EXPECT_FALSE(region.collider_set(unknown_type));
	//目标编号不存在
	EXPECT_FALSE(region.collider_set(box_config(collider_ID + 1, 0.0)));
	//半径非正
	nlohmann::json bad_radius = nlohmann::json::object();
	bad_radius["collider_ID"] = collider_ID;
	bad_radius["type"] = "sphere";
	bad_radius["radius"] = 0.0;
	EXPECT_FALSE(region.collider_set(bad_radius));
	//数组长度不为三
	nlohmann::json bad_extent = box_config(collider_ID, 0.0);
	bad_extent["half_extent"] = nlohmann::json::array({ 1.0, 1.0 });
	EXPECT_FALSE(region.collider_set(bad_extent));

	//非法配置不应使碰撞体进入碰撞世界
	EXPECT_FALSE(region.contains(collider_ID));
}

//碰撞空间：几何体集合按相对变换装配，各部件相对位置参与碰撞检测
TEST_F(Collision_Region_Test, 几何体集合相对变换参与碰撞检测)
{
	//激活空间
	region.state_set(true);

	//构建碰撞体A（几何体集合：两个盒体分别位于本体两侧五个单位处）
	const uint64_t collider_A = region.collider_build();
	nlohmann::json set_A = nlohmann::json::object();
	set_A["collider_ID"] = collider_A;
	set_A["geometries"] = nlohmann::json::array();
	//左侧部件
	nlohmann::json part_left = nlohmann::json::object();
	part_left["type"] = "box";
	part_left["half_extent"] = nlohmann::json::array({ 1.0, 1.0, 1.0 });
	part_left["position"] = nlohmann::json::array({ -5.0, 0.0, 0.0 });
	set_A["geometries"].push_back(part_left);
	//右侧部件
	nlohmann::json part_right = nlohmann::json::object();
	part_right["type"] = "box";
	part_right["half_extent"] = nlohmann::json::array({ 1.0, 1.0, 1.0 });
	part_right["position"] = nlohmann::json::array({ 5.0, 0.0, 0.0 });
	set_A["geometries"].push_back(part_right);

	//挂载集合式几何体
	ASSERT_TRUE(region.collider_set(set_A));
	//挂载后应进入碰撞世界
	EXPECT_TRUE(region.contains(collider_A));
	//几何配置应被留档
	EXPECT_FALSE(region.collider_geometry(collider_A).empty());

	//构建碰撞体B（单体盒体，仅与A的右侧部件重叠）
	const uint64_t collider_B = region.collider_build();
	ASSERT_TRUE(region.collider_set(box_config(collider_B, 5.5)));

	//检测应报告A与B的一对碰撞（证明部件相对变换生效）
	std::optional<std::vector<engine::Collision_Result>> results = region.detect();
	ASSERT_TRUE(results.has_value());
	ASSERT_EQ(results->size(), 1u);
	EXPECT_EQ((*results)[0].collider_A, collider_A);
	EXPECT_EQ((*results)[0].collider_B, collider_B);

	//几何体集合不可为空
	nlohmann::json empty_set = nlohmann::json::object();
	empty_set["collider_ID"] = collider_A;
	empty_set["geometries"] = nlohmann::json::array();
	EXPECT_FALSE(region.collider_set(empty_set));

	//几何体集合元素须为对象
	nlohmann::json bad_set = nlohmann::json::object();
	bad_set["collider_ID"] = collider_A;
	bad_set["geometries"] = nlohmann::json::array({ 1, 2 });
	EXPECT_FALSE(region.collider_set(bad_set));
}

//碰撞空间：位移事件驱动离散检测，重叠时报告碰撞对
TEST_F(Collision_Region_Test, 位移重叠触发碰撞检测)
{
	//构建两个盒体
	const uint64_t collider_A = region.collider_build();
	const uint64_t collider_B = region.collider_build();
	ASSERT_TRUE(region.collider_set(box_config(collider_A, 0.0)));
	ASSERT_TRUE(region.collider_set(box_config(collider_B, 5.0)));
	//激活碰撞空间
	region.state_set(true);

	//当前位移向量（模拟位移事件通道交由碰撞空间读取的内容）
	engine::Vector3 displacement(0.0f, 0.0f, 0.0f);
	//注入位移读取回调（仅第二个盒体带位移，其余盒体无位移）
	region.displacement_reader_set([&displacement, collider_B](uint64_t collider_ID,
		engine::Vector3& receiver)
		{
			//非目标碰撞体无位移
			if (collider_ID != collider_B)
				return false;
			//写入当前位移向量
			receiver = displacement;
			return true;
		});

	//相距五个单位时无碰撞
	std::optional<std::vector<engine::Collision_Result>> detected = region.detect();
	ASSERT_TRUE(detected.has_value());
	EXPECT_TRUE(detected->empty());

	//位移事件把第二个盒体移入重叠位置
	displacement.setValue(-4.0f, 0.0f, 0.0f);
	detected = region.detect();
	ASSERT_TRUE(detected.has_value());
	ASSERT_EQ(detected->size(), 1u);
	//碰撞对编号应归一化
	EXPECT_EQ((*detected)[0].collider_A, collider_A);
	EXPECT_EQ((*detected)[0].collider_B, collider_B);

	//位移事件把第二个盒体移开
	displacement.setValue(-8.0f, 0.0f, 0.0f);
	detected = region.detect();
	ASSERT_TRUE(detected.has_value());
	EXPECT_TRUE(detected->empty());
}

//碰撞空间：同组豁免的碰撞对不参与检测
TEST_F(Collision_Region_Test, 同组豁免碰撞对被跳过)
{
	//构建两个相互重叠的盒体
	const uint64_t collider_A = region.collider_build();
	const uint64_t collider_B = region.collider_build();
	ASSERT_TRUE(region.collider_set(box_config(collider_A, 0.0)));
	ASSERT_TRUE(region.collider_set(box_config(collider_B, 1.0)));
	//激活碰撞空间
	region.state_set(true);

	//豁免标记相同且非零时不参与检测
	ASSERT_TRUE(region.collider_set(collider_A, static_cast<uint64_t>(1)));
	ASSERT_TRUE(region.collider_set(collider_B, static_cast<uint64_t>(1)));
	std::optional<std::vector<engine::Collision_Result>> detected = region.detect();
	ASSERT_TRUE(detected.has_value());
	EXPECT_TRUE(detected->empty());

	//豁免标记不同则照常检测
	ASSERT_TRUE(region.collider_set(collider_B, static_cast<uint64_t>(2)));
	detected = region.detect();
	ASSERT_TRUE(detected.has_value());
	EXPECT_EQ(detected->size(), 1u);
}

//碰撞空间：未激活时不执行检测
TEST_F(Collision_Region_Test, 未激活时不执行检测)
{
	//未激活的碰撞空间返回空结果
	EXPECT_FALSE(region.detect().has_value());
	//激活后返回检测结果
	region.state_set(true);
	EXPECT_TRUE(region.detect().has_value());
}

//碰撞空间：碰撞体卸载后编号注销
TEST_F(Collision_Region_Test, 碰撞体卸载后注销)
{
	//构建并设置几何体
	const uint64_t collider_ID = region.collider_build();
	ASSERT_TRUE(region.collider_set(box_config(collider_ID, 0.0)));

	//卸载碰撞体
	EXPECT_TRUE(region.collider_unload(collider_ID));
	//编号应被注销且退出碰撞世界
	EXPECT_FALSE(region.collider_find(collider_ID));
	EXPECT_FALSE(region.contains(collider_ID));
	EXPECT_TRUE(region.colliders().empty());
	//重复卸载失败
	EXPECT_FALSE(region.collider_unload(collider_ID));
}

//碰撞空间：空间边界可由 OBJ 网格构建，非法路径被拒
TEST_F(Collision_Region_Test, 空间边界构建与卸载)
{
	//写入临时立方体网格
	const std::string mesh_name = temp_mesh_write();

	//构建空间边界
	EXPECT_TRUE(region.boundary_build(mesh_name));
	//重复构建覆盖原有边界
	EXPECT_TRUE(region.boundary_build(mesh_name));
	//非法路径构建失败
	EXPECT_FALSE(region.boundary_build("src/单元测试/EngineCore/核心/不存在的网格.obj"));

	//卸载空间边界
	region.boundary_unload();

	//清理临时网格文件
	std::error_code ec;
	std::filesystem::remove(
		engine::Engine_Env::exe_dir_get() / engine::detail::string_to_path(mesh_name), ec);
}

//碰撞空间：跨越状态转移时收集通知，稳态不重复（接触判部分跨越，射线奇偶判内外）
TEST_F(Collision_Region_Test, 跨越状态转移与通知收集)
{
	//写入临时立方体网格（半长为一的封闭立方体边界）
	const std::string mesh_name = temp_mesh_write();
	//构建空间边界
	ASSERT_TRUE(region.boundary_build(mesh_name));
	//激活碰撞空间
	region.state_set(true);

	//构建盒体碰撞体（半长零点三，完全位于边界内部）
	const uint64_t collider_ID = region.collider_build();
	nlohmann::json inside_config = box_config(collider_ID, 0.0);
	inside_config["half_extent"] = nlohmann::json::array({ 0.3, 0.3, 0.3 });
	ASSERT_TRUE(region.collider_set(inside_config));

	//首次检测仅建立基准（完全在内），不产生通知
	ASSERT_TRUE(region.detect().has_value());
	EXPECT_TRUE(region.cross_notices_take().empty());

	//移出边界（远离至五个单位处）：完全在内 → 完全在外
	nlohmann::json outside_config = inside_config;
	outside_config["position"] = nlohmann::json::array({ 5.0, 0.0, 0.0 });
	ASSERT_TRUE(region.collider_set(outside_config));
	ASSERT_TRUE(region.detect().has_value());
	std::vector<engine::Cross_Notice> notices = region.cross_notices_take();
	ASSERT_EQ(notices.size(), 1u);
	EXPECT_EQ(notices[0].collider_ID, collider_ID);
	EXPECT_EQ(notices[0].kind, "exit");

	//稳态重复检测不再产生通知
	ASSERT_TRUE(region.detect().has_value());
	EXPECT_TRUE(region.cross_notices_take().empty());

	//移回边界内部：完全在外 → 完全在内
	ASSERT_TRUE(region.collider_set(inside_config));
	ASSERT_TRUE(region.detect().has_value());
	notices = region.cross_notices_take();
	ASSERT_EQ(notices.size(), 1u);
	EXPECT_EQ(notices[0].kind, "return");

	//跨越边界（盒体跨越右侧面而与边界接触）：完全在内 → 部分在内
	nlohmann::json crossing_config = inside_config;
	crossing_config["position"] = nlohmann::json::array({ 0.9, 0.0, 0.0 });
	ASSERT_TRUE(region.collider_set(crossing_config));
	ASSERT_TRUE(region.detect().has_value());
	notices = region.cross_notices_take();
	ASSERT_EQ(notices.size(), 1u);
	EXPECT_EQ(notices[0].kind, "cross");

	//跨越状态下检测结果应不含空间边界自身（边界不属于碰撞体）
	std::optional<std::vector<engine::Collision_Result>> detected = region.detect();
	ASSERT_TRUE(detected.has_value());
	EXPECT_TRUE(detected->empty());

	//由部分跨越直接移出：部分在内 → 完全在外
	ASSERT_TRUE(region.collider_set(outside_config));
	ASSERT_TRUE(region.detect().has_value());
	notices = region.cross_notices_take();
	ASSERT_EQ(notices.size(), 1u);
	EXPECT_EQ(notices[0].kind, "exit");

	//清理临时网格文件
	std::error_code ec;
	std::filesystem::remove(
		engine::Engine_Env::exe_dir_get() / engine::detail::string_to_path(mesh_name), ec);
}

//碰撞代理器测试夹具

TEST_F(Collision_Region_Test, 位移作废与改写)
{
	//构建两个盒体（相距五个单位）
	const uint64_t collider_A = region.collider_build();
	ASSERT_TRUE(region.collider_set(box_config(collider_A, 0.0)));
	const uint64_t collider_B = region.collider_build();
	ASSERT_TRUE(region.collider_set(box_config(collider_B, 5.0)));

	//改写第二个盒体的位移（向左推进四个单位）
	EXPECT_TRUE(region.collider_displacement_replace(collider_B, engine::Vector3(-4.0, 0.0, 0.0)));
	//不存在的编号改写与作废均应失败
	EXPECT_FALSE(region.collider_displacement_replace(collider_B + 100, engine::Vector3(0.0, 0.0, 0.0)));
	EXPECT_FALSE(region.collider_displacement_void(collider_B + 100));

	//激活碰撞空间
	region.state_set(true);

	//第一次检测：位移生效使两盒体重叠，报告一对碰撞
	std::optional<std::vector<engine::Collision_Result>> first = region.detect();
	ASSERT_TRUE(first.has_value());
	ASSERT_EQ(first->size(), 1u);

	//作废第二个盒体的位移后其不再运动
	EXPECT_TRUE(region.collider_displacement_void(collider_B));
	std::optional<std::vector<engine::Collision_Result>> second = region.detect();
	ASSERT_TRUE(second.has_value());
	//盒体停留在重叠位置，仍报告一对碰撞（证明位移被作废未再推进）
	EXPECT_EQ(second->size(), 1u);

	//改写位移解除作废：按新向量向右退回原点
	EXPECT_TRUE(region.collider_displacement_replace(collider_B, engine::Vector3(4.0, 0.0, 0.0)));
	std::optional<std::vector<engine::Collision_Result>> third = region.detect();
	ASSERT_TRUE(third.has_value());
	//盒体退回至与第一个盒体分离的位置
	EXPECT_TRUE(third->empty());
}

//碰撞代理器：碰撞响应为停止运动时位移作废，碰撞体不再推进

TEST_F(Collision_Region_Test, 形状字段类型非法被拒)
{
	//构建碰撞体
	const uint64_t collider_ID = region.collider_build();

	//形状类型为数字（非字符串）被拒
	nlohmann::json bad_type = nlohmann::json::object();
	bad_type["collider_ID"] = collider_ID;
	bad_type["type"] = 123;
	EXPECT_FALSE(region.collider_set(bad_type));

	//盒体缺少半长字段被拒
	nlohmann::json box_no_extent = bad_type;
	box_no_extent["type"] = "box";
	EXPECT_FALSE(region.collider_set(box_no_extent));

	//盒体半长字段为字符串（非数组）被拒
	nlohmann::json box_bad_extent = box_no_extent;
	box_bad_extent["half_extent"] = "非法半长";
	EXPECT_FALSE(region.collider_set(box_bad_extent));

	//几何体集合字段为对象（非数组）被拒
	nlohmann::json bad_set = nlohmann::json::object();
	bad_set["collider_ID"] = collider_ID;
	bad_set["geometries"] = nlohmann::json::object();
	EXPECT_FALSE(region.collider_set(bad_set));

	//盒体碰撞边距为负被拒
	nlohmann::json box_bad_margin = box_config(collider_ID, 0.0);
	box_bad_margin["margin"] = -1.0;
	EXPECT_FALSE(region.collider_set(box_bad_margin));

	//全部非法配置均不得使碰撞体进入碰撞世界
	EXPECT_FALSE(region.contains(collider_ID));
}