/*
碰撞区域测试（空间状态 / 碰撞体编号与构建 / 几何与运动检测 / 边界与跨越 / 形状字段校验）
（第五十九轮由「碰撞测试.cpp」按夹具拆分；逻辑未改）
*/
#include "碰撞测试辅助.h"
//获取逻辑帧操作工具（频率门控用例需要）
#include "Engine/EngineSystem/common/runtime/逻辑帧.h"

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

//碰撞空间：运动事件驱动离散检测，重叠时报告碰撞对
TEST_F(Collision_Region_Test, 运动重叠触发碰撞检测)
{
	//构建两个盒体
	const uint64_t collider_A = region.collider_build();
	const uint64_t collider_B = region.collider_build();
	ASSERT_TRUE(region.collider_set(box_config(collider_A, 0.0)));
	ASSERT_TRUE(region.collider_set(box_config(collider_B, 5.0)));
	//激活碰撞空间
	region.state_set(true);

	//当前运动量（模拟运动事件通道交由碰撞空间读取的内容）
	engine::Motion_Event motion_event;
	//注入运动读取回调（仅第二个盒体带运动，其余盒体无运动）
	region.motion_reader_set([&motion_event, collider_B](uint64_t collider_ID,
		engine::Motion_Event& receiver)
		{
			//非目标碰撞体无运动
			if (collider_ID != collider_B)
				return false;
			//写入当前运动量
			receiver = motion_event;
			return true;
		});

	//相距五个单位时无碰撞
	std::optional<std::vector<engine::Collision_Result>> detected = region.detect();
	ASSERT_TRUE(detected.has_value());
	EXPECT_TRUE(detected->empty());

	//运动事件把第二个盒体移入重叠位置
	motion_event.body.translation.setValue(-4.0f, 0.0f, 0.0f);
	detected = region.detect();
	ASSERT_TRUE(detected.has_value());
	ASSERT_EQ(detected->size(), 1u);
	//碰撞对编号应归一化
	EXPECT_EQ((*detected)[0].collider_A, collider_A);
	EXPECT_EQ((*detected)[0].collider_B, collider_B);

	//运动事件把第二个盒体移开
	motion_event.body.translation.setValue(-8.0f, 0.0f, 0.0f);
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
	EXPECT_FALSE(region.boundary_build("tests/System/src/碰撞/不存在的网格.obj"));

	//卸载空间边界
	region.boundary_unload();

	//清理临时网格文件
	std::error_code ec;
	std::filesystem::remove(
		engine::detail::exe_dir_get() / engine::detail::string_to_path(mesh_name), ec);
}

//碰撞空间：跨越状态转移时收集通知，稳态不重复（接触时多探针判部分跨越，射线奇偶判内外）
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
		engine::detail::exe_dir_get() / engine::detail::string_to_path(mesh_name), ec);
}

//碰撞空间：内部近壁（间隙小于碰撞边距）不得被判为部分跨越
TEST_F(Collision_Region_Test, 内部近壁不产生跨越通知)
{
	//写入临时立方体网格
	const std::string mesh_name = temp_mesh_write();
	//构建空间边界并激活
	ASSERT_TRUE(region.boundary_build(mesh_name));
	region.state_set(true);

	//构建盒体碰撞体并置于空间中部
	const uint64_t collider_ID = region.collider_build();
	nlohmann::json inside_config = box_config(collider_ID, 0.0);
	inside_config["half_extent"] = nlohmann::json::array({ 0.3, 0.3, 0.3 });
	ASSERT_TRUE(region.collider_set(inside_config));
	//首次检测只建立基准
	ASSERT_TRUE(region.detect().has_value());
	EXPECT_TRUE(region.cross_notices_take().empty());

	//贴近右侧内壁（最大面位于 x=0.995，仍完全在边界内）
	nlohmann::json near_wall = inside_config;
	near_wall["position"] = nlohmann::json::array({ 0.695, 0.0, 0.0 });
	ASSERT_TRUE(region.collider_set(near_wall));
	ASSERT_TRUE(region.detect().has_value());
	//不得产生跨越通知
	EXPECT_TRUE(region.cross_notices_take().empty());

	//快照应仍为空间内
	std::vector<engine::Cross_State_Entry> entries;
	region.cross_state_snapshot(entries);
	ASSERT_EQ(entries.size(), 1u);
	EXPECT_TRUE(entries[0].known);
	EXPECT_EQ(entries[0].state, engine::Cross_State::inside);

	//清理临时网格文件
	std::error_code ec;
	std::filesystem::remove(
		engine::detail::exe_dir_get() / engine::detail::string_to_path(mesh_name), ec);
}

//碰撞空间：扫掠一步穿出边界时补发跨越与离开两条通知
TEST_F(Collision_Region_Test, 扫掠穿出补发跨越与离开通知)
{
	//写入临时立方体网格
	const std::string mesh_name = temp_mesh_write();
	//构建空间边界并激活
	ASSERT_TRUE(region.boundary_build(mesh_name));
	region.state_set(true);

	//构建盒体碰撞体
	const uint64_t collider_ID = region.collider_build();
	nlohmann::json inside_config = box_config(collider_ID, 0.0);
	inside_config["half_extent"] = nlohmann::json::array({ 0.2, 0.2, 0.2 });
	ASSERT_TRUE(region.collider_set(inside_config));
	//启用扫掠检测并设为单步
	engine::Detection_Mode detection_mode;
	detection_mode.is_swept_volume = true;
	detection_mode.step_length = 1;
	ASSERT_TRUE(region.collider_set(collider_ID, detection_mode));
	//首次检测建立基准
	ASSERT_TRUE(region.detect().has_value());
	EXPECT_TRUE(region.cross_notices_take().empty());

	//注入一次性运动事件（向右五单位，一步扫掠穿出边界）
	engine::Motion_Event motion_event;
	motion_event.body.translation = engine::Vector3(5.0f, 0.0f, 0.0f);
	region.motion_reader_set([motion_event](uint64_t, engine::Motion_Event& receiver)
		{
			receiver = motion_event;
			return true;
		});

	//执行检测
	ASSERT_TRUE(region.detect().has_value());
	std::vector<engine::Cross_Notice> notices = region.cross_notices_take();
	ASSERT_EQ(notices.size(), 2u);
	EXPECT_EQ(notices[0].kind, "cross");
	EXPECT_EQ(notices[1].kind, "exit");
	EXPECT_EQ(notices[0].previous, engine::Cross_State::inside);
	EXPECT_LT(notices[0].sequence, notices[1].sequence);

	//清理临时网格文件
	std::error_code ec;
	std::filesystem::remove(
		engine::detail::exe_dir_get() / engine::detail::string_to_path(mesh_name), ec);
}

//碰撞空间：边界重建后重新建立基准，不得产生跨越通知
TEST_F(Collision_Region_Test, 边界重建不产生跨越通知)
{
	//写入临时立方体网格
	const std::string mesh_name = temp_mesh_write();
	//构建空间边界并激活
	ASSERT_TRUE(region.boundary_build(mesh_name));
	region.state_set(true);

	//构建盒体碰撞体
	const uint64_t collider_ID = region.collider_build();
	nlohmann::json inside_config = box_config(collider_ID, 0.0);
	inside_config["half_extent"] = nlohmann::json::array({ 0.3, 0.3, 0.3 });
	ASSERT_TRUE(region.collider_set(inside_config));
	//首次检测建立基准
	ASSERT_TRUE(region.detect().has_value());
	EXPECT_TRUE(region.cross_notices_take().empty());

	//以同一路径重建边界
	ASSERT_TRUE(region.boundary_build(mesh_name));
	ASSERT_TRUE(region.detect().has_value());
	//重建后的首次检测只建立基准
	EXPECT_TRUE(region.cross_notices_take().empty());

	//清理临时网格文件
	std::error_code ec;
	std::filesystem::remove(
		engine::detail::exe_dir_get() / engine::detail::string_to_path(mesh_name), ec);
}

//碰撞空间：空间停用清空待发通知，重新激活后重新建立基准
TEST_F(Collision_Region_Test, 停用清空待发跨越通知)
{
	//写入临时立方体网格
	const std::string mesh_name = temp_mesh_write();
	//构建空间边界并激活
	ASSERT_TRUE(region.boundary_build(mesh_name));
	region.state_set(true);

	//构建盒体碰撞体
	const uint64_t collider_ID = region.collider_build();
	nlohmann::json inside_config = box_config(collider_ID, 0.0);
	inside_config["half_extent"] = nlohmann::json::array({ 0.3, 0.3, 0.3 });
	ASSERT_TRUE(region.collider_set(inside_config));
	//首次检测建立基准
	ASSERT_TRUE(region.detect().has_value());
	EXPECT_TRUE(region.cross_notices_take().empty());

	//移出边界并检测（产生一条待发通知）
	nlohmann::json outside_config = inside_config;
	outside_config["position"] = nlohmann::json::array({ 5.0, 0.0, 0.0 });
	ASSERT_TRUE(region.collider_set(outside_config));
	ASSERT_TRUE(region.detect().has_value());

	//停用后清空待发通知
	region.state_set(false);
	region.state_set(true);
	EXPECT_TRUE(region.cross_notices_take().empty());

	//重新激活后的首次检测只建立基准
	ASSERT_TRUE(region.detect().has_value());
	EXPECT_TRUE(region.cross_notices_take().empty());

	//清理临时网格文件
	std::error_code ec;
	std::filesystem::remove(
		engine::detail::exe_dir_get() / engine::detail::string_to_path(mesh_name), ec);
}

//碰撞空间：跨越状态快照在检测前为未知，检测后反映内外
TEST_F(Collision_Region_Test, 跨越状态快照反映内外)
{
	//写入临时立方体网格
	const std::string mesh_name = temp_mesh_write();
	//构建空间边界并激活
	ASSERT_TRUE(region.boundary_build(mesh_name));
	region.state_set(true);

	//构建盒体碰撞体
	const uint64_t collider_ID = region.collider_build();
	nlohmann::json inside_config = box_config(collider_ID, 0.0);
	inside_config["half_extent"] = nlohmann::json::array({ 0.3, 0.3, 0.3 });
	ASSERT_TRUE(region.collider_set(inside_config));

	//检测前尚无基准
	std::vector<engine::Cross_State_Entry> entries;
	region.cross_state_snapshot(entries);
	ASSERT_EQ(entries.size(), 1u);
	EXPECT_FALSE(entries[0].known);

	//检测后位于空间内
	ASSERT_TRUE(region.detect().has_value());
	entries.clear();
	region.cross_state_snapshot(entries);
	ASSERT_EQ(entries.size(), 1u);
	EXPECT_TRUE(entries[0].known);
	EXPECT_EQ(entries[0].state, engine::Cross_State::inside);

	//清理临时网格文件
	std::error_code ec;
	std::filesystem::remove(
		engine::detail::exe_dir_get() / engine::detail::string_to_path(mesh_name), ec);
}

//碰撞代理器测试夹具

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

//碰撞空间：含旋转的运动强制走离散分段（中间分段才与目标相交）
TEST_F(Collision_Region_Test, 旋转运动强制离散分段)
{
	//细长盒体（长轴沿 X）
	const uint64_t collider_A = region.collider_build();
	nlohmann::json config_A = nlohmann::json::object();
	config_A["collider_ID"] = collider_A;
	config_A["type"] = "box";
	config_A["half_extent"] = nlohmann::json::array({ 3.0, 0.5, 0.5 });
	config_A["position"] = nlohmann::json::array({ 0.0, 0.0, 0.0 });
	ASSERT_TRUE(region.collider_set(config_A));

	//目标盒体位于第一象限
	const uint64_t collider_B = region.collider_build();
	nlohmann::json config_B = box_config(collider_B, 0.0);
	config_B["position"] = nlohmann::json::array({ 2.0, 2.0, 0.0 });
	ASSERT_TRUE(region.collider_set(config_B));

	//细长盒体启用离散检测，步长二
	engine::Detection_Mode mode;
	mode.is_swept_volume = false;
	mode.step_length = 2u;
	ASSERT_TRUE(region.collider_set(collider_A, mode));

	//运动事件：细长盒体绕 Z 轴旋转九十度（无平移）
	engine::Motion_Event motion_event;
	motion_event.body.rotation = engine::Quaternion::from_axis_angle(
		engine::Vector3(0.0f, 0.0f, 1.0f), 3.14159265f / 2.0f);
	region.motion_reader_set([&motion_event, collider_A](uint64_t collider_ID,
		engine::Motion_Event& receiver)
		{
			//仅细长盒体带运动
			if (collider_ID != collider_A)
				return false;
			//写入当前运动量
			receiver = motion_event;
			return true;
		});

	//激活碰撞空间
	region.state_set(true);

	/*
	零度与九十度姿态下细长盒体均不覆盖第一象限的目标，
	只有四十五度的中间分段会与目标相交，故检测结果证明按段推进。
	*/
	std::optional<std::vector<engine::Collision_Result>> detected = region.detect();
	ASSERT_TRUE(detected.has_value());
	ASSERT_EQ(detected->size(), 1u);
}

//碰撞空间：同一碰撞体的子部件自碰，结果精确到子部件编号
TEST_F(Collision_Region_Test, 自碰上报子部件编号)
{
	//单个碰撞体（两个相互重叠的子部件）
	const uint64_t collider_ID = region.collider_build();
	nlohmann::json config = nlohmann::json::object();
	config["collider_ID"] = collider_ID;
	//左侧子部件
	nlohmann::json part_left = nlohmann::json::object();
	part_left["type"] = "box";
	part_left["half_extent"] = nlohmann::json::array({ 1.0, 1.0, 1.0 });
	part_left["position"] = nlohmann::json::array({ 0.0, 0.0, 0.0 });
	part_left["part_ID"] = 5u;
	//右侧子部件（与左侧重叠）
	nlohmann::json part_right = nlohmann::json::object();
	part_right["type"] = "box";
	part_right["half_extent"] = nlohmann::json::array({ 1.0, 1.0, 1.0 });
	part_right["position"] = nlohmann::json::array({ 0.5, 0.0, 0.0 });
	part_right["part_ID"] = 9u;
	config["geometries"] = nlohmann::json::array({ part_left, part_right });
	ASSERT_TRUE(region.collider_set(config));
	region.state_set(true);

	//检测应上报同一碰撞体的子部件自碰
	std::optional<std::vector<engine::Collision_Result>> detected = region.detect();
	ASSERT_TRUE(detected.has_value());
	ASSERT_EQ(detected->size(), 1u);
	//碰撞体编号相同，子部件编号必须不同
	EXPECT_EQ((*detected)[0].collider_A, collider_ID);
	EXPECT_EQ((*detected)[0].collider_B, collider_ID);
	EXPECT_NE((*detected)[0].part_A, (*detected)[0].part_B);
	//两侧子部件编号应分别为五与九
	bool pair_match = ((*detected)[0].part_A == 5u && (*detected)[0].part_B == 9u) ||
		((*detected)[0].part_A == 9u && (*detected)[0].part_B == 5u);
	EXPECT_TRUE(pair_match);
}

//碰撞空间：子部件忽略对生效（对称，双方均不再参与检测）
TEST_F(Collision_Region_Test, 子部件忽略对生效)
{
	//单个碰撞体（两个相互重叠的子部件，编号五与九）
	const uint64_t collider_ID = region.collider_build();
	nlohmann::json config = nlohmann::json::object();
	config["collider_ID"] = collider_ID;
	nlohmann::json part_left = nlohmann::json::object();
	part_left["type"] = "box";
	part_left["half_extent"] = nlohmann::json::array({ 1.0, 1.0, 1.0 });
	part_left["position"] = nlohmann::json::array({ 0.0, 0.0, 0.0 });
	part_left["part_ID"] = 5u;
	nlohmann::json part_right = nlohmann::json::object();
	part_right["type"] = "box";
	part_right["half_extent"] = nlohmann::json::array({ 1.0, 1.0, 1.0 });
	part_right["position"] = nlohmann::json::array({ 0.5, 0.0, 0.0 });
	part_right["part_ID"] = 9u;
	config["geometries"] = nlohmann::json::array({ part_left, part_right });
	config["part_ignore"] = nlohmann::json::array({ nlohmann::json::array({ 5, 9 }) });
	ASSERT_TRUE(region.collider_set(config));
	region.state_set(true);

	//忽略对生效后自碰被宽阶段过滤
	std::optional<std::vector<engine::Collision_Result>> detected = region.detect();
	ASSERT_TRUE(detected.has_value());
	EXPECT_TRUE(detected->empty());

	//移除忽略对后自碰重新上报
	config.erase("part_ignore");
	ASSERT_TRUE(region.collider_set(config));
	detected = region.detect();
	ASSERT_TRUE(detected.has_value());
	ASSERT_EQ(detected->size(), 1u);
}

//碰撞空间：扫掠命中上报目标侧子部件编号
TEST_F(Collision_Region_Test, 扫掠命中上报目标子部件编号)
{
	//目标碰撞体（两个部件：编号五位于零点，编号九位于十个单位处）
	const uint64_t target = region.collider_build();
	nlohmann::json config = nlohmann::json::object();
	config["collider_ID"] = target;
	nlohmann::json part_near = nlohmann::json::object();
	part_near["type"] = "box";
	part_near["half_extent"] = nlohmann::json::array({ 1.0, 1.0, 1.0 });
	part_near["position"] = nlohmann::json::array({ 0.0, 0.0, 0.0 });
	part_near["part_ID"] = 5u;
	nlohmann::json part_far = nlohmann::json::object();
	part_far["type"] = "box";
	part_far["half_extent"] = nlohmann::json::array({ 1.0, 1.0, 1.0 });
	part_far["position"] = nlohmann::json::array({ 10.0, 0.0, 0.0 });
	part_far["part_ID"] = 9u;
	config["geometries"] = nlohmann::json::array({ part_near, part_far });
	ASSERT_TRUE(region.collider_set(config));

	//移动体（单部件，位于负六个单位处）
	const uint64_t mover = region.collider_build();
	ASSERT_TRUE(region.collider_set(box_config(mover, -6.0)));
	//移动体启用扫掠检测
	engine::Detection_Mode mode;
	mode.is_swept_volume = true;
	mode.step_length = 0u;
	ASSERT_TRUE(region.collider_set(mover, mode));

	//运动事件：移动体沿正 X 方向推进八个单位
	engine::Motion_Event motion_event;
	motion_event.body.translation = engine::Vector3(8.0f, 0.0f, 0.0f);
	region.motion_reader_set([&motion_event, mover](uint64_t collider_ID,
		engine::Motion_Event& receiver)
		{
			//仅移动体带运动
			if (collider_ID != mover)
				return false;
			//写入当前运动量
			receiver = motion_event;
			return true;
		});
	region.state_set(true);

	//扫掠应命中目标近侧部件，并上报该部件的编号
	std::optional<std::vector<engine::Collision_Result>> detected = region.detect();
	ASSERT_TRUE(detected.has_value());
	ASSERT_EQ(detected->size(), 1u);
	EXPECT_EQ((*detected)[0].collider_A, target);
	EXPECT_EQ((*detected)[0].collider_B, mover);
	EXPECT_EQ((*detected)[0].part_A, 5u);
	EXPECT_EQ((*detected)[0].part_B, 0u);
}

//碰撞空间：豁免标记变更后宽阶段配对被重建
TEST_F(Collision_Region_Test, 豁免标记变更刷新配对)
{
	//两个相互重叠的单部件碰撞体
	const uint64_t collider_A = region.collider_build();
	const uint64_t collider_B = region.collider_build();
	ASSERT_TRUE(region.collider_set(box_config(collider_A, 0.0)));
	ASSERT_TRUE(region.collider_set(box_config(collider_B, 1.0)));
	region.state_set(true);

	//未设置豁免时照常上报
	std::optional<std::vector<engine::Collision_Result>> detected = region.detect();
	ASSERT_TRUE(detected.has_value());
	ASSERT_EQ(detected->size(), 1u);

	//两侧同组非零即互免
	ASSERT_TRUE(region.collider_set(collider_A, static_cast<uint64_t>(7)));
	ASSERT_TRUE(region.collider_set(collider_B, static_cast<uint64_t>(7)));
	detected = region.detect();
	ASSERT_TRUE(detected.has_value());
	EXPECT_TRUE(detected->empty());

	//清除豁免标记后配对重建并恢复上报
	ASSERT_TRUE(region.collider_set(collider_A, static_cast<uint64_t>(0)));
	ASSERT_TRUE(region.collider_set(collider_B, static_cast<uint64_t>(0)));
	detected = region.detect();
	ASSERT_TRUE(detected.has_value());
	ASSERT_EQ(detected->size(), 1u);
}

//碰撞空间：作用频率门控对旋转运动生效
TEST_F(Collision_Region_Test, 频率门控对旋转生效)
{
	//逻辑帧率与帧计数复位（频率门控依赖两者）
	engine::Logics::logic_frames_set(60);
	engine::Logics::frame_count_set(0);

	//细长盒体（长轴沿 X）
	const uint64_t collider_A = region.collider_build();
	nlohmann::json config_A = nlohmann::json::object();
	config_A["collider_ID"] = collider_A;
	config_A["type"] = "box";
	config_A["half_extent"] = nlohmann::json::array({ 3.0, 0.5, 0.5 });
	config_A["position"] = nlohmann::json::array({ 0.0, 0.0, 0.0 });
	ASSERT_TRUE(region.collider_set(config_A));

	//目标盒体位于短轴正上方的远处
	const uint64_t collider_B = region.collider_build();
	nlohmann::json config_B = nlohmann::json::object();
	config_B["collider_ID"] = collider_B;
	config_B["type"] = "box";
	config_B["half_extent"] = nlohmann::json::array({ 0.3, 0.3, 0.3 });
	config_B["position"] = nlohmann::json::array({ 0.0, 2.2, 0.0 });
	ASSERT_TRUE(region.collider_set(config_B));

	//细长盒体以半步一帧的频率旋转（逻辑帧率六十、作用频率二即间隔三十帧）
	engine::Detection_Mode mode;
	mode.is_swept_volume = false;
	mode.step_length = 1u;
	ASSERT_TRUE(region.collider_set(collider_A, mode));
	ASSERT_TRUE(region.collider_frequency_set(collider_A, 2));

	//运动事件：细长盒体绕 Z 轴旋转九十度
	engine::Motion_Event motion_event;
	motion_event.body.rotation = engine::Quaternion::from_axis_angle(
		engine::Vector3(0.0f, 0.0f, 1.0f), 3.14159265f / 2.0f);
	region.motion_reader_set([&motion_event, collider_A](uint64_t collider_ID,
		engine::Motion_Event& receiver)
		{
			//仅细长盒体带运动
			if (collider_ID != collider_A)
				return false;
			//写入当前运动量
			receiver = motion_event;
			return true;
		});

	//激活碰撞空间
	region.state_set(true);

	//第零帧与第五帧均未达间隔，不施加任何旋转
	std::optional<std::vector<engine::Collision_Result>> detected = region.detect();
	ASSERT_TRUE(detected.has_value());
	EXPECT_TRUE(detected->empty());
	for (uint32_t tick = 0; tick < 5; ++tick)
		engine::Logics::frame_tick();
	ASSERT_TRUE(region.detect().has_value());
	EXPECT_TRUE(region.detect()->empty());

	//推进到第三十帧：施加四十五度旋转，仍未与目标相交
	for (uint32_t tick = 0; tick < 25; ++tick)
		engine::Logics::frame_tick();
	detected = region.detect();
	ASSERT_TRUE(detected.has_value());
	EXPECT_TRUE(detected->empty());

	//推进到第六十帧：再施加四十五度累计至九十度，长轴扫过目标
	for (uint32_t tick = 0; tick < 30; ++tick)
		engine::Logics::frame_tick();
	detected = region.detect();
	ASSERT_TRUE(detected.has_value());
	ASSERT_EQ(detected->size(), 1u);

	//逻辑帧率复位，避免影响其余用例
	engine::Logics::logic_frames_set(0);
	engine::Logics::frame_count_set(0);
}