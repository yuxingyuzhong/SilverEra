//配置加载器测试：覆盖可信根目录的建立、卸载、增量重建与只读缓存加载链路
#include <gtest/gtest.h>

//获取配置加载器
#include "Engine/EngineCore/src/core/config/Config_Loader/配置加载器.h"
//获取事件中转器
#include "Engine/EngineCore/src/core/event/Event_Broker/事件中转器.h"
//获取路径字符串转换工具
#include "Engine/EngineCore/src/tools/Detail/路径字符串转换.h"

//配置加载器测试夹具
class Config_Loader_Test : public ::testing::Test
{
protected:
	//被测对象名
	static constexpr const char* object_name = "测试对象";

	//事件中转器
	engine::Event_Broker broker{};
	//被测配置加载器
	engine::Config_Loader loader{};
	//投递到的配置事件
	std::vector<std::shared_ptr<engine::Event>> delivered{};

	//条目自定义事件类别（为空表示条目不写该字段）
	std::string entry_category{};
	//条目自定义事件标签（为空表示条目不写该字段）
	std::string entry_tag{};
	//订阅者订阅的事件类别（缺省为 Config/Load）
	std::string subscribe_category = "Config";
	//订阅者订阅的事件标签（缺省为 Config/Load）
	std::string subscribe_tag = "Load";

	//本次用例根目录名
	std::string root_name{};
	//可信根索引目录文本
	std::string root_route{};
	//可信根配置目录文本
	std::string root_config{};
	//对象索引目录文本
	std::string object_route{};
	//对象配置目录文本
	std::string object_config{};
	//对象配置文件路径文本
	std::string config_path{};

	//测试收尾
	void TearDown(void) override
	{
		//清除本次用例产生的测试根目录
		if (!root_name.empty())
			std::filesystem::remove_all(engine::Engine_Env::exe_dir_get() /
				engine::detail::string_to_path(root_name));
	}

	//绝对路径获取
	static std::filesystem::path absolute_get(const std::string& text)
	{
		//以可执行文件目录为基准拼接
		return engine::Engine_Env::exe_dir_get() / engine::detail::string_to_path(text);
	}

	//可信根目录准备（nest 为真时把对象目录下探一层，用于区分可信根目录与对象自身目录）
	void root_make(const std::string& name, bool nest)
	{
		//记录本次根目录名
		root_name = name;
		//构造各级目录文本
		root_route = name + "/索引";
		root_config = name + "/配置";
		object_route = nest ? root_route + "/对象甲" : root_route;
		object_config = nest ? root_config + "/对象甲" : root_config;
		config_path = object_config + "/配置.json";

		//建立对象索引目录与对象配置目录
		std::filesystem::create_directories(absolute_get(object_route));
		std::filesystem::create_directories(absolute_get(object_config));

		//写入初态路由文件（仅含路由条目、无任何脏标记）
		route_file_reset();

		//组装配置文件内容
		nlohmann::json config_file;
		config_file["键"] = "值";
		//写入配置文件
		std::ofstream config_stream(absolute_get(config_path));
		config_stream << config_file.dump(4);
		config_stream.close();
	}

	//路由文件重置（覆盖写回「仅含路由条目、无任何脏标记」的初态）
	//被 root_make 用于建立初态；也被需要「让后续扫描再次投递」的用例用于清除脏标记
	void route_file_reset(void)
	{
		//组装初态路由文件内容（顶层无脏标记、条目无脏标记）
		nlohmann::json route_file;
		route_file["config_routes"] = nlohmann::json::array();
		nlohmann::json entry;
		entry["object"] = object_name;
		entry["config_path"] = config_path;
		//若配置了自定义事件类别则写入条目（缺省不写，投递时回落到 Config）
		if (!entry_category.empty())
			entry["event_category"] = entry_category;
		//若配置了自定义事件标签则写入条目（缺省不写，投递时回落到 Load）
		if (!entry_tag.empty())
			entry["event_tag"] = entry_tag;
		route_file["config_routes"].push_back(entry);

		//覆盖写回路由文件
		std::ofstream route_stream(absolute_get(object_route + "/路由.json"));
		route_stream << route_file.dump(4);
		route_stream.close();
	}

	//终端接入中转站
	void loader_attach(void)
	{
		//绑定中转站接入端口
		loader.event_terminal->bind(engine::Port_ID::Broker_Attacher,
			[this](const std::string& name, const std::vector<engine::Event>& needed,
				std::function<void(std::shared_ptr<engine::Event>)> entry)
			{ broker.attach(name, needed, entry); });
		//绑定单事件发送端口
		loader.event_terminal->bind(engine::Port_ID::Evt_Sender,
			[this](std::shared_ptr<engine::Event> evt)
			{ broker.receive(evt); });
		//绑定多事件发送端口
		loader.event_terminal->bind(engine::Port_ID::Evts_Sender,
			[this](std::vector<std::shared_ptr<engine::Event>> events)
			{ broker.receive(events); });
		//绑定单事件中转站呼叫端口（供接收者存在性检查）
		loader.event_terminal->bind(engine::Port_ID::Evt_Broker_Caller,
			[this](std::shared_ptr<engine::Event> evt) { return broker.process(evt); });
		//接入中转站
		loader.attach();
	}

	//订阅对象接入（接收配置投递）
	void object_attach(void)
	{
		//登记订阅对象并留存投递的配置事件
		broker.attach(object_name,
			{ engine::Event("", "", subscribe_category, subscribe_tag) },
			[this](std::shared_ptr<engine::Event> evt) { delivered.push_back(evt); });
	}

	//事件发送（定向到配置加载器）
	void event_send(const std::string& category, const std::string& tag,
		const nlohmann::json& config)
	{
		//构造定向事件
		auto evt = loader.event_terminal.build("", "Config_Loader", category, tag);
		//填充载荷
		evt->config = config;
		//送交中转站
		broker.receive(evt);
	}

	//可信根目录载荷
	static nlohmann::json root_payload(const std::string& route, const std::string& config)
	{
		//组装载荷
		nlohmann::json payload;
		payload["route"] = route;
		payload["config"] = config;
		return payload;
	}

	//对象配置加载载荷
	nlohmann::json object_payload(void) const
	{
		//组装载荷（显式指定该对象的配置文件）
		nlohmann::json payload;
		payload["object"] = object_name;
		payload["files"] = nlohmann::json::array();
		payload["files"].push_back(config_path);
		return payload;
	}

	//路由加载载荷
	static nlohmann::json route_payload(const std::string& route, const std::string& config)
	{
		//组装载荷
		nlohmann::json payload;
		payload["object"] = object_name;
		payload["route"] = route;
		payload["config"] = config;
		return payload;
	}
};

//可信根加载：建立缓存后对象配置可被加载
TEST_F(Config_Loader_Test, 可信根添加后配置可加载)
{
	//准备测试目录
	root_make("可信根测试_添加", false);
	//接入终端与订阅对象
	loader_attach();
	object_attach();

	//加载可信根目录
	event_send("Believed_Root", "Load", root_payload(root_route, root_config));
	//请求对象配置加载
	event_send("Config", "load", object_payload());

	//应投递一次配置
	ASSERT_EQ(delivered.size(), 1u);
	//投递内容应为配置文件正文
	EXPECT_EQ(delivered[0]->config["键"].get<std::string>(), "值");
}

//可信根重复加载：已登记的可信根目录被跳过
TEST_F(Config_Loader_Test, 重复添加可信根被跳过)
{
	//准备测试目录
	root_make("可信根测试_重复添加", false);
	//接入终端与订阅对象
	loader_attach();
	object_attach();

	//加载可信根目录
	event_send("Believed_Root", "Load", root_payload(root_route, root_config));
	//请求对象配置加载
	event_send("Config", "load", object_payload());
	ASSERT_EQ(delivered.size(), 1u);

	//再次加载同一可信根目录（应被跳过，缓存条目脏标记不被重置）
	event_send("Believed_Root", "Load", root_payload(root_route, root_config));
	//再次请求对象配置加载
	event_send("Config", "load", object_payload());

	//脏标记仍在，不应再次投递
	EXPECT_EQ(delivered.size(), 1u);
}

//可信根增量重建：卸载后重新加载会重新登记并重扫重建缓存
TEST_F(Config_Loader_Test, 可信根增量重建恢复缓存)
{
	//准备下探一层的测试目录
	root_make("可信根测试_重建", true);
	//接入终端与订阅对象
	loader_attach();
	object_attach();

	//加载可信根目录（登记并建立缓存；加载本身即时投递一次）
	event_send("Believed_Root", "Load", root_payload(root_route, root_config));
	ASSERT_EQ(delivered.size(), 1u);
	//卸载可信根目录（清除该根下的对象缓存并取消登记）
	event_send("Believed_Root", "Unload", root_payload(root_route, root_config));

	//重置路由文件脏标记：使重建扫描不会被文件头脏标记跳过
	route_file_reset();
	//增量重建可信根目录（已随卸载取消登记 ⇒ 重新登记并重扫建立缓存，再次投递）
	event_send("Believed_Root", "Load", root_payload(root_route, root_config));
	ASSERT_EQ(delivered.size(), 2u);

	//请求对象配置加载：缓存条目已带本轮脏标记 ⇒ 不再重复投递
	event_send("Config", "load", object_payload());
	ASSERT_EQ(delivered.size(), 2u);
	EXPECT_EQ(delivered.back()->config["键"].get<std::string>(), "值");
}

//可信根载荷缺失：事件被驳回
TEST_F(Config_Loader_Test, 可信根载荷缺失被驳回)
{
	//接入终端与订阅对象（不准备任何测试目录）
	loader_attach();
	object_attach();

	//构造缺少配置目录字段的载荷
	nlohmann::json payload;
	payload["route"] = "可信根测试_载荷";
	//发送载荷不完整的添加事件
	event_send("Believed_Root", "Add", payload);
	//请求对象配置加载
	event_send("Config", "load", object_payload());

	//事件被驳回，缓存未建立，不应投递
	EXPECT_EQ(delivered.size(), 0u);
}

//路由加载：仅对象自身目录对命中缓存
TEST_F(Config_Loader_Test, 路由加载仅认对象自身目录对)
{
	//准备下探一层的测试目录
	root_make("可信根测试_目录对", true);
	//接入终端与订阅对象
	loader_attach();
	object_attach();

	//加载可信根目录（本语义下「加载」会登记并建立缓存，且即时投递一次）
	event_send("Believed_Root", "Load", root_payload(root_route, root_config));

	//以可信根目录对请求路由加载（非对象自身目录对，应被驳回：投递数不增加）
	const size_t baseline_count = delivered.size();
	event_send("Route", "load", route_payload(root_route, root_config));
	EXPECT_EQ(delivered.size(), baseline_count);

	//重置路由文件脏标记：使对象自身目录对的加载能真正再次投递（否则会被文件头脏标记跳过）
	route_file_reset();
	//以对象自身目录对请求路由加载（应命中缓存并投递）
	event_send("Route", "load", route_payload(object_route, object_config));
	ASSERT_EQ(delivered.size(), baseline_count + 1);
	EXPECT_EQ(delivered.back()->config["键"].get<std::string>(), "值");
}

//条目自定义事件名：投递事件按条目 event_category/event_tag 发送
TEST_F(Config_Loader_Test, 条目自定义事件名生效)
{
	//配置条目自定义事件名与对应订阅者
	entry_category = "Property";
	entry_tag = "Push";
	subscribe_category = "Property";
	subscribe_tag = "Push";
	//准备测试目录
	root_make("配置事件名测试_自定义", false);
	//接入终端与订阅对象
	loader_attach();
	object_attach();

	//加载可信根目录（应即时按自定义事件名投递一次）
	event_send("Believed_Root", "Load", root_payload(root_route, root_config));

	//应投递一次，且事件名与条目一致
	ASSERT_EQ(delivered.size(), 1u);
	EXPECT_EQ(delivered[0]->category, "Property");
	EXPECT_EQ(delivered[0]->tag, "Push");
}

//条目自定义事件名无订阅者：投递被中转站丢弃
TEST_F(Config_Loader_Test, 条目自定义事件名无订阅者被丢弃)
{
	//配置条目自定义事件名，但订阅者仍只订阅缺省事件名
	entry_category = "Property";
	entry_tag = "Push";
	//准备测试目录
	root_make("配置事件名测试_无订阅", false);
	//接入终端与订阅对象
	loader_attach();
	object_attach();

	//加载可信根目录（投递事件名为 Property/Push，无订阅者应答）
	event_send("Believed_Root", "Load", root_payload(root_route, root_config));

	//投递被丢弃，订阅者收不到事件
	EXPECT_EQ(delivered.size(), 0u);
}

//条目事件名缺省回落：未写 event_category/event_tag 时回落 Config/Load
TEST_F(Config_Loader_Test, 条目事件名缺省回落)
{
	//不配置条目事件名，订阅者订阅缺省事件名
	root_make("配置事件名测试_回落", false);
	//接入终端与订阅对象
	loader_attach();
	object_attach();

	//加载可信根目录（应即时按缺省事件名投递一次）
	event_send("Believed_Root", "Load", root_payload(root_route, root_config));

	//应投递一次，且事件名为缺省值
	ASSERT_EQ(delivered.size(), 1u);
	EXPECT_EQ(delivered[0]->category, "Config");
	EXPECT_EQ(delivered[0]->tag, "Load");
}

//配置卸载：卸载指定文件后可经路由加载重建
TEST_F(Config_Loader_Test, 配置卸载后可重建)
{
	//准备测试目录
	root_make("配置卸载测试", false);
	//接入终端与订阅对象
	loader_attach();
	object_attach();

	//加载可信根目录（即时投递一次）
	event_send("Believed_Root", "Load", root_payload(root_route, root_config));
	ASSERT_EQ(delivered.size(), 1u);

	//卸载该对象的指定配置文件缓存（条目标记与文件头标记一并被抹除）
	event_send("Config", "Unload", object_payload());
	//再次请求对象配置加载：缓存已清且无文件清单可加载，不应投递
	event_send("Config", "load", object_payload());
	EXPECT_EQ(delivered.size(), 1u);

	//以同一目录对请求路由加载（登记仍在、标记已抹除 ⇒ 重新扫描并投递）
	event_send("Route", "load", route_payload(object_route, object_config));
	ASSERT_EQ(delivered.size(), 2u);
	EXPECT_EQ(delivered.back()->config["键"].get<std::string>(), "值");
}

//路由卸载：卸载该目录对后可经路由加载重建
TEST_F(Config_Loader_Test, 路由卸载后可重建)
{
	//准备测试目录
	root_make("路由卸载测试", false);
	//接入终端与订阅对象
	loader_attach();
	object_attach();

	//加载可信根目录（即时投递一次）
	event_send("Believed_Root", "Load", root_payload(root_route, root_config));
	ASSERT_EQ(delivered.size(), 1u);

	//卸载该目录对下该对象的缓存
	event_send("Route", "Unload", route_payload(object_route, object_config));
	//对象配置缓存已清，加载不再投递
	event_send("Config", "load", object_payload());
	EXPECT_EQ(delivered.size(), 1u);

	//以同一目录对请求路由加载（重新扫描并投递）
	event_send("Route", "load", route_payload(object_route, object_config));
	ASSERT_EQ(delivered.size(), 2u);
	EXPECT_EQ(delivered.back()->config["键"].get<std::string>(), "值");
}

//配置卸载空载荷：未给 files 时全量卸载该对象
TEST_F(Config_Loader_Test, 配置卸载空载荷全卸载)
{
	//准备测试目录
	root_make("配置全卸载测试", false);
	//接入终端与订阅对象
	loader_attach();
	object_attach();

	//加载可信根目录（即时投递一次）
	event_send("Believed_Root", "Load", root_payload(root_route, root_config));
	ASSERT_EQ(delivered.size(), 1u);

	//构造仅含对象字段的卸载载荷（无 files ⇒ 全量卸载）
	nlohmann::json payload;
	payload["object"] = object_name;
	event_send("Config", "Unload", payload);
	//对象配置缓存已清，加载不再投递
	event_send("Config", "load", object_payload());
	EXPECT_EQ(delivered.size(), 1u);

	//以同一目录对请求路由加载（重新扫描并投递）
	event_send("Route", "load", route_payload(object_route, object_config));
	ASSERT_EQ(delivered.size(), 2u);
}

//非法卸载目标：对象无缓存或目录对未登记时被驳回
TEST_F(Config_Loader_Test, 非法卸载目标被驳回)
{
	//准备测试目录
	root_make("非法卸载测试", false);
	//接入终端与订阅对象
	loader_attach();
	object_attach();

	//加载可信根目录（即时投递一次）
	event_send("Believed_Root", "Load", root_payload(root_route, root_config));
	ASSERT_EQ(delivered.size(), 1u);

	//卸载一个无配置记录的对象（应被驳回）
	nlohmann::json bad_payload;
	bad_payload["object"] = "不存在对象";
	event_send("Config", "Unload", bad_payload);
	//卸载一个未登记的目录对（应被驳回）
	event_send("Route", "Unload", route_payload(object_route + "/无此目录", object_config + "/无此目录"));
	//对象配置缓存未被清除，仍可命中脏标记而不重复投递
	event_send("Config", "load", object_payload());

	//投递数保持不变
	EXPECT_EQ(delivered.size(), 1u);
}