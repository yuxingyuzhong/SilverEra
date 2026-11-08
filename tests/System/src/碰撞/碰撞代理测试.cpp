/*
碰撞代理测试（空间与碰撞体构建 / 事件接入与驱动 / 几何集合经事件生效）
（第五十九轮由「碰撞测试.cpp」按夹具拆分；逻辑未改）
*/
#include "碰撞测试辅助.h"

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

//碰撞代理器：事件接入时登记模块名与权限密钥
TEST_F(Collision_Proxy_Test, 事件接入登记权限密钥)
{
	//中转站接入入口接到的模块名
	std::string attached_name;
	//中转站接入入口接到的权限密钥
	uint64_t attached_key = 0;

	//注册中转站接入入口（新签名：模块名 + 权限密钥 + 接收器）
	proxy.event_terminal->bind(engine::Port_ID::Broker_Attacher,
		[&](auto&& module_name, auto&& acl_key, auto&& event_entry)
		{
			//留存模块名
			attached_name = module_name;
			//留存权限密钥
			attached_key = acl_key;
		});

	//接入事件中转站
	proxy.attach();

	//模块名应为碰撞代理器
	EXPECT_EQ(attached_name, "Collision_Proxy");
	//接入时应携带非空权限密钥
	EXPECT_NE(attached_key, 0u);
}

//碰撞代理器：事件驱动构建空间与碰撞体，并回告编号
TEST_F(Collision_Proxy_Test, 事件驱动碰撞体构建)
{
	//已发送事件集合
	std::vector<std::shared_ptr<engine::Event>> sent;
	//接入入口接到的接收通道
	std::function<void(std::shared_ptr<engine::Event>)> entry;

	//注册发送通道与接入入口
	proxy.event_terminal->bind(engine::Port_ID::Evt_Sender,
		[&sent](std::shared_ptr<engine::Event> evt) { sent.push_back(evt); });
	proxy.event_terminal->bind(engine::Port_ID::Broker_Attacher,
		[&entry](auto&&, auto&&, auto&& event_entry) { entry = event_entry; });
	proxy.attach();
	ASSERT_TRUE(static_cast<bool>(entry));

	//排除接入时发出的订阅登记事件，仅统计接入后的业务事件
	sent.clear();

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
	std::vector<std::shared_ptr<engine::Event>> sent;
	//接入入口接到的接收通道
	std::function<void(std::shared_ptr<engine::Event>)> entry;

	//注册发送通道与接入入口
	proxy.event_terminal->bind(engine::Port_ID::Evt_Sender,
		[&sent](std::shared_ptr<engine::Event> evt) { sent.push_back(evt); });
	proxy.event_terminal->bind(engine::Port_ID::Broker_Attacher,
		[&entry](auto&&, auto&&, auto&& event_entry) { entry = event_entry; });
	proxy.attach();
	ASSERT_TRUE(static_cast<bool>(entry));

	//排除接入时发出的订阅登记事件，仅统计接入后的业务事件
	sent.clear();

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
	ASSERT_TRUE(proxy.region_detect(region_payload["region"].get<std::string>()).has_value());
	ASSERT_EQ(sent.size(), 1u);
	EXPECT_EQ(sent[0]->tag, "DetectResult");
	EXPECT_EQ(sent[0]->config["region"], "事件空间");
	ASSERT_TRUE(sent[0]->config["results"].is_array());
	EXPECT_TRUE(sent[0]->config["results"].empty());

	//经运动事件把第二个盒体移入重叠位置
	sent.clear();
	nlohmann::json move_B = collider_payload("事件空间", collider_B);
	move_B["motion"] = nlohmann::json::object();
	move_B["motion"]["translation"] = nlohmann::json::array({ -4.0, 0.0, 0.0 });
	entry(make_event("ColliderMotion", move_B));

	//公共 API 同步返回碰撞详情（含接触点）
	std::optional<std::vector<engine::Collision_Detail>> detected =
		proxy.region_detect("事件空间");
	ASSERT_TRUE(detected.has_value());
	ASSERT_EQ(detected->size(), 1u);
	EXPECT_EQ((*detected)[0].collider_A, collider_A);
	EXPECT_EQ((*detected)[0].collider_B, collider_B);
	ASSERT_FALSE((*detected)[0].contacts.empty());

	//检测结果应报告一对碰撞
	ASSERT_FALSE(sent.empty());
	const std::shared_ptr<engine::Event> result = sent.back();
	EXPECT_EQ(result->tag, "DetectResult");
	ASSERT_EQ(result->config["results"].size(), 1u);
	EXPECT_EQ(result->config["results"][0]["collider_A"].get<uint64_t>(), collider_A);
	EXPECT_EQ(result->config["results"][0]["collider_B"].get<uint64_t>(), collider_B);
}

//碰撞代理器：同一运动事件在连续检测中持续生效（运动不再一次性清零）
TEST_F(Collision_Proxy_Test, 运动事件持续生效)
{
	//已发送事件集合
	std::vector<std::shared_ptr<engine::Event>> sent;
	//接入入口接到的接收通道
	std::function<void(std::shared_ptr<engine::Event>)> entry;

	//注册发送通道与接入入口
	proxy.event_terminal->bind(engine::Port_ID::Evt_Sender,
		[&sent](std::shared_ptr<engine::Event> evt) { sent.push_back(evt); });
	proxy.event_terminal->bind(engine::Port_ID::Broker_Attacher,
		[&entry](auto&&, auto&&, auto&& event_entry) { entry = event_entry; });
	proxy.attach();
	ASSERT_TRUE(static_cast<bool>(entry));

	//构建空间与两个盒体
	ASSERT_TRUE(proxy.region_build("运动空间"));
	std::optional<uint64_t> collider_A = proxy.collider_build("运动空间");
	std::optional<uint64_t> collider_B = proxy.collider_build("运动空间");
	ASSERT_TRUE(collider_A.has_value());
	ASSERT_TRUE(collider_B.has_value());

	//经事件设置两个盒体的几何体（相距五个单位）
	nlohmann::json set_A = collider_payload("运动空间", *collider_A);
	set_A["geometry"] = box_config(*collider_A, 0.0);
	set_A["geometry"].erase("collider_ID");
	entry(make_event("ColliderSet", set_A));

	nlohmann::json set_B = collider_payload("运动空间", *collider_B);
	set_B["geometry"] = box_config(*collider_B, 5.0);
	set_B["geometry"].erase("collider_ID");
	entry(make_event("ColliderSet", set_B));

	//经事件为第二个盒体提交一次运动（持续向左推进四个单位）
	nlohmann::json motion = collider_payload("运动空间", *collider_B);
	motion["motion"] = nlohmann::json::object();
	motion["motion"]["translation"] = nlohmann::json::array({ -4.0, 0.0, 0.0 });
	entry(make_event("ColliderMotion", motion));

	//激活碰撞空间
	ASSERT_TRUE(proxy.region_state_set("运动空间", true));

	//第一次检测：运动生效使两盒体重叠，报告一对碰撞
	sent.clear();
	ASSERT_TRUE(proxy.region_detect("运动空间").has_value());
	ASSERT_FALSE(sent.empty());
	ASSERT_EQ(sent.back()->config["results"].size(), 1u);

	//第二次检测：运动持续生效使盒体继续推进至远离位置，不再报告碰撞
	sent.clear();
	ASSERT_TRUE(proxy.region_detect("运动空间").has_value());
	ASSERT_FALSE(sent.empty());
	EXPECT_TRUE(sent.back()->config["results"].empty());
}

//碰撞代理器：几何体集合经事件通道设置并参与检测
TEST_F(Collision_Proxy_Test, 几何体集合经事件设置生效)
{
	//已发送事件集合
	std::vector<std::shared_ptr<engine::Event>> sent;
	//接入入口接到的接收通道
	std::function<void(std::shared_ptr<engine::Event>)> entry;

	//注册发送通道与接入入口
	proxy.event_terminal->bind(engine::Port_ID::Evt_Sender,
		[&sent](std::shared_ptr<engine::Event> evt) { sent.push_back(evt); });
	proxy.event_terminal->bind(engine::Port_ID::Broker_Attacher,
		[&entry](auto&&, auto&&, auto&& event_entry) { entry = event_entry; });
	proxy.attach();
	ASSERT_TRUE(static_cast<bool>(entry));

	//排除接入时发出的订阅登记事件，仅统计接入后的业务事件
	sent.clear();

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
	ASSERT_TRUE(proxy.region_detect(region_payload["region"].get<std::string>()).has_value());
	ASSERT_FALSE(sent.empty());
	const std::shared_ptr<engine::Event> result = sent.back();
	ASSERT_EQ(result->tag, "DetectResult");
	ASSERT_EQ(result->config["results"].size(), 1u);
	EXPECT_EQ(result->config["results"][0]["collider_A"].get<uint64_t>(), collider_A);
	EXPECT_EQ(result->config["results"][0]["collider_B"].get<uint64_t>(), collider_B);
}

//碰撞代理器：简略结果事件开关可经公共 API 与事件设置
TEST_F(Collision_Proxy_Test, 简略结果事件开关)
{
	//已发送事件集合
	std::vector<std::shared_ptr<engine::Event>> sent;
	//接入入口接到的接收通道
	std::function<void(std::shared_ptr<engine::Event>)> entry;

	//注册发送通道与接入入口
	proxy.event_terminal->bind(engine::Port_ID::Evt_Sender,
		[&sent](std::shared_ptr<engine::Event> evt) { sent.push_back(evt); });
	proxy.event_terminal->bind(engine::Port_ID::Broker_Attacher,
		[&entry](auto&&, auto&&, auto&& event_entry) { entry = event_entry; });
	proxy.attach();
	ASSERT_TRUE(static_cast<bool>(entry));

	//构建空间并激活（无碰撞体，检测结果为空）
	ASSERT_TRUE(proxy.region_build("开关空间"));
	ASSERT_TRUE(proxy.region_state_set("开关空间", true));
	//排除接入时发出的订阅登记事件
	sent.clear();

	//默认开启：检测后应发布简略结果事件
	ASSERT_TRUE(proxy.region_detect("开关空间").has_value());
	ASSERT_FALSE(sent.empty());
	EXPECT_EQ(sent.back()->tag, "DetectResult");

	//经公共 API 关闭
	sent.clear();
	ASSERT_TRUE(proxy.region_event_output_set("开关空间", false));
	EXPECT_FALSE(proxy.region_event_output_get("开关空间"));
	ASSERT_TRUE(proxy.region_detect("开关空间").has_value());
	EXPECT_TRUE(sent.empty());

	//经事件重新开启
	nlohmann::json output_payload = nlohmann::json::object();
	output_payload["region"] = "开关空间";
	output_payload["enabled"] = true;
	entry(make_event("RegionEventOutput", output_payload));
	EXPECT_TRUE(proxy.region_event_output_get("开关空间"));
	sent.clear();
	ASSERT_TRUE(proxy.region_detect("开关空间").has_value());
	ASSERT_FALSE(sent.empty());
	EXPECT_EQ(sent.back()->tag, "DetectResult");

	//未构建空间无法设置开关
	EXPECT_FALSE(proxy.region_event_output_set("未构建空间", false));
}

//碰撞代理器：含旋转的运动事件缺少离散步长时被拒绝，补齐步长后生效
TEST_F(Collision_Proxy_Test, 旋转运动缺步数被拒绝)
{
	//已发送事件集合
	std::vector<std::shared_ptr<engine::Event>> sent;
	//接入入口接到的接收通道
	std::function<void(std::shared_ptr<engine::Event>)> entry;

	//注册发送通道与接入入口
	proxy.event_terminal->bind(engine::Port_ID::Evt_Sender,
		[&sent](std::shared_ptr<engine::Event> evt) { sent.push_back(evt); });
	proxy.event_terminal->bind(engine::Port_ID::Broker_Attacher,
		[&entry](auto&&, auto&&, auto&& event_entry) { entry = event_entry; });
	proxy.attach();
	ASSERT_TRUE(static_cast<bool>(entry));

	//排除接入时发出的订阅登记事件
	sent.clear();

	//空间与两个盒体
	nlohmann::json region_payload = nlohmann::json::object();
	region_payload["region"] = "旋转空间";
	entry(make_event("RegionBuild", region_payload));
	entry(make_event("ColliderBuild", region_payload));
	entry(make_event("ColliderBuild", region_payload));
	ASSERT_EQ(sent.size(), 2u);
	const uint64_t collider_A = sent[0]->config["collider_ID"].get<uint64_t>();
	const uint64_t collider_B = sent[1]->config["collider_ID"].get<uint64_t>();

	//两个盒体分别位于零与五个单位处
	nlohmann::json set_A = collider_payload("旋转空间", collider_A);
	set_A["geometry"] = box_config(collider_A, 0.0);
	set_A["geometry"].erase("collider_ID");
	entry(make_event("ColliderSet", set_A));

	nlohmann::json set_B = collider_payload("旋转空间", collider_B);
	set_B["geometry"] = box_config(collider_B, 5.0);
	set_B["geometry"].erase("collider_ID");
	entry(make_event("ColliderSet", set_B));

	//激活空间
	nlohmann::json state_payload = region_payload;
	state_payload["active"] = true;
	entry(make_event("RegionState", state_payload));

	//第二个盒体的含旋转运动（绕 Z 轴旋转九十度并左移四个单位）
	nlohmann::json motion = collider_payload("旋转空间", collider_B);
	motion["motion"] = nlohmann::json::object();
	motion["motion"]["translation"] = nlohmann::json::array({ -4.0, 0.0, 0.0 });
	motion["motion"]["rotation"] = nlohmann::json::array({ 0.0, 0.0, 0.70710678, 0.70710678 });

	//离散步长仍为零，事件应被整条拒绝
	entry(make_event("ColliderMotion", motion));
	sent.clear();
	ASSERT_TRUE(proxy.region_detect("旋转空间").has_value());
	ASSERT_FALSE(sent.empty());
	EXPECT_EQ(sent.back()->tag, "DetectResult");
	EXPECT_TRUE(sent.back()->config["results"].empty());

	//补齐离散步长
	nlohmann::json set_mode = collider_payload("旋转空间", collider_B);
	set_mode["detection_mode"] = nlohmann::json::object();
	set_mode["detection_mode"]["is_swept_volume"] = false;
	set_mode["detection_mode"]["step_length"] = 2u;
	entry(make_event("ColliderSet", set_mode));

	//补齐步长后同一运动事件被接受
	entry(make_event("ColliderMotion", motion));
	sent.clear();
	ASSERT_TRUE(proxy.region_detect("旋转空间").has_value());
	ASSERT_FALSE(sent.empty());
	ASSERT_EQ(sent.back()->config["results"].size(), 1u);
	EXPECT_EQ(sent.back()->config["results"][0]["collider_A"].get<uint64_t>(), collider_A);
	EXPECT_EQ(sent.back()->config["results"][0]["collider_B"].get<uint64_t>(), collider_B);

	//删除响应协议后不再出现裁决事件
	for (const std::shared_ptr<engine::Event>& evt : sent)
		EXPECT_NE(evt->tag, "ColliderCollision");
}

//碰撞代理器：部件运动下标校验、单件生效与几何重建清空
TEST_F(Collision_Proxy_Test, 部件运动与几何重建清空)
{
	//已发送事件集合
	std::vector<std::shared_ptr<engine::Event>> sent;
	//接入入口接到的接收通道
	std::function<void(std::shared_ptr<engine::Event>)> entry;

	//注册发送通道与接入入口
	proxy.event_terminal->bind(engine::Port_ID::Evt_Sender,
		[&sent](std::shared_ptr<engine::Event> evt) { sent.push_back(evt); });
	proxy.event_terminal->bind(engine::Port_ID::Broker_Attacher,
		[&entry](auto&&, auto&&, auto&& event_entry) { entry = event_entry; });
	proxy.attach();
	ASSERT_TRUE(static_cast<bool>(entry));

	//排除接入时发出的订阅登记事件
	sent.clear();

	//空间与两个碰撞体
	nlohmann::json region_payload = nlohmann::json::object();
	region_payload["region"] = "部件空间";
	entry(make_event("RegionBuild", region_payload));
	entry(make_event("ColliderBuild", region_payload));
	entry(make_event("ColliderBuild", region_payload));
	ASSERT_EQ(sent.size(), 2u);
	const uint64_t collider_A = sent[0]->config["collider_ID"].get<uint64_t>();
	const uint64_t collider_B = sent[1]->config["collider_ID"].get<uint64_t>();

	//碰撞体A为双部件（两个盒体分别位于正负五个单位处）
	nlohmann::json set_A = collider_payload("部件空间", collider_A);
	set_A["geometry"] = nlohmann::json::array();
	nlohmann::json part_left = nlohmann::json::object();
	part_left["type"] = "box";
	part_left["half_extent"] = nlohmann::json::array({ 1.0, 1.0, 1.0 });
	part_left["position"] = nlohmann::json::array({ -5.0, 0.0, 0.0 });
	set_A["geometry"].push_back(part_left);
	nlohmann::json part_right = nlohmann::json::object();
	part_right["type"] = "box";
	part_right["half_extent"] = nlohmann::json::array({ 1.0, 1.0, 1.0 });
	part_right["position"] = nlohmann::json::array({ 5.0, 0.0, 0.0 });
	set_A["geometry"].push_back(part_right);
	entry(make_event("ColliderSet", set_A));

	//碰撞体B为单体盒体，与本体的左侧部件重叠
	nlohmann::json set_B = collider_payload("部件空间", collider_B);
	set_B["geometry"] = box_config(collider_B, -5.5);
	set_B["geometry"].erase("collider_ID");
	entry(make_event("ColliderSet", set_B));

	//碰撞体A启用离散检测，步长一（单段跳到终态，避免途中掠过目标）
	nlohmann::json set_mode = collider_payload("部件空间", collider_A);
	set_mode["detection_mode"] = nlohmann::json::object();
	set_mode["detection_mode"]["is_swept_volume"] = false;
	set_mode["detection_mode"]["step_length"] = 1u;
	entry(make_event("ColliderSet", set_mode));

	//激活空间
	nlohmann::json state_payload = region_payload;
	state_payload["active"] = true;
	entry(make_event("RegionState", state_payload));

	//基准检测报告一对碰撞（左侧部件与B重叠）
	sent.clear();
	ASSERT_TRUE(proxy.region_detect("部件空间").has_value());
	ASSERT_FALSE(sent.empty());
	ASSERT_EQ(sent.back()->config["results"].size(), 1u);
	//结果条目必须携带子部件编号（两侧自动分配的首个编号均为零）
	EXPECT_EQ(sent.back()->config["results"][0]["part_A"].get<uint16_t>(), 0u);
	EXPECT_EQ(sent.back()->config["results"][0]["part_B"].get<uint16_t>(), 0u);

	//越界部件下标的运动事件被整条拒绝
	nlohmann::json bad_parts = collider_payload("部件空间", collider_A);
	bad_parts["parts"] = nlohmann::json::array();
	nlohmann::json bad_item = nlohmann::json::object();
	bad_item["part"] = 2u;
	bad_item["translation"] = nlohmann::json::array({ 0.0, 3.0, 0.0 });
	bad_parts["parts"].push_back(bad_item);
	entry(make_event("ColliderMotion", bad_parts));
	sent.clear();
	ASSERT_TRUE(proxy.region_detect("部件空间").has_value());
	ASSERT_FALSE(sent.empty());
	ASSERT_EQ(sent.back()->config["results"].size(), 1u);

	//合法部件运动把左侧部件推离B
	nlohmann::json parts_motion = collider_payload("部件空间", collider_A);
	parts_motion["parts"] = nlohmann::json::array();
	nlohmann::json part_item = nlohmann::json::object();
	part_item["part"] = 0u;
	part_item["translation"] = nlohmann::json::array({ 0.0, 3.0, 0.0 });
	parts_motion["parts"].push_back(part_item);
	entry(make_event("ColliderMotion", parts_motion));
	sent.clear();
	ASSERT_TRUE(proxy.region_detect("部件空间").has_value());
	ASSERT_FALSE(sent.empty());
	EXPECT_TRUE(sent.back()->config["results"].empty());

	//同编号几何重建：子部件编号仍存在，运动登记保留，左侧部件继续被推离
	entry(make_event("ColliderSet", set_A));
	sent.clear();
	ASSERT_TRUE(proxy.region_detect("部件空间").has_value());
	ASSERT_FALSE(sent.empty());
	EXPECT_TRUE(sent.back()->config["results"].empty());

	//改用新编号重建几何：旧编号的运动登记被丢弃，左侧部件回到原位并再次重叠
	nlohmann::json set_A_new = set_A;
	set_A_new["geometry"][0]["part_ID"] = 3u;
	set_A_new["geometry"][1]["part_ID"] = 7u;
	entry(make_event("ColliderSet", set_A_new));
	sent.clear();
	ASSERT_TRUE(proxy.region_detect("部件空间").has_value());
	ASSERT_FALSE(sent.empty());
	ASSERT_EQ(sent.back()->config["results"].size(), 1u);
	//新编号重建后的碰撞对为 A 的编号三部件与 B 的编号零部件
	EXPECT_EQ(sent.back()->config["results"][0]["part_A"].get<uint16_t>(), 3u);
	EXPECT_EQ(sent.back()->config["results"][0]["part_B"].get<uint16_t>(), 0u);
}

//碰撞代理器：离散步长归零时清除含旋转的运动登记
TEST_F(Collision_Proxy_Test, 离散步长归零清除旋转登记)
{
	//已发送事件集合
	std::vector<std::shared_ptr<engine::Event>> sent;
	//接入入口接到的接收通道
	std::function<void(std::shared_ptr<engine::Event>)> entry;

	//注册发送通道与接入入口
	proxy.event_terminal->bind(engine::Port_ID::Evt_Sender,
		[&sent](std::shared_ptr<engine::Event> evt) { sent.push_back(evt); });
	proxy.event_terminal->bind(engine::Port_ID::Broker_Attacher,
		[&entry](auto&&, auto&&, auto&& event_entry) { entry = event_entry; });
	proxy.attach();
	ASSERT_TRUE(static_cast<bool>(entry));

	//排除接入时发出的订阅登记事件
	sent.clear();

	//空间与两个碰撞体
	nlohmann::json region_payload = nlohmann::json::object();
	region_payload["region"] = "清零空间";
	entry(make_event("RegionBuild", region_payload));
	entry(make_event("ColliderBuild", region_payload));
	entry(make_event("ColliderBuild", region_payload));
	ASSERT_EQ(sent.size(), 2u);
	const uint64_t collider_A = sent[0]->config["collider_ID"].get<uint64_t>();
	const uint64_t collider_B = sent[1]->config["collider_ID"].get<uint64_t>();

	//细长盒体（长轴沿 X）
	nlohmann::json set_A = collider_payload("清零空间", collider_A);
	set_A["geometry"] = nlohmann::json::object();
	set_A["geometry"]["type"] = "box";
	set_A["geometry"]["half_extent"] = nlohmann::json::array({ 3.0, 0.5, 0.5 });
	set_A["geometry"]["position"] = nlohmann::json::array({ 0.0, 0.0, 0.0 });
	entry(make_event("ColliderSet", set_A));

	//目标盒体位于短轴正上方
	nlohmann::json set_B = collider_payload("清零空间", collider_B);
	set_B["geometry"] = nlohmann::json::object();
	set_B["geometry"]["type"] = "box";
	set_B["geometry"]["half_extent"] = nlohmann::json::array({ 0.3, 0.3, 0.3 });
	set_B["geometry"]["position"] = nlohmann::json::array({ 0.0, 2.2, 0.0 });
	entry(make_event("ColliderSet", set_B));

	//细长盒体启用离散检测，步长二
	nlohmann::json set_mode = collider_payload("清零空间", collider_A);
	set_mode["detection_mode"] = nlohmann::json::object();
	set_mode["detection_mode"]["is_swept_volume"] = false;
	set_mode["detection_mode"]["step_length"] = 2u;
	entry(make_event("ColliderSet", set_mode));

	//激活空间
	nlohmann::json state_payload = region_payload;
	state_payload["active"] = true;
	entry(make_event("RegionState", state_payload));

	//旋转九十度的运动事件
	nlohmann::json motion = collider_payload("清零空间", collider_A);
	motion["motion"] = nlohmann::json::object();
	motion["motion"]["rotation"] = nlohmann::json::array({ 0.0, 0.0, 0.70710678, 0.70710678 });
	entry(make_event("ColliderMotion", motion));

	//旋转到九十度时长轴扫过目标
	sent.clear();
	ASSERT_TRUE(proxy.region_detect("清零空间").has_value());
	ASSERT_FALSE(sent.empty());
	ASSERT_EQ(sent.back()->config["results"].size(), 1u);

	//离散步长归零，含旋转的运动登记被清除
	nlohmann::json clear_mode = collider_payload("清零空间", collider_A);
	clear_mode["detection_mode"] = nlohmann::json::object();
	clear_mode["detection_mode"]["is_swept_volume"] = false;
	clear_mode["detection_mode"]["step_length"] = 0u;
	entry(make_event("ColliderSet", clear_mode));

	/*
	登记已清除，细长盒体停留在九十度姿态不再旋转；
	若登记残留则会继续旋转到一百八十度而与目标分离。
	*/
	sent.clear();
	ASSERT_TRUE(proxy.region_detect("清零空间").has_value());
	ASSERT_FALSE(sent.empty());
	ASSERT_EQ(sent.back()->config["results"].size(), 1u);
}

//碰撞代理器：跨越状态查询经事件协议返回快照
TEST_F(Collision_Proxy_Test, 跨越状态查询事件返回快照)
{
	//已发送事件集合
	std::vector<std::shared_ptr<engine::Event>> sent;
	//接入入口接到的接收通道
	std::function<void(std::shared_ptr<engine::Event>)> entry;

	//注册发送通道与接入入口
	proxy.event_terminal->bind(engine::Port_ID::Evt_Sender,
		[&sent](std::shared_ptr<engine::Event> evt) { sent.push_back(evt); });
	proxy.event_terminal->bind(engine::Port_ID::Broker_Attacher,
		[&entry](auto&&, auto&&, auto&& event_entry) { entry = event_entry; });
	proxy.attach();
	ASSERT_TRUE(static_cast<bool>(entry));
	//排除接入时发出的订阅登记事件
	sent.clear();

	//写入临时网格与边界配置
	const std::string mesh_name = temp_mesh_write();
	const std::string boundary_path = temp_boundary_write("查询空间", mesh_name);

	//构建空间与边界
	ASSERT_TRUE(proxy.region_build("查询空间"));
	ASSERT_TRUE(proxy.region_boundary_set(boundary_path));

	//经事件构建碰撞体
	nlohmann::json build_payload = nlohmann::json::object();
	build_payload["region"] = "查询空间";
	entry(make_event("ColliderBuild", build_payload));
	ASSERT_FALSE(sent.empty());
	ASSERT_EQ(sent.back()->tag, "ColliderBuildResult");
	const uint64_t collider_ID = sent.back()->config["collider_ID"].get<uint64_t>();

	//经事件设置盒体几何
	nlohmann::json set_payload = collider_payload("查询空间", collider_ID);
	set_payload["geometry"] = box_config(collider_ID, 0.0);
	set_payload["geometry"]["half_extent"] = nlohmann::json::array({ 0.3, 0.3, 0.3 });
	set_payload["geometry"].erase("collider_ID");
	entry(make_event("ColliderSet", set_payload));

	//激活空间并执行首次检测（建立基准）
	ASSERT_TRUE(proxy.region_state_set("查询空间", true));
	ASSERT_TRUE(proxy.region_detect("查询空间").has_value());

	//发送跨越状态查询（指定编号与请求编号）
	sent.clear();
	nlohmann::json query = collider_payload("查询空间", collider_ID);
	query["request_ID"] = 77u;
	entry(make_event("RegionCrossStateQuery", query));

	//查询结果事件应携带快照
	ASSERT_FALSE(sent.empty());
	const std::shared_ptr<engine::Event> result = sent.back();
	ASSERT_EQ(result->tag, "RegionCrossStateResult");
	EXPECT_TRUE(result->config["found"].get<bool>());
	EXPECT_EQ(result->config["request_ID"].get<uint64_t>(), 77u);
	ASSERT_EQ(result->config["states"].size(), 1u);
	EXPECT_EQ(result->config["states"][0]["collider_ID"].get<uint64_t>(), collider_ID);
	EXPECT_EQ(result->config["states"][0]["state"].get<std::string>(), "inside");

	//清理临时网格文件
	std::error_code ec;
	std::filesystem::remove(
		engine::detail::exe_dir_get() / engine::detail::string_to_path(mesh_name), ec);
}