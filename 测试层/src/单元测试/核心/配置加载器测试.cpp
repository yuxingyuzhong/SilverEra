//配置加载器测试：覆盖可信根目录的建立、卸载、增量重建与只读缓存加载链路
#include <gtest/gtest.h>

//获取配置加载器
#include "src/core/config/Config_Loader/配置加载器.h"
//获取事件中转器
#include "src/core/event/Event_Broker/事件中转器.h"
//获取路径字符串转换工具
#include "src/tools/Detail/路径字符串转换.h"

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
		route_file["config_routes"].push_back(entry);

		//覆盖写回路由文件
		std::ofstream route_stream(absolute_get(object_route + "/路由.json"));
		route_stream << route_file.dump(4);
		route_stream.close();
	}

	//终端接入中转站
	void loader_attach(void)
	{
		//注册中转站接入入口
		loader.event_terminal->attach_handler_register(
			[this](const std::string& name, const std::vector<engine::Event>& needed,
				std::function<void(std::shared_ptr<engine::Event>)> entry)
			{ broker.attach(name, needed, entry); });
		//注册事件发送入口（单事件重载）
		loader.event_terminal->event_sender_register(
			[this](std::shared_ptr<engine::Event> evt)
			{ broker.receive(evt); });
		//注册事件发送入口（批量重载）
		loader.event_terminal->event_sender_register(
			[this](std::vector<std::shared_ptr<engine::Event>> events)
			{ broker.receive(events); });
		//注册中转站交互入口（供接收者存在性检查）
		loader.event_terminal->event_interactor_register(
			[this](std::shared_ptr<engine::Event> evt) { return broker.process(evt); });
		//接入中转站
		loader.attach();
	}

	//订阅对象接入（接收配置投递）
	void object_attach(void)
	{
		//登记订阅对象并留存投递的配置事件
		broker.attach(object_name, { engine::Event("", "", "Config", "Load") },
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
	event_send("Belived_Root", "Load", root_payload(root_route, root_config));
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
	event_send("Belived_Root", "Load", root_payload(root_route, root_config));
	//请求对象配置加载
	event_send("Config", "load", object_payload());
	ASSERT_EQ(delivered.size(), 1u);

	//再次加载同一可信根目录（应被跳过，缓存条目脏标记不被重置）
	event_send("Belived_Root", "Load", root_payload(root_route, root_config));
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
	event_send("Belived_Root", "Load", root_payload(root_route, root_config));
	ASSERT_EQ(delivered.size(), 1u);
	//卸载可信根目录（清除该根下的对象缓存并取消登记）
	event_send("Belived_Root", "Unload", root_payload(root_route, root_config));

	//重置路由文件脏标记：使重建扫描不会被文件头脏标记跳过
	route_file_reset();
	//增量重建可信根目录（已随卸载取消登记 ⇒ 重新登记并重扫建立缓存，再次投递）
	event_send("Belived_Root", "Load", root_payload(root_route, root_config));
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
	event_send("Belived_Root", "Add", payload);
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
	event_send("Belived_Root", "Load", root_payload(root_route, root_config));

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