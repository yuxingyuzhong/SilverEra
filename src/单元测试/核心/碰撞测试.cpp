//碰撞模块测试：覆盖碰撞体搬移、碰撞空间构建与检测、空间边界与碰撞代理器的事件接入
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>

//获取碰撞代理器（含碰撞空间、碰撞体与事件系统）
#include "src/core/spatial/collision/Collision_Proxy/碰撞代理器.h"

//构造盒体几何配置
static nlohmann::json box_config(const uint64_t collider_ID, const double position_X)
{
	//几何配置
	nlohmann::json config = nlohmann::json::object();
	//写入目标碰撞体编号
	config["collider_ID"] = collider_ID;
	//写入形状类型
	config["type"] = "box";
	//写入盒体半长
	config["half_extent"] = nlohmann::json::array({ 1.0, 1.0, 1.0 });
	//写入初始位置
	config["position"] = nlohmann::json::array({ position_X, 0.0, 0.0 });
	return config;
}

//构造碰撞体事件载荷
static nlohmann::json collider_payload(const std::string& region, const uint64_t collider_ID)
{
	//事件载荷
	nlohmann::json payload = nlohmann::json::object();
	//写入空间名称与碰撞体编号
	payload["region"] = region;
	payload["collider_ID"] = collider_ID;
	return payload;
}

//构造碰撞模块事件
static std::shared_ptr<engine::event> make_event(const std::string& tag,
	const nlohmann::json& config)
{
	return std::make_shared<engine::event>("", "", "Collision", tag, config);
}

//临时网格文件（相对可执行文件目录）
static const std::string temp_mesh_name = "src/单元测试/核心/碰撞测试临时网格.obj";

//写入手工立方体网格文件，返回可读取的相对路径
static std::string temp_mesh_write(void)
{
	//写入路径
	std::filesystem::path mesh_path =
		engine::Engine_Env::exe_dir_get() / engine::string_to_path(temp_mesh_name);

	//打开临时网格文件
	std::ofstream file(mesh_path);
	//写入八个顶点（立方体半长为一）
	file << "v -1.0 -1.0 -1.0\n";
	file << "v 1.0 -1.0 -1.0\n";
	file << "v 1.0 1.0 -1.0\n";
	file << "v -1.0 1.0 -1.0\n";
	file << "v -1.0 -1.0 1.0\n";
	file << "v 1.0 -1.0 1.0\n";
	file << "v 1.0 1.0 1.0\n";
	file << "v -1.0 1.0 1.0\n";
	//写入十二个三角面
	file << "f 1 2 3\n" << "f 1 3 4\n";
	file << "f 5 7 6\n" << "f 5 8 7\n";
	file << "f 1 6 2\n" << "f 1 5 6\n";
	file << "f 2 7 3\n" << "f 2 6 7\n";
	file << "f 3 8 4\n" << "f 3 7 8\n";
	file << "f 4 5 1\n" << "f 4 8 5\n";
	//关闭文件
	file.close();

	return temp_mesh_name;
}

//临时边界配置文件（相对可执行文件目录）
static const std::string temp_boundary_name = "src/单元测试/核心/碰撞测试临时边界.json";

//写入边界配置文件（登记空间的边界网格路径），返回可读取的相对路径
static std::string temp_boundary_write(const std::string& region, const std::string& mesh_name)
{
	//写入路径
	std::filesystem::path config_path =
		engine::Engine_Env::exe_dir_get() / engine::string_to_path(temp_boundary_name);

	//打开临时边界配置文件
	std::ofstream file(config_path);
	//写入边界配置（空间数组：空间名称 + 边界网格路径）
	file << "[{\"region\":\"" << region << "\",\"mesh_path\":\"" << mesh_name << "\"}]";
	//关闭文件
	file.close();

	return temp_boundary_name;
}

//碰撞体测试夹具
class Collider_Test : public ::testing::Test
{
};

//碰撞体：默认构造即挂载自指针，可直接反查
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
class Collision_Region_Test : public ::testing::Test
{
protected:
	//被测碰撞空间
	engine::Collision_Region region{ "测试空间" };
};

//碰撞空间：后端可用、默认非激活、状态可设置
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
	EXPECT_FALSE(region.boundary_build("src/单元测试/核心/不存在的网格.obj"));

	//卸载空间边界
	region.boundary_unload();

	//清理临时网格文件
	std::error_code ec;
	std::filesystem::remove(
		engine::Engine_Env::exe_dir_get() / engine::string_to_path(mesh_name), ec);
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
		engine::Engine_Env::exe_dir_get() / engine::string_to_path(mesh_name), ec);
}

//碰撞代理器测试夹具
class Collision_Proxy_Test : public ::testing::Test
{
protected:
	//被测碰撞代理器
	engine::Collision_Proxy proxy;
};

//碰撞代理器：空间构建、重复构建与卸载
TEST_F(Collision_Proxy_Test, 空间构建与卸载)
{
	//构建空间
	EXPECT_TRUE(proxy.region_build("空间甲"));
	//重复构建失败
	EXPECT_FALSE(proxy.region_build("空间甲"));
	//未构建的空间无法设置活跃性
	EXPECT_FALSE(proxy.region_state_set("空间乙", true));
	//卸载空间
	EXPECT_TRUE(proxy.region_unload("空间甲"));
	//重复卸载失败
	EXPECT_FALSE(proxy.region_unload("空间甲"));
}

//碰撞代理器：碰撞体构建返回编号，卸载需指定归属空间
TEST_F(Collision_Proxy_Test, 碰撞体构建与卸载)
{
	//构建空间
	ASSERT_TRUE(proxy.region_build("空间甲"));

	//构建碰撞体
	std::optional<uint64_t> collider_ID = proxy.collider_build("空间甲");
	ASSERT_TRUE(collider_ID.has_value());
	//不存在的空间无法构建碰撞体
	EXPECT_FALSE(proxy.collider_build("空间乙").has_value());

	//卸载碰撞体
	EXPECT_TRUE(proxy.collider_unload(*collider_ID, "空间甲"));
	//重复卸载失败
	EXPECT_FALSE(proxy.collider_unload(*collider_ID, "空间甲"));
}

//碰撞代理器：按编号反查归属空间并分发设置
TEST_F(Collision_Proxy_Test, 碰撞体设置按编号分发)
{
	//构建空间与碰撞体
	ASSERT_TRUE(proxy.region_build("空间甲"));
	std::optional<uint64_t> collider_ID = proxy.collider_build("空间甲");
	ASSERT_TRUE(collider_ID.has_value());

	//按编号设置检测方式（代理器自行反查归属空间）
	engine::Detection_Mode detection_mode;
	detection_mode.is_swept_volume = true;
	detection_mode.step_length = 4;
	EXPECT_TRUE(proxy.collider_set(*collider_ID, detection_mode));
	//设置豁免标记
	EXPECT_TRUE(proxy.collider_set(*collider_ID, static_cast<uint64_t>(3)));
	//不存在的编号设置失败
	EXPECT_FALSE(proxy.collider_set(*collider_ID + 100, detection_mode));
}

//碰撞代理器：镜像产生新编号，转移保留原编号
TEST_F(Collision_Proxy_Test, 碰撞体镜像与转移)
{
	//构建两个空间
	ASSERT_TRUE(proxy.region_build("空间甲"));
	ASSERT_TRUE(proxy.region_build("空间乙"));

	//在源空间内构建碰撞体
	std::optional<uint64_t> source_ID = proxy.collider_build("空间甲");
	ASSERT_TRUE(source_ID.has_value());

	//镜像：源空间内得到独立的新碰撞体（占用新编号）
	ASSERT_TRUE(proxy.collider_mirror(*source_ID, "空间甲"));
	std::optional<uint64_t> mirror_check = proxy.collider_build("空间甲");
	ASSERT_TRUE(mirror_check.has_value());
	EXPECT_GT(*mirror_check, *source_ID);

	//转移：目标空间接管并保留原编号
	ASSERT_TRUE(proxy.collider_transfer(*source_ID, "空间乙"));
	//转移后按编号设置仍可命中目标空间
	EXPECT_TRUE(proxy.collider_set(*source_ID, static_cast<uint64_t>(1)));
	//已在目标空间内的碰撞体无法再次转移
	EXPECT_FALSE(proxy.collider_transfer(*source_ID, "空间乙"));
	//不存在的编号无法转移
	EXPECT_FALSE(proxy.collider_transfer(*source_ID + 100, "空间乙"));
}

//碰撞代理器：未注册中转站接入入口时不执行接入且不崩溃
TEST_F(Collision_Proxy_Test, 未注册接入入口时不接入)
{
	//未注册接入入口时接入应安全中止
	EXPECT_NO_THROW(proxy.attach());
}

//碰撞代理器：事件接入时登记模块名与订阅清单
TEST_F(Collision_Proxy_Test, 事件接入登记订阅清单)
{
	//中转站接入入口接到的模块名
	std::string attached_name;
	//中转站接入入口接到的订阅清单
	std::vector<engine::event> attached_events;

	//注册中转站接入入口
	proxy.event_terminal->attach_handler_register(
		[&](auto&& module_name, auto&& needed_events, auto&& event_entry)
		{
			//留存模块名
			attached_name = module_name;
			//留存订阅清单
			attached_events = needed_events;
		});

	//接入事件中转站
	proxy.attach();

	//模块名应为碰撞代理器
	EXPECT_EQ(attached_name, "Collision_Proxy");
	//订阅清单应包含配置路由与全部碰撞指令
	EXPECT_EQ(attached_events.size(), 13u);
}

//碰撞代理器：事件驱动构建空间与碰撞体，并回告编号
TEST_F(Collision_Proxy_Test, 事件驱动碰撞体构建)
{
	//已发送事件集合
	std::vector<std::shared_ptr<engine::event>> sent;
	//接入入口接到的接收通道
	std::function<void(std::shared_ptr<engine::event>)> entry;

	//注册发送通道与接入入口
	proxy.event_terminal->event_sender_register(
		[&sent](std::shared_ptr<engine::event> evt) { sent.push_back(evt); });
	proxy.event_terminal->attach_handler_register(
		[&entry](auto&&, auto&&, auto&& event_entry) { entry = event_entry; });
	proxy.attach();
	ASSERT_TRUE(static_cast<bool>(entry));

	//空间构建事件载荷
	nlohmann::json region_payload = nlohmann::json::object();
	region_payload["region"] = "事件空间";

	//经事件构建空间与两个碰撞体
	entry(make_event("RegionBuild", region_payload));
	entry(make_event("ColliderBuild", region_payload));
	entry(make_event("ColliderBuild", region_payload));

	//两个碰撞体构建结果事件应回告编号
	ASSERT_EQ(sent.size(), 2u);
	EXPECT_EQ(sent[0]->category, "Collision");
	EXPECT_EQ(sent[0]->tag, "ColliderBuildResult");
	EXPECT_EQ(sent[0]->config["region"], "事件空间");
	const uint64_t collider_A = sent[0]->config["collider_ID"].get<uint64_t>();
	const uint64_t collider_B = sent[1]->config["collider_ID"].get<uint64_t>();
	EXPECT_NE(collider_A, collider_B);
}

//碰撞代理器：事件全链路驱动检测并发布碰撞对
TEST_F(Collision_Proxy_Test, 事件驱动检测链路)
{
	//已发送事件集合
	std::vector<std::shared_ptr<engine::event>> sent;
	//接入入口接到的接收通道
	std::function<void(std::shared_ptr<engine::event>)> entry;

	//注册发送通道与接入入口
	proxy.event_terminal->event_sender_register(
		[&sent](std::shared_ptr<engine::event> evt) { sent.push_back(evt); });
	proxy.event_terminal->attach_handler_register(
		[&entry](auto&&, auto&&, auto&& event_entry) { entry = event_entry; });
	proxy.attach();
	ASSERT_TRUE(static_cast<bool>(entry));

	//空间构建事件载荷
	nlohmann::json region_payload = nlohmann::json::object();
	region_payload["region"] = "事件空间";

	//构建空间与两个碰撞体
	entry(make_event("RegionBuild", region_payload));
	entry(make_event("ColliderBuild", region_payload));
	entry(make_event("ColliderBuild", region_payload));
	ASSERT_EQ(sent.size(), 2u);
	const uint64_t collider_A = sent[0]->config["collider_ID"].get<uint64_t>();
	const uint64_t collider_B = sent[1]->config["collider_ID"].get<uint64_t>();

	//经事件设置两个盒体的几何体（相距五个单位）
	//空间已由事件构建，重复构建应失败
	EXPECT_FALSE(proxy.region_build("事件空间"));
	nlohmann::json set_A = collider_payload("事件空间", collider_A);
	set_A["geometry"] = box_config(collider_A, 0.0);
	set_A["geometry"].erase("collider_ID");
	sent.clear();
	entry(make_event("ColliderSet", set_A));

	nlohmann::json set_B = collider_payload("事件空间", collider_B);
	set_B["geometry"] = box_config(collider_B, 5.0);
	set_B["geometry"].erase("collider_ID");
	entry(make_event("ColliderSet", set_B));

	//经事件激活空间
	nlohmann::json state_payload = region_payload;
	state_payload["active"] = true;
	entry(make_event("RegionState", state_payload));

	//距离五个单位时检测结果为空
	sent.clear();
	entry(make_event("RegionDetect", region_payload));
	ASSERT_EQ(sent.size(), 1u);
	EXPECT_EQ(sent[0]->tag, "DetectResult");
	EXPECT_EQ(sent[0]->config["region"], "事件空间");
	ASSERT_TRUE(sent[0]->config["results"].is_array());
	EXPECT_TRUE(sent[0]->config["results"].empty());

	//经位移事件把第二个盒体移入重叠位置
	sent.clear();
	nlohmann::json move_B = collider_payload("事件空间", collider_B);
	move_B["displacement"] = nlohmann::json::array({ -4.0, 0.0, 0.0 });
	entry(make_event("ColliderDisplacement", move_B));
	entry(make_event("RegionDetect", region_payload));

	//检测结果应报告一对碰撞
	ASSERT_FALSE(sent.empty());
	const std::shared_ptr<engine::event> result = sent.back();
	EXPECT_EQ(result->tag, "DetectResult");
	ASSERT_EQ(result->config["results"].size(), 1u);
	EXPECT_EQ(result->config["results"][0]["collider_A"].get<uint64_t>(), collider_A);
	EXPECT_EQ(result->config["results"][0]["collider_B"].get<uint64_t>(), collider_B);
}

//碰撞代理器：同一位移事件在连续检测中持续生效（位移不再一次性清零）
TEST_F(Collision_Proxy_Test, 位移事件持续生效)
{
	//已发送事件集合
	std::vector<std::shared_ptr<engine::event>> sent;
	//接入入口接到的接收通道
	std::function<void(std::shared_ptr<engine::event>)> entry;

	//注册发送通道与接入入口
	proxy.event_terminal->event_sender_register(
		[&sent](std::shared_ptr<engine::event> evt) { sent.push_back(evt); });
	proxy.event_terminal->attach_handler_register(
		[&entry](auto&&, auto&&, auto&& event_entry) { entry = event_entry; });
	proxy.attach();
	ASSERT_TRUE(static_cast<bool>(entry));

	//构建空间与两个盒体
	ASSERT_TRUE(proxy.region_build("位移空间"));
	std::optional<uint64_t> collider_A = proxy.collider_build("位移空间");
	std::optional<uint64_t> collider_B = proxy.collider_build("位移空间");
	ASSERT_TRUE(collider_A.has_value());
	ASSERT_TRUE(collider_B.has_value());

	//经事件设置两个盒体的几何体（相距五个单位）
	nlohmann::json set_A = collider_payload("位移空间", *collider_A);
	set_A["geometry"] = box_config(*collider_A, 0.0);
	set_A["geometry"].erase("collider_ID");
	entry(make_event("ColliderSet", set_A));

	nlohmann::json set_B = collider_payload("位移空间", *collider_B);
	set_B["geometry"] = box_config(*collider_B, 5.0);
	set_B["geometry"].erase("collider_ID");
	entry(make_event("ColliderSet", set_B));

	//经事件为第二个盒体提交一次位移（持续向左推进四个单位）
	nlohmann::json displacement = collider_payload("位移空间", *collider_B);
	displacement["displacement"] = nlohmann::json::array({ -4.0, 0.0, 0.0 });
	entry(make_event("ColliderDisplacement", displacement));

	//激活碰撞空间
	ASSERT_TRUE(proxy.region_state_set("位移空间", true));

	//第一次检测：位移生效使两盒体重叠，报告一对碰撞
	sent.clear();
	ASSERT_TRUE(proxy.region_detect("位移空间"));
	ASSERT_FALSE(sent.empty());
	ASSERT_EQ(sent.back()->config["results"].size(), 1u);

	//第二次检测：位移持续生效使盒体继续推进至远离位置，不再报告碰撞
	sent.clear();
	ASSERT_TRUE(proxy.region_detect("位移空间"));
	ASSERT_FALSE(sent.empty());
	EXPECT_TRUE(sent.back()->config["results"].empty());
}

//碰撞代理器：几何体集合经事件通道设置并参与检测
TEST_F(Collision_Proxy_Test, 几何体集合经事件设置生效)
{
	//已发送事件集合
	std::vector<std::shared_ptr<engine::event>> sent;
	//接入入口接到的接收通道
	std::function<void(std::shared_ptr<engine::event>)> entry;

	//注册发送通道与接入入口
	proxy.event_terminal->event_sender_register(
		[&sent](std::shared_ptr<engine::event> evt) { sent.push_back(evt); });
	proxy.event_terminal->attach_handler_register(
		[&entry](auto&&, auto&&, auto&& event_entry) { entry = event_entry; });
	proxy.attach();
	ASSERT_TRUE(static_cast<bool>(entry));

	//空间构建事件载荷
	nlohmann::json region_payload = nlohmann::json::object();
	region_payload["region"] = "集合空间";

	//构建空间与两个碰撞体
	entry(make_event("RegionBuild", region_payload));
	entry(make_event("ColliderBuild", region_payload));
	entry(make_event("ColliderBuild", region_payload));
	ASSERT_EQ(sent.size(), 2u);
	const uint64_t collider_A = sent[0]->config["collider_ID"].get<uint64_t>();
	const uint64_t collider_B = sent[1]->config["collider_ID"].get<uint64_t>();

	//经事件把碰撞体A设置为几何体集合（两个盒体分别位于本体两侧五个单位处）
	nlohmann::json set_A = collider_payload("集合空间", collider_A);
	set_A["geometry"] = nlohmann::json::array();
	//左侧部件
	nlohmann::json part_left = nlohmann::json::object();
	part_left["type"] = "box";
	part_left["half_extent"] = nlohmann::json::array({ 1.0, 1.0, 1.0 });
	part_left["position"] = nlohmann::json::array({ -5.0, 0.0, 0.0 });
	set_A["geometry"].push_back(part_left);
	//右侧部件
	nlohmann::json part_right = nlohmann::json::object();
	part_right["type"] = "box";
	part_right["half_extent"] = nlohmann::json::array({ 1.0, 1.0, 1.0 });
	part_right["position"] = nlohmann::json::array({ 5.0, 0.0, 0.0 });
	set_A["geometry"].push_back(part_right);
	sent.clear();
	entry(make_event("ColliderSet", set_A));

	//经事件设置碰撞体B（单体盒体，仅与A的右侧部件重叠）
	nlohmann::json set_B = collider_payload("集合空间", collider_B);
	set_B["geometry"] = box_config(collider_B, 5.5);
	set_B["geometry"].erase("collider_ID");
	entry(make_event("ColliderSet", set_B));

	//经事件激活空间
	nlohmann::json state_payload = region_payload;
	state_payload["active"] = true;
	entry(make_event("RegionState", state_payload));

	//检测应报告一对碰撞
	sent.clear();
	entry(make_event("RegionDetect", region_payload));
	ASSERT_FALSE(sent.empty());
	const std::shared_ptr<engine::event> result = sent.back();
	ASSERT_EQ(result->tag, "DetectResult");
	ASSERT_EQ(result->config["results"].size(), 1u);
	EXPECT_EQ(result->config["results"][0]["collider_A"].get<uint64_t>(), collider_A);
	EXPECT_EQ(result->config["results"][0]["collider_B"].get<uint64_t>(), collider_B);
}

//碰撞空间：位移作废后不再施加位移，位移改写解除作废后按新向量运动
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
TEST_F(Collision_Proxy_Test, 碰撞响应停止运动作废位移)
{
	//已发送事件集合
	std::vector<std::shared_ptr<engine::event>> sent;
	//接入入口接到的接收通道
	std::function<void(std::shared_ptr<engine::event>)> entry;

	//注册发送通道：收到碰撞事件时立即按编号回发响应（模拟外部同步应答）
	proxy.event_terminal->event_sender_register(
		[&sent, &entry](std::shared_ptr<engine::event> evt)
		{
			sent.push_back(evt);
			//仅对碰撞事件回发响应
			if (evt->tag != "ColliderCollision" || !entry)
				return;
			//响应载荷（一律停止运动）
			nlohmann::json response = nlohmann::json::object();
			response["collider_ID"] = evt->config["collider_ID"];
			response["response"] = "stop";
			//回发响应事件
			entry(std::make_shared<engine::event>(
				"", "Collision_Proxy", "Collision", "ColliderCollisionResponse", response));
		});
	proxy.event_terminal->attach_handler_register(
		[&entry](auto&&, auto&&, auto&& event_entry) { entry = event_entry; });
	proxy.attach();
	ASSERT_TRUE(static_cast<bool>(entry));

	//构建空间与两个盒体
	ASSERT_TRUE(proxy.region_build("响应空间"));
	std::optional<uint64_t> collider_A = proxy.collider_build("响应空间");
	std::optional<uint64_t> collider_B = proxy.collider_build("响应空间");
	ASSERT_TRUE(collider_A.has_value());
	ASSERT_TRUE(collider_B.has_value());

	//设置几何体（相距五个单位）
	nlohmann::json set_A = collider_payload("响应空间", *collider_A);
	set_A["geometry"] = box_config(*collider_A, 0.0);
	set_A["geometry"].erase("collider_ID");
	entry(make_event("ColliderSet", set_A));
	nlohmann::json set_B = collider_payload("响应空间", *collider_B);
	set_B["geometry"] = box_config(*collider_B, 5.0);
	set_B["geometry"].erase("collider_ID");
	entry(make_event("ColliderSet", set_B));

	//提交位移事件（向左推进四个单位）
	nlohmann::json displacement = collider_payload("响应空间", *collider_B);
	displacement["displacement"] = nlohmann::json::array({ -4.0, 0.0, 0.0 });
	entry(make_event("ColliderDisplacement", displacement));

	//激活碰撞空间
	ASSERT_TRUE(proxy.region_state_set("响应空间", true));

	//第一次检测：位移生效使两盒体重叠，报告一对碰撞
	sent.clear();
	ASSERT_TRUE(proxy.region_detect("响应空间"));
	ASSERT_FALSE(sent.empty());
	const std::shared_ptr<engine::event> first = sent.back();
	ASSERT_EQ(first->tag, "DetectResult");
	ASSERT_EQ(first->config["results"].size(), 1u);

	//第二次检测：位移已作废，盒体不再推进，仍停留在重叠位置
	sent.clear();
	ASSERT_TRUE(proxy.region_detect("响应空间"));
	ASSERT_FALSE(sent.empty());
	const std::shared_ptr<engine::event> second = sent.back();
	ASSERT_EQ(second->tag, "DetectResult");
	EXPECT_EQ(second->config["results"].size(), 1u);
}

//碰撞代理器：碰撞响应为继续运动且位移保持时，碰撞体继续推进
TEST_F(Collision_Proxy_Test, 碰撞响应继续运动保持位移)
{
	//已发送事件集合
	std::vector<std::shared_ptr<engine::event>> sent;
	//接入入口接到的接收通道
	std::function<void(std::shared_ptr<engine::event>)> entry;

	//注册发送通道：收到碰撞事件时立即回发"保持位移"响应
	proxy.event_terminal->event_sender_register(
		[&sent, &entry](std::shared_ptr<engine::event> evt)
		{
			sent.push_back(evt);
			//仅对碰撞事件回发响应
			if (evt->tag != "ColliderCollision" || !entry)
				return;
			//响应载荷（一律保持位移）
			nlohmann::json response = nlohmann::json::object();
			response["collider_ID"] = evt->config["collider_ID"];
			response["response"] = "keep";
			//回发响应事件
			entry(std::make_shared<engine::event>(
				"", "Collision_Proxy", "Collision", "ColliderCollisionResponse", response));
		});
	proxy.event_terminal->attach_handler_register(
		[&entry](auto&&, auto&&, auto&& event_entry) { entry = event_entry; });
	proxy.attach();
	ASSERT_TRUE(static_cast<bool>(entry));

	//构建空间与两个盒体
	ASSERT_TRUE(proxy.region_build("保持空间"));
	std::optional<uint64_t> collider_A = proxy.collider_build("保持空间");
	std::optional<uint64_t> collider_B = proxy.collider_build("保持空间");
	ASSERT_TRUE(collider_A.has_value());
	ASSERT_TRUE(collider_B.has_value());

	//设置几何体（相距五个单位）
	nlohmann::json set_A = collider_payload("保持空间", *collider_A);
	set_A["geometry"] = box_config(*collider_A, 0.0);
	set_A["geometry"].erase("collider_ID");
	entry(make_event("ColliderSet", set_A));
	nlohmann::json set_B = collider_payload("保持空间", *collider_B);
	set_B["geometry"] = box_config(*collider_B, 5.0);
	set_B["geometry"].erase("collider_ID");
	entry(make_event("ColliderSet", set_B));

	//提交位移事件（向左推进四个单位）
	nlohmann::json displacement = collider_payload("保持空间", *collider_B);
	displacement["displacement"] = nlohmann::json::array({ -4.0, 0.0, 0.0 });
	entry(make_event("ColliderDisplacement", displacement));

	//激活碰撞空间
	ASSERT_TRUE(proxy.region_state_set("保持空间", true));

	//第一次检测：位移生效使两盒体重叠，报告一对碰撞
	sent.clear();
	ASSERT_TRUE(proxy.region_detect("保持空间"));
	ASSERT_FALSE(sent.empty());
	ASSERT_EQ(sent.back()->config["results"].size(), 1u);

	//第二次检测：位移保持使盒体继续推进至远离位置，不再报告碰撞
	sent.clear();
	ASSERT_TRUE(proxy.region_detect("保持空间"));
	ASSERT_FALSE(sent.empty());
	EXPECT_EQ(sent.back()->tag, "DetectResult");
	EXPECT_TRUE(sent.back()->config["results"].empty());
}

//碰撞代理器：碰撞响应为继续运动且位移变化时，碰撞体改按新位移推进
TEST_F(Collision_Proxy_Test, 碰撞响应位移变化)
{
	//已发送事件集合
	std::vector<std::shared_ptr<engine::event>> sent;
	//接入入口接到的接收通道
	std::function<void(std::shared_ptr<engine::event>)> entry;
	//被改写位移的碰撞体编号
	uint64_t changed_ID = 0;

	//注册发送通道：对指定碰撞体回发"位移变化"响应，其余回发"保持位移"
	proxy.event_terminal->event_sender_register(
		[&sent, &entry, &changed_ID](std::shared_ptr<engine::event> evt)
		{
			sent.push_back(evt);
			//仅对碰撞事件回发响应
			if (evt->tag != "ColliderCollision" || !entry)
				return;
			//响应载荷
			nlohmann::json response = nlohmann::json::object();
			response["collider_ID"] = evt->config["collider_ID"];
			//指定碰撞体改为向右推进，其余保持位移
			if (evt->config["collider_ID"].get<uint64_t>() == changed_ID)
			{
				response["response"] = "change";
				response["displacement"] = nlohmann::json::array({ 4.0, 0.0, 0.0 });
			}
			else
				response["response"] = "keep";
			//回发响应事件
			entry(std::make_shared<engine::event>(
				"", "Collision_Proxy", "Collision", "ColliderCollisionResponse", response));
		});
	proxy.event_terminal->attach_handler_register(
		[&entry](auto&&, auto&&, auto&& event_entry) { entry = event_entry; });
	proxy.attach();
	ASSERT_TRUE(static_cast<bool>(entry));

	//构建空间与三个盒体（A 在原点，B 在五，C 在九）
	ASSERT_TRUE(proxy.region_build("变化空间"));
	std::optional<uint64_t> collider_A = proxy.collider_build("变化空间");
	std::optional<uint64_t> collider_B = proxy.collider_build("变化空间");
	std::optional<uint64_t> collider_C = proxy.collider_build("变化空间");
	ASSERT_TRUE(collider_A.has_value());
	ASSERT_TRUE(collider_B.has_value());
	ASSERT_TRUE(collider_C.has_value());
	changed_ID = *collider_B;

	//设置几何体
	nlohmann::json set_A = collider_payload("变化空间", *collider_A);
	set_A["geometry"] = box_config(*collider_A, 0.0);
	set_A["geometry"].erase("collider_ID");
	entry(make_event("ColliderSet", set_A));
	nlohmann::json set_B = collider_payload("变化空间", *collider_B);
	set_B["geometry"] = box_config(*collider_B, 5.0);
	set_B["geometry"].erase("collider_ID");
	entry(make_event("ColliderSet", set_B));
	nlohmann::json set_C = collider_payload("变化空间", *collider_C);
	set_C["geometry"] = box_config(*collider_C, 9.0);
	set_C["geometry"].erase("collider_ID");
	entry(make_event("ColliderSet", set_C));

	//提交位移事件（向左推进四个单位）
	nlohmann::json displacement = collider_payload("变化空间", *collider_B);
	displacement["displacement"] = nlohmann::json::array({ -4.0, 0.0, 0.0 });
	entry(make_event("ColliderDisplacement", displacement));

	//激活碰撞空间
	ASSERT_TRUE(proxy.region_state_set("变化空间", true));

	//第一次检测：B 左移与 A 重叠，报告一对碰撞；响应把 B 的位移改为向右推进
	sent.clear();
	ASSERT_TRUE(proxy.region_detect("变化空间"));
	ASSERT_FALSE(sent.empty());
	ASSERT_EQ(sent.back()->tag, "DetectResult");
	ASSERT_EQ(sent.back()->config["results"].size(), 1u);

	//第二次检测：B 改向右推进至初始位置，两侧均无重叠
	sent.clear();
	ASSERT_TRUE(proxy.region_detect("变化空间"));
	ASSERT_FALSE(sent.empty());
	ASSERT_EQ(sent.back()->tag, "DetectResult");
	EXPECT_TRUE(sent.back()->config["results"].empty());

	//第三次检测：B 继续右移与 C 重叠，证明位移已被改写为向右
	sent.clear();
	ASSERT_TRUE(proxy.region_detect("变化空间"));
	ASSERT_FALSE(sent.empty());
	ASSERT_EQ(sent.back()->tag, "DetectResult");
	ASSERT_EQ(sent.back()->config["results"].size(), 1u);
	EXPECT_EQ(sent.back()->config["results"][0]["collider_A"].get<uint64_t>(), *collider_B);
	EXPECT_EQ(sent.back()->config["results"][0]["collider_B"].get<uint64_t>(), *collider_C);
}

//碰撞代理器：碰撞响应缺失时搁置并重发，重发命中后按响应处理
TEST_F(Collision_Proxy_Test, 碰撞响应缺失时搁置重发)
{
	//已发送事件集合
	std::vector<std::shared_ptr<engine::event>> sent;
	//接入入口接到的接收通道
	std::function<void(std::shared_ptr<engine::event>)> entry;
	//各碰撞体收到的碰撞事件次数
	std::unordered_map<uint64_t, int> collision_times;
	//被重发后命中的碰撞体编号
	uint64_t replied_ID = 0;

	//注册发送通道：首个碰撞事件不回发响应，重发时才回发"停止运动"响应
	proxy.event_terminal->event_sender_register(
		[&sent, &entry, &collision_times, &replied_ID](std::shared_ptr<engine::event> evt)
		{
			sent.push_back(evt);
			//仅对碰撞事件计数与应答
			if (evt->tag != "ColliderCollision" || !entry)
				return;
			//目标碰撞体编号
			uint64_t collider_ID = evt->config["collider_ID"].get<uint64_t>();
			//累计该碰撞体收到的碰撞事件次数
			int times = ++collision_times[collider_ID];
			//首个碰撞事件不回发响应（制造搁置），重发时回发
			if (times < 2 || collider_ID != replied_ID)
				return;
			//响应载荷（停止运动）
			nlohmann::json response = nlohmann::json::object();
			response["collider_ID"] = collider_ID;
			response["response"] = "stop";
			//回发响应事件
			entry(std::make_shared<engine::event>(
				"", "Collision_Proxy", "Collision", "ColliderCollisionResponse", response));
		});
	proxy.event_terminal->attach_handler_register(
		[&entry](auto&&, auto&&, auto&& event_entry) { entry = event_entry; });
	proxy.attach();
	ASSERT_TRUE(static_cast<bool>(entry));

	//构建空间与两个盒体
	ASSERT_TRUE(proxy.region_build("搁置空间"));
	std::optional<uint64_t> collider_A = proxy.collider_build("搁置空间");
	std::optional<uint64_t> collider_B = proxy.collider_build("搁置空间");
	ASSERT_TRUE(collider_A.has_value());
	ASSERT_TRUE(collider_B.has_value());
	replied_ID = *collider_B;

	//设置几何体（相距五个单位）
	nlohmann::json set_A = collider_payload("搁置空间", *collider_A);
	set_A["geometry"] = box_config(*collider_A, 0.0);
	set_A["geometry"].erase("collider_ID");
	entry(make_event("ColliderSet", set_A));
	nlohmann::json set_B = collider_payload("搁置空间", *collider_B);
	set_B["geometry"] = box_config(*collider_B, 5.0);
	set_B["geometry"].erase("collider_ID");
	entry(make_event("ColliderSet", set_B));

	//提交位移事件（向左推进四个单位）
	nlohmann::json displacement = collider_payload("搁置空间", *collider_B);
	displacement["displacement"] = nlohmann::json::array({ -4.0, 0.0, 0.0 });
	entry(make_event("ColliderDisplacement", displacement));

	//激活碰撞空间
	ASSERT_TRUE(proxy.region_state_set("搁置空间", true));

	//第一次检测：碰撞事件发布后首个响应缺失，B 经重发才被应答
	sent.clear();
	ASSERT_TRUE(proxy.region_detect("搁置空间"));
	ASSERT_FALSE(sent.empty());
	ASSERT_EQ(sent.back()->tag, "DetectResult");
	ASSERT_EQ(sent.back()->config["results"].size(), 1u);
	//B 应收到两次碰撞事件（首次与重发），未被应答的 A 同样被重发两次
	EXPECT_EQ(collision_times[*collider_B], 2);
	EXPECT_EQ(collision_times[*collider_A], 2);

	//第二次检测：B 的停止运动响应已生效，盒体不再推进，仍停留在重叠位置
	sent.clear();
	ASSERT_TRUE(proxy.region_detect("搁置空间"));
	ASSERT_FALSE(sent.empty());
	ASSERT_EQ(sent.back()->tag, "DetectResult");
	EXPECT_EQ(sent.back()->config["results"].size(), 1u);
}

//碰撞代理器：碰撞响应始终缺失时搁置处理且不崩溃
TEST_F(Collision_Proxy_Test, 碰撞响应始终缺失时不崩溃)
{
	//已发送事件集合
	std::vector<std::shared_ptr<engine::event>> sent;
	//接入入口接到的接收通道
	std::function<void(std::shared_ptr<engine::event>)> entry;

	//注册发送通道：只记录不回发响应
	proxy.event_terminal->event_sender_register(
		[&sent](std::shared_ptr<engine::event> evt) { sent.push_back(evt); });
	proxy.event_terminal->attach_handler_register(
		[&entry](auto&&, auto&&, auto&& event_entry) { entry = event_entry; });
	proxy.attach();
	ASSERT_TRUE(static_cast<bool>(entry));

	//构建空间与两个盒体
	ASSERT_TRUE(proxy.region_build("无响应空间"));
	std::optional<uint64_t> collider_A = proxy.collider_build("无响应空间");
	std::optional<uint64_t> collider_B = proxy.collider_build("无响应空间");
	ASSERT_TRUE(collider_A.has_value());
	ASSERT_TRUE(collider_B.has_value());

	//设置几何体（相距五个单位）
	nlohmann::json set_A = collider_payload("无响应空间", *collider_A);
	set_A["geometry"] = box_config(*collider_A, 0.0);
	set_A["geometry"].erase("collider_ID");
	entry(make_event("ColliderSet", set_A));
	nlohmann::json set_B = collider_payload("无响应空间", *collider_B);
	set_B["geometry"] = box_config(*collider_B, 5.0);
	set_B["geometry"].erase("collider_ID");
	entry(make_event("ColliderSet", set_B));

	//提交位移事件（向左推进四个单位）
	nlohmann::json displacement = collider_payload("无响应空间", *collider_B);
	displacement["displacement"] = nlohmann::json::array({ -4.0, 0.0, 0.0 });
	entry(make_event("ColliderDisplacement", displacement));

	//激活碰撞空间
	ASSERT_TRUE(proxy.region_state_set("无响应空间", true));

	//检测：两个碰撞体各发布两次碰撞事件（首次与重发）且不崩溃
	sent.clear();
	ASSERT_TRUE(proxy.region_detect("无响应空间"));
	ASSERT_FALSE(sent.empty());
	//统计碰撞事件数量
	int collision_events = 0;
	for (const std::shared_ptr<engine::event>& evt : sent)
	{
		//累计碰撞事件
		if (evt->tag == "ColliderCollision")
			++collision_events;
	}
	EXPECT_EQ(collision_events, 4);
	//检测结果仍应发布（碰撞体位移未被改动，保持原有推进）
	EXPECT_EQ(sent.back()->tag, "DetectResult");
	EXPECT_EQ(sent.back()->config["results"].size(), 1u);
}

//碰撞代理器：碰撞体跨越空间边界时发布跨越通知事件
TEST_F(Collision_Proxy_Test, 跨越通知事件发布)
{
	//已发送事件集合
	std::vector<std::shared_ptr<engine::event>> sent;
	//接入入口接到的接收通道
	std::function<void(std::shared_ptr<engine::event>)> entry;

	//注册发送通道与接入入口
	proxy.event_terminal->event_sender_register(
		[&sent](std::shared_ptr<engine::event> evt) { sent.push_back(evt); });
	proxy.event_terminal->attach_handler_register(
		[&entry](auto&&, auto&&, auto&& event_entry) { entry = event_entry; });
	proxy.attach();
	ASSERT_TRUE(static_cast<bool>(entry));

	//写入临时立方体网格与边界配置
	const std::string mesh_name = temp_mesh_write();
	const std::string boundary_name = temp_boundary_write("跨越空间", mesh_name);

	//经事件构建空间与碰撞体
	nlohmann::json region_payload = nlohmann::json::object();
	region_payload["region"] = "跨越空间";
	entry(make_event("RegionBuild", region_payload));
	entry(make_event("ColliderBuild", region_payload));
	ASSERT_EQ(sent.size(), 1u);
	const uint64_t collider_ID = sent[0]->config["collider_ID"].get<uint64_t>();

	//经事件设置几何体（半长零点三的盒体，位于边界内部）
	nlohmann::json set_inside = collider_payload("跨越空间", collider_ID);
	set_inside["geometry"] = box_config(collider_ID, 0.0);
	set_inside["geometry"]["half_extent"] = nlohmann::json::array({ 0.3, 0.3, 0.3 });
	set_inside["geometry"].erase("collider_ID");
	entry(make_event("ColliderSet", set_inside));

	//经事件设置空间边界
	nlohmann::json boundary_payload = nlohmann::json::object();
	boundary_payload["path"] = boundary_name;
	entry(make_event("RegionBoundary", boundary_payload));

	//经事件激活空间
	nlohmann::json state_payload = region_payload;
	state_payload["active"] = true;
	entry(make_event("RegionState", state_payload));

	//首次检测仅建立基准，不发布跨越通知
	sent.clear();
	entry(make_event("RegionDetect", region_payload));
	ASSERT_EQ(sent.size(), 1u);
	EXPECT_EQ(sent[0]->tag, "DetectResult");

	//经事件把碰撞体移出边界
	nlohmann::json set_outside = collider_payload("跨越空间", collider_ID);
	set_outside["geometry"] = box_config(collider_ID, 5.0);
	set_outside["geometry"]["half_extent"] = nlohmann::json::array({ 0.3, 0.3, 0.3 });
	set_outside["geometry"].erase("collider_ID");
	entry(make_event("ColliderSet", set_outside));

	//再检测应先行发布"完全超出跨越"通知，随后发布检测结果
	sent.clear();
	entry(make_event("RegionDetect", region_payload));
	ASSERT_GE(sent.size(), 2u);
	EXPECT_EQ(sent.front()->tag, "RegionCrossNotice");
	EXPECT_EQ(sent.front()->config["region"], "跨越空间");
	EXPECT_EQ(sent.front()->config["collider_ID"].get<uint64_t>(), collider_ID);
	EXPECT_EQ(sent.front()->config["state"], "exit");
	EXPECT_EQ(sent.back()->tag, "DetectResult");

	//清理临时网格与边界配置文件
	std::error_code ec;
	std::filesystem::remove(
		engine::Engine_Env::exe_dir_get() / engine::string_to_path(mesh_name), ec);
	std::filesystem::remove(
		engine::Engine_Env::exe_dir_get() / engine::string_to_path(boundary_name), ec);
}
