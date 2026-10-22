/*
碰撞响应测试（碰撞响应的停止 / 继续 / 位移变化 / 缺失重发 / 跨越通知与级联卸载）
（第五十九轮由「碰撞测试.cpp」按功能域拆分；逻辑未改）
*/
#include "碰撞测试辅助.h"

TEST_F(Collision_Proxy_Test, 碰撞响应停止运动作废位移)
{
	//已发送事件集合
	std::vector<std::shared_ptr<engine::Event>> sent;
	//接入入口接到的接收通道
	std::function<void(std::shared_ptr<engine::Event>)> entry;

	//注册发送通道：收到碰撞事件时立即按编号回发响应（模拟外部同步应答）
	proxy.event_terminal->bind(engine::Port_ID::Evt_Sender,
		[&sent, &entry](std::shared_ptr<engine::Event> evt)
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
			entry(std::make_shared<engine::Event>(
				"", "Collision_Proxy", "Collision", "ColliderCollisionResponse", response));
		});
	proxy.event_terminal->bind(engine::Port_ID::Broker_Attacher,
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
	const std::shared_ptr<engine::Event> first = sent.back();
	ASSERT_EQ(first->tag, "DetectResult");
	ASSERT_EQ(first->config["results"].size(), 1u);

	//第二次检测：位移已作废，盒体不再推进，仍停留在重叠位置
	sent.clear();
	ASSERT_TRUE(proxy.region_detect("响应空间"));
	ASSERT_FALSE(sent.empty());
	const std::shared_ptr<engine::Event> second = sent.back();
	ASSERT_EQ(second->tag, "DetectResult");
	EXPECT_EQ(second->config["results"].size(), 1u);
}

//碰撞代理器：碰撞响应为继续运动且位移保持时，碰撞体继续推进
TEST_F(Collision_Proxy_Test, 碰撞响应继续运动保持位移)
{
	//已发送事件集合
	std::vector<std::shared_ptr<engine::Event>> sent;
	//接入入口接到的接收通道
	std::function<void(std::shared_ptr<engine::Event>)> entry;

	//注册发送通道：收到碰撞事件时立即回发"保持位移"响应
	proxy.event_terminal->bind(engine::Port_ID::Evt_Sender,
		[&sent, &entry](std::shared_ptr<engine::Event> evt)
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
			entry(std::make_shared<engine::Event>(
				"", "Collision_Proxy", "Collision", "ColliderCollisionResponse", response));
		});
	proxy.event_terminal->bind(engine::Port_ID::Broker_Attacher,
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
	std::vector<std::shared_ptr<engine::Event>> sent;
	//接入入口接到的接收通道
	std::function<void(std::shared_ptr<engine::Event>)> entry;
	//被改写位移的碰撞体编号
	uint64_t changed_ID = 0;

	//注册发送通道：对指定碰撞体回发"位移变化"响应，其余回发"保持位移"
	proxy.event_terminal->bind(engine::Port_ID::Evt_Sender,
		[&sent, &entry, &changed_ID](std::shared_ptr<engine::Event> evt)
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
			entry(std::make_shared<engine::Event>(
				"", "Collision_Proxy", "Collision", "ColliderCollisionResponse", response));
		});
	proxy.event_terminal->bind(engine::Port_ID::Broker_Attacher,
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
	std::vector<std::shared_ptr<engine::Event>> sent;
	//接入入口接到的接收通道
	std::function<void(std::shared_ptr<engine::Event>)> entry;
	//各碰撞体收到的碰撞事件次数
	std::unordered_map<uint64_t, int> collision_times;
	//被重发后命中的碰撞体编号
	uint64_t replied_ID = 0;

	//注册发送通道：首个碰撞事件不回发响应，重发时才回发"停止运动"响应
	proxy.event_terminal->bind(engine::Port_ID::Evt_Sender,
		[&sent, &entry, &collision_times, &replied_ID](std::shared_ptr<engine::Event> evt)
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
			entry(std::make_shared<engine::Event>(
				"", "Collision_Proxy", "Collision", "ColliderCollisionResponse", response));
		});
	proxy.event_terminal->bind(engine::Port_ID::Broker_Attacher,
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
	std::vector<std::shared_ptr<engine::Event>> sent;
	//接入入口接到的接收通道
	std::function<void(std::shared_ptr<engine::Event>)> entry;

	//注册发送通道：只记录不回发响应
	proxy.event_terminal->bind(engine::Port_ID::Evt_Sender,
		[&sent](std::shared_ptr<engine::Event> evt) { sent.push_back(evt); });
	proxy.event_terminal->bind(engine::Port_ID::Broker_Attacher,
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
	for (const std::shared_ptr<engine::Event>& evt : sent)
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
		engine::Engine_Env::exe_dir_get() / engine::detail::string_to_path(mesh_name), ec);
	std::filesystem::remove(
		engine::Engine_Env::exe_dir_get() / engine::detail::string_to_path(boundary_name), ec);
}

//碰撞代理器：空名称空间的构建与卸载全程不崩溃
TEST_F(Collision_Proxy_Test, 空名称空间构建与卸载不崩溃)
{
	//空名称空间构建不应崩溃
	bool 构建结果 = false;
	EXPECT_NO_THROW(构建结果 = proxy.region_build(""));
	//空名称可正常登记
	EXPECT_TRUE(构建结果);
	//空名称空间默认未激活，检测不执行
	EXPECT_FALSE(proxy.region_detect(""));
	//空名称空间可设置活跃性
	EXPECT_TRUE(proxy.region_state_set("", true));
	//空名称空间卸载不应崩溃
	bool 卸载结果 = false;
	EXPECT_NO_THROW(卸载结果 = proxy.region_unload(""));
	//空名称可正常卸载
	EXPECT_TRUE(卸载结果);
}

//碰撞代理器：卸载空间时级联注销其全部碰撞体编号
TEST_F(Collision_Proxy_Test, 空间卸载级联注销碰撞体)
{
	//构建空间与两个碰撞体
	ASSERT_TRUE(proxy.region_build("级联空间"));
	std::optional<uint64_t> collider_A = proxy.collider_build("级联空间");
	std::optional<uint64_t> collider_B = proxy.collider_build("级联空间");
	ASSERT_TRUE(collider_A.has_value());
	ASSERT_TRUE(collider_B.has_value());

	//卸载空间应级联注销所属碰撞体
	ASSERT_TRUE(proxy.region_unload("级联空间"));
	//空间已不存在，剩余编号的设置失败
	engine::Detection_Mode detection_mode;
	EXPECT_FALSE(proxy.collider_set(*collider_A, detection_mode));
	//空间已不存在，剩余编号的卸载失败
	EXPECT_FALSE(proxy.collider_unload(*collider_B, "级联空间"));
}

//碰撞代理器：碰撞体重复卸载失败且卸载后编号设置失败
TEST_F(Collision_Proxy_Test, 碰撞体重复卸载后设置失败)
{
	//构建空间与碰撞体
	ASSERT_TRUE(proxy.region_build("重复卸载空间"));
	std::optional<uint64_t> collider_ID = proxy.collider_build("重复卸载空间");
	ASSERT_TRUE(collider_ID.has_value());

	//首次卸载成功
	EXPECT_TRUE(proxy.collider_unload(*collider_ID, "重复卸载空间"));
	//重复卸载同一碰撞体失败
	EXPECT_FALSE(proxy.collider_unload(*collider_ID, "重复卸载空间"));
	//已注销编号的豁免标记设置失败
	EXPECT_FALSE(proxy.collider_set(*collider_ID, static_cast<uint64_t>(1)));
	//归属空间不存在时卸载失败
	EXPECT_FALSE(proxy.collider_unload(*collider_ID, "未构建空间"));
}

//碰撞代理器：未构建空间时设置碰撞体失败且不崩溃
TEST_F(Collision_Proxy_Test, 未构建空间时设置碰撞体不崩溃)
{
	//检测方式参数
	engine::Detection_Mode detection_mode;
	detection_mode.is_swept_volume = true;
	detection_mode.step_length = 8;

	//未登记编号的检测方式设置不崩溃
	EXPECT_NO_THROW(proxy.collider_set(9999u, detection_mode));
	//未登记编号的检测方式设置失败
	EXPECT_FALSE(proxy.collider_set(9999u, detection_mode));
	//未登记编号的豁免标记设置失败
	EXPECT_FALSE(proxy.collider_set(9999u, static_cast<uint64_t>(2)));
	//未构建空间无法构建碰撞体
	EXPECT_FALSE(proxy.collider_build("未构建空间").has_value());
	//未构建空间的检测与状态设置均失败
	EXPECT_FALSE(proxy.region_detect("未构建空间"));
	EXPECT_FALSE(proxy.region_state_set("未构建空间", true));
}

//碰撞代理器：经事件下发的空几何配置被忽略，碰撞体不进入碰撞世界
TEST_F(Collision_Proxy_Test, 空几何配置经事件被忽略)
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
	region_payload["region"] = "空几何空间";

	//经事件构建空间与两个碰撞体
	entry(make_event("RegionBuild", region_payload));
	entry(make_event("ColliderBuild", region_payload));
	entry(make_event("ColliderBuild", region_payload));
	ASSERT_EQ(sent.size(), 2u);
	const uint64_t collider_A = sent[0]->config["collider_ID"].get<uint64_t>();
	const uint64_t collider_B = sent[1]->config["collider_ID"].get<uint64_t>();

	//经事件把碰撞体A设置为空几何配置（应被忽略）
	nlohmann::json set_A = collider_payload("空几何空间", collider_A);
	set_A["geometry"] = nlohmann::json::object();
	entry(make_event("ColliderSet", set_A));

	//经事件把碰撞体B设置为位于原点的合法盒体
	nlohmann::json set_B = collider_payload("空几何空间", collider_B);
	set_B["geometry"] = box_config(collider_B, 0.0);
	set_B["geometry"].erase("collider_ID");
	entry(make_event("ColliderSet", set_B));

	//经事件激活空间
	nlohmann::json state_payload = region_payload;
	state_payload["active"] = true;
	entry(make_event("RegionState", state_payload));

	//检测：A 未进入碰撞世界，仅有 B 参与，无碰撞对
	sent.clear();
	entry(make_event("RegionDetect", region_payload));
	ASSERT_FALSE(sent.empty());
	ASSERT_EQ(sent.back()->tag, "DetectResult");
	EXPECT_TRUE(sent.back()->config["results"].empty());
}

//碰撞空间：形状字段类型非法与缺字段一律被拒