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
	const std::shared_ptr<engine::Event> result = sent.back();
	EXPECT_EQ(result->tag, "DetectResult");
	ASSERT_EQ(result->config["results"].size(), 1u);
	EXPECT_EQ(result->config["results"][0]["collider_A"].get<uint64_t>(), collider_A);
	EXPECT_EQ(result->config["results"][0]["collider_B"].get<uint64_t>(), collider_B);
}

//碰撞代理器：同一位移事件在连续检测中持续生效（位移不再一次性清零）
TEST_F(Collision_Proxy_Test, 位移事件持续生效)
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
	entry(make_event("RegionDetect", region_payload));
	ASSERT_FALSE(sent.empty());
	const std::shared_ptr<engine::Event> result = sent.back();
	ASSERT_EQ(result->tag, "DetectResult");
	ASSERT_EQ(result->config["results"].size(), 1u);
	EXPECT_EQ(result->config["results"][0]["collider_A"].get<uint64_t>(), collider_A);
	EXPECT_EQ(result->config["results"][0]["collider_B"].get<uint64_t>(), collider_B);
}

//碰撞空间：位移作废后不再施加位移，位移改写解除作废后按新向量运动