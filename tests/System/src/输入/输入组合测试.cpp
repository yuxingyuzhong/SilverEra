/*
输入键位扩展与组合映射测试
覆盖：新键位种类解析、位域编码校验、组合跨帧 / 同帧触发、组合抑制成员单键、
      更具体组合优先、左右修饰键等价、held 权威快照、帧号跳变清空、
      注入组合键、组合表重载、监听与组合共存、单键向后兼容
*/
#include "Engine/EngineFoundation/common/前置头文件包含.h"
#include <gtest/gtest.h>

//获取事件中转器
#include "Engine/EngineFoundation/src/event/Event_Broker/事件中转器.h"
//获取事件终端
#include "Engine/EngineFoundation/src/event/Event_Terminal/事件终端.h"
//获取事件协议
#include "Engine/EngineFoundation/src/event/Common/事件协议.h"
//获取输入处理系统
#include "Engine/EngineSystem/src/input/mapping/Input_Manager/输入管理器.h"
//获取焦点管理系统
#include "Engine/EngineSystem/src/input/focus/Focus_Manager/焦点系统.h"

//游戏引擎命名空间
using namespace engine;

//输入组合测试夹具
class Input_Chord_Test : public ::testing::Test
{
protected:
	//事件中转站
	Event_Broker broker{};
	//输入处理系统
	Input_Manager input{};
	//焦点管理系统
	Focus_System focus{};
	//命令监听终端
	Event_Terminal listener{};
	//命令监听密钥
	int64_t listener_key = 0;
	//收到的语义命令
	std::vector<std::shared_ptr<Event>> commands{};

	//测试准备
	void SetUp(void) override
	{
		// —— 输入系统接线 ——
		input.event_terminal->bind(Port_ID::Broker_Attacher,
			[this](const std::string& name, const uint64_t& key, auto receiver) -> void
			{
				this->broker.attach(name, key, receiver);
			});
		input.event_terminal->bind(Port_ID::Evt_Sender,
			[this](std::shared_ptr<Event> evt) -> void
			{
				this->broker.receive(evt);
			});
		ASSERT_TRUE(input.attach());

		// —— 焦点系统接线 ——
		focus.event_terminal->bind(Port_ID::Broker_Attacher,
			[this](const std::string& name, const uint64_t& key, auto receiver) -> void
			{
				this->broker.attach(name, key, receiver);
			});
		focus.event_terminal->bind(Port_ID::Evt_Sender,
			[this](std::shared_ptr<Event> evt) -> void
			{
				this->broker.receive(evt);
			});
		ASSERT_TRUE(focus.attach());

		// —— 命令监听接线 ——
		listener->bind(Port_ID::Broker_Attacher,
			[this](const std::string& name, const uint64_t& key, auto receiver) -> void
			{
				this->broker.attach(name, key, receiver);
			});
		listener->bind(Port_ID::Evt_Sender,
			[this](std::shared_ptr<Event> evt) -> void
			{
				this->broker.receive(evt);
			});
		listener->bind(Port_ID::Evt_Receiver,
			[this](std::shared_ptr<Event> evt) -> void
			{
				this->commands.push_back(evt);
			});
		//生成并登记监听密钥
		listener_key = listener.key().value_or(0);
		ASSERT_TRUE(listener.attach("Command_Listener", listener_key));

		//订阅语义命令
		auto subscribe_evt = listener.build("", "", event_protocol::subscribed_events, event_protocol::add);
		ASSERT_TRUE(subscribe_evt != nullptr);
		//订阅项：Input / Command
		nlohmann::json subscribed_event = nlohmann::json::object();
		subscribed_event[event_protocol::field_category] = "Input";
		subscribed_event[event_protocol::field_tag] = "Command";
		subscribe_evt->config[event_protocol::field_object] = "Command_Listener";
		subscribe_evt->config[event_protocol::field_acl_key] = static_cast<uint64_t>(listener_key);
		subscribe_evt->config[event_protocol::field_subscribed_events] =
			nlohmann::json::array({ subscribed_event });
		ASSERT_TRUE(listener.send(subscribe_evt, listener_key));
	}

	//构造映射项
	static nlohmann::json binding_make(const std::string& key, const std::string& command,
		const std::string& target = {}, const std::vector<std::string>& modifiers = {},
		const bool& repeat = false)
	{
		//映射项
		nlohmann::json item = nlohmann::json::object();
		item["key"] = key;
		item["command"] = command;
		//若指定目标对象
		if (!target.empty())
			item["target"] = target;
		//若指定组合修饰项
		if (!modifiers.empty())
		{
			nlohmann::json list = nlohmann::json::array();
			for (const auto& modifier : modifiers)
				list.push_back(modifier);
			item["modifiers"] = list;
		}
		//若开启长按重复响应
		if (repeat)
			item["repeat"] = true;
		return item;
	}

	//构造单条按键边沿
	static nlohmann::json key_edge(const std::string& key, bool pressed = true)
	{
		nlohmann::json edge = nlohmann::json::object();
		edge["key"] = key;
		edge["pressed"] = pressed;
		return edge;
	}

	//构造长按重复边沿
	static nlohmann::json repeat_edge(const std::string& key)
	{
		nlohmann::json edge = nlohmann::json::object();
		edge["key"] = key;
		edge["pressed"] = true;
		edge["repeat"] = true;
		return edge;
	}

	//发送对象创建事件
	void object_create(const nlohmann::json& config)
	{
		auto evt = std::make_shared<Event>("Test", "Input_Manager", "Input", "Create", config);
		broker.receive(evt);
	}

	//发送映射表重载事件
	void object_reload(const std::string& name, const nlohmann::json& mapping)
	{
		nlohmann::json config = nlohmann::json::object();
		config["object"] = name;
		config["mapping"] = mapping;
		auto evt = std::make_shared<Event>("Test", "Input_Manager", "Input", "Reload", config);
		broker.receive(evt);
	}

	//发送原始输入帧（完整载荷）
	void raw_frame(const nlohmann::json& config)
	{
		auto evt = std::make_shared<Event>("Renderer", "Focus_System", "Input", "Raw", config);
		broker.receive(evt);
	}

	//发送原始输入帧（仅键位边沿）
	void raw_input(const nlohmann::json& keys, const uint64_t& frame)
	{
		nlohmann::json config = nlohmann::json::object();
		config["frame"] = frame;
		config["keys"] = keys;
		raw_frame(config);
	}

	//发送模态压栈（指定发送者）
	void modal_push_by(const std::string& sender, const std::string& name)
	{
		nlohmann::json config = nlohmann::json::object();
		config["object"] = name;
		auto evt = std::make_shared<Event>(sender, "Focus_System", "Input", "ModalPush", config);
		broker.receive(evt);
	}

	//发送模态弹栈（指定发送者）
	void modal_pop_by(const std::string& sender)
	{
		auto evt = std::make_shared<Event>(sender, "Focus_System", "Input", "ModalPop",
			nlohmann::json::object());
		broker.receive(evt);
	}
};

//新键位种类解析：符号 / 导航 / 小键盘 / 鼠标扩展 / 修饰键 / 滚轮
TEST_F(Input_Chord_Test, 新键位种类解析)
{
	//待解析键位
	Input_Key key = 0;

	//符号键（字符写法）
	ASSERT_TRUE(input_mapping::key_parse(nlohmann::json("-"), key));
	EXPECT_EQ(input_key_class_get(key), Input_Key_Class::main);
	//符号键（英文键名写法）
	ASSERT_TRUE(input_mapping::key_parse(nlohmann::json("PageDown"), key));
	EXPECT_EQ(input_key_class_get(key), Input_Key_Class::control);
	//小键盘
	ASSERT_TRUE(input_mapping::key_parse(nlohmann::json("KP5"), key));
	EXPECT_EQ(input_key_class_get(key), Input_Key_Class::keypad);
	EXPECT_EQ(input_key_code_get(key), 6u);
	//鼠标扩展键
	ASSERT_TRUE(input_mapping::key_parse(nlohmann::json("MOUSE_X1"), key));
	EXPECT_EQ(input_key_class_get(key), Input_Key_Class::mouse);
	//滚轮
	ASSERT_TRUE(input_mapping::key_parse(nlohmann::json("WHEEL_UP"), key));
	EXPECT_EQ(input_key_class_get(key), Input_Key_Class::wheel);

	//左右修饰键可区分且规范化掩码一致
	Input_Key left_shift = 0;
	Input_Key right_shift = 0;
	ASSERT_TRUE(input_mapping::key_parse(nlohmann::json("LSHIFT"), left_shift));
	ASSERT_TRUE(input_mapping::key_parse(nlohmann::json("RSHIFT"), right_shift));
	EXPECT_NE(left_shift, right_shift);
	EXPECT_EQ(input_mapping::modifier_mask_of(left_shift), INPUT_MODIFIER_SHIFT);
	EXPECT_EQ(input_mapping::modifier_mask_of(right_shift), INPUT_MODIFIER_SHIFT);
}

//位域编码校验：合法位域可用，旧平铺数字与空编号被拒
TEST_F(Input_Chord_Test, 位域编码校验)
{
	//待解析键位
	Input_Key key = 0;
	//合法位域
	ASSERT_TRUE(input_mapping::key_parse(nlohmann::json(
		static_cast<uint64_t>(input_key_make(Input_Key_Class::main, 1))), key));
	EXPECT_EQ(key, input_key_make(Input_Key_Class::main, 1));
	//旧平铺数字（1）
	EXPECT_FALSE(input_mapping::key_parse(nlohmann::json(1), key));
	//键类合法但类内编号为 0
	EXPECT_FALSE(input_mapping::key_parse(nlohmann::json(0x10000000), key));
	//键类越界
	EXPECT_FALSE(input_mapping::key_parse(nlohmann::json(0x60000001), key));
}

//组合跨帧触发：先按修饰键，再按主键
TEST_F(Input_Chord_Test, 组合跨帧触发)
{
	//创建主对象
	nlohmann::json player = nlohmann::json::object();
	player["name"] = "Player";
	player["mapping"] = nlohmann::json::array({
		binding_make("S", "Save", {}, { "CTRL" }) });
	object_create(player);
	focus.frame_begin();
	ASSERT_EQ(focus.current_main_get(), "Player");

	//帧 1：按下左 Ctrl
	raw_input(nlohmann::json::array({ key_edge("LCTRL") }), 1);
	EXPECT_TRUE(commands.empty());

	//帧 2：按下 S
	raw_input(nlohmann::json::array({ key_edge("S") }), 2);
	ASSERT_EQ(commands.size(), std::size_t{1});
	EXPECT_EQ(commands[0]->config["command"], "Save");
}

//组合同帧触发：修饰键与主键同帧按下
TEST_F(Input_Chord_Test, 组合同帧触发)
{
	//创建主对象
	nlohmann::json player = nlohmann::json::object();
	player["name"] = "Player";
	player["mapping"] = nlohmann::json::array({
		binding_make("S", "Save", {}, { "CTRL" }) });
	object_create(player);
	focus.frame_begin();

	//同帧按下 Ctrl + S
	raw_input(nlohmann::json::array({ key_edge("LCTRL"), key_edge("S") }), 1);
	ASSERT_EQ(commands.size(), std::size_t{1});
	EXPECT_EQ(commands[0]->config["command"], "Save");
}

//组合抑制成员单键：Ctrl、S 各自有单键绑定，组合命中后均不产出
TEST_F(Input_Chord_Test, 组合抑制成员单键)
{
	//创建主对象
	nlohmann::json player = nlohmann::json::object();
	player["name"] = "Player";
	player["mapping"] = nlohmann::json::array({
		binding_make("LCTRL", "FocusMode"),
		binding_make("S", "Move"),
		binding_make("S", "Save", {}, { "CTRL" }) });
	object_create(player);
	focus.frame_begin();

	//同帧按下 Ctrl + S
	raw_input(nlohmann::json::array({ key_edge("LCTRL"), key_edge("S") }), 1);
	ASSERT_EQ(commands.size(), std::size_t{1});
	EXPECT_EQ(commands[0]->config["command"], "Save");
}

//释放修饰键后组合失效：普通单键恢复产出
TEST_F(Input_Chord_Test, 释放修饰键后组合失效)
{
	//创建主对象
	nlohmann::json player = nlohmann::json::object();
	player["name"] = "Player";
	player["mapping"] = nlohmann::json::array({
		binding_make("S", "Move"),
		binding_make("S", "Save", {}, { "CTRL" }) });
	object_create(player);
	focus.frame_begin();

	//帧 1：按下 Ctrl
	raw_input(nlohmann::json::array({ key_edge("LCTRL") }), 1);
	//帧 2：按下 S，组合命中
	raw_input(nlohmann::json::array({ key_edge("S") }), 2);
	ASSERT_EQ(commands.size(), std::size_t{1});
	EXPECT_EQ(commands[0]->config["command"], "Save");

	//帧 3：释放 S 与 Ctrl
	raw_input(nlohmann::json::array({ key_edge("S", false), key_edge("LCTRL", false) }), 3);
	EXPECT_EQ(commands.size(), std::size_t{1});

	//帧 4：仅按下 S，落到单键
	raw_input(nlohmann::json::array({ key_edge("S") }), 4);
	ASSERT_EQ(commands.size(), std::size_t{2});
	EXPECT_EQ(commands[1]->config["command"], "Move");
}

//更具体组合优先：Ctrl+Shift+S 优先于 Ctrl+S
TEST_F(Input_Chord_Test, 更具体组合优先)
{
	//创建主对象
	nlohmann::json player = nlohmann::json::object();
	player["name"] = "Player";
	player["mapping"] = nlohmann::json::array({
		binding_make("S", "Save", {}, { "CTRL" }),
		binding_make("S", "SaveAll", {}, { "CTRL", "SHIFT" }) });
	object_create(player);
	focus.frame_begin();

	//同帧按下 Ctrl + Shift + S
	raw_input(nlohmann::json::array({
		key_edge("LCTRL"), key_edge("LSHIFT"), key_edge("S") }), 1);
	ASSERT_EQ(commands.size(), std::size_t{1});
	EXPECT_EQ(commands[0]->config["command"], "SaveAll");
}

//左右修饰键等价：组合写 SHIFT 时右 Shift 同样命中
TEST_F(Input_Chord_Test, 左右修饰键等价)
{
	//创建主对象
	nlohmann::json player = nlohmann::json::object();
	player["name"] = "Player";
	player["mapping"] = nlohmann::json::array({
		binding_make("S", "Run", {}, { "SHIFT" }) });
	object_create(player);
	focus.frame_begin();

	//用右 Shift 参与组合
	raw_input(nlohmann::json::array({ key_edge("RSHIFT"), key_edge("S") }), 1);
	ASSERT_EQ(commands.size(), std::size_t{1});
	EXPECT_EQ(commands[0]->config["command"], "Run");
}

//held 权威快照：快照中的修饰键无需出现在边沿序列
TEST_F(Input_Chord_Test, held权威快照)
{
	//创建主对象
	nlohmann::json player = nlohmann::json::object();
	player["name"] = "Player";
	player["mapping"] = nlohmann::json::array({
		binding_make("S", "Save", {}, { "CTRL" }) });
	object_create(player);
	focus.frame_begin();

	//帧 1：仅提供按住快照
	nlohmann::json frame_one = nlohmann::json::object();
	frame_one["frame"] = 1;
	frame_one["keys"] = nlohmann::json::array();
	frame_one["held"] = nlohmann::json::array({ "LCTRL" });
	raw_frame(frame_one);
	EXPECT_TRUE(commands.empty());

	//帧 2：只给 S 的按下沿
	raw_input(nlohmann::json::array({ key_edge("S") }), 2);
	ASSERT_EQ(commands.size(), std::size_t{1});
	EXPECT_EQ(commands[0]->config["command"], "Save");

	//帧 3：快照清空后组合不再命中
	nlohmann::json frame_three = nlohmann::json::object();
	frame_three["frame"] = 3;
	frame_three["keys"] = nlohmann::json::array({ key_edge("S", false) });
	frame_three["held"] = nlohmann::json::array();
	raw_frame(frame_three);
	raw_input(nlohmann::json::array({ key_edge("S") }), 4);
	EXPECT_EQ(commands.size(), std::size_t{1});
}

//帧号跳变清空：按住状态不可跨帧号断层沿用
TEST_F(Input_Chord_Test, 帧号跳变清空按住状态)
{
	//创建主对象
	nlohmann::json player = nlohmann::json::object();
	player["name"] = "Player";
	player["mapping"] = nlohmann::json::array({
		binding_make("S", "Save", {}, { "CTRL" }) });
	object_create(player);
	focus.frame_begin();

	//帧 1：按下 Ctrl
	raw_input(nlohmann::json::array({ key_edge("LCTRL") }), 1);
	//帧 3：跳号后按 S，旧按住状态已清空
	raw_input(nlohmann::json::array({ key_edge("S") }), 3);
	EXPECT_TRUE(commands.empty());
}

//注入组合键：子对象声明的 Ctrl+E 生效并触发焦点请求
TEST_F(Input_Chord_Test, 注入组合键)
{
	//创建主对象
	nlohmann::json player = nlohmann::json::object();
	player["name"] = "Player";
	object_create(player);
	focus.frame_begin();
	ASSERT_EQ(focus.current_main_get(), "Player");

	//子对象注入组合键
	nlohmann::json backpack = nlohmann::json::object();
	backpack["name"] = "Backpack";
	backpack["host"] = "Player";
	backpack["added_keys"] = nlohmann::json::array({
		binding_make("E", "Enter", "Backpack", { "CTRL" }) });
	object_create(backpack);
	ASSERT_EQ(input.object_injected_count("Backpack"), std::size_t{1});

	//按下 Ctrl + E
	raw_input(nlohmann::json::array({ key_edge("LCTRL"), key_edge("E") }), 1);
	//转移尚未生效
	EXPECT_EQ(focus.current_child_get(), "");
	//帧首应用后子对象接管
	focus.frame_begin();
	EXPECT_EQ(focus.current_child_get(), "Backpack");
}

//组合表重载：旧组合失效、新组合生效
TEST_F(Input_Chord_Test, 组合表重载)
{
	//创建无映射主对象
	nlohmann::json player = nlohmann::json::object();
	player["name"] = "Player";
	object_create(player);
	focus.frame_begin();

	//重载为 Ctrl+S → Save
	object_reload("Player", nlohmann::json::array({
		binding_make("S", "Save", {}, { "CTRL" }) }));
	//帧 1：按下 Ctrl
	raw_input(nlohmann::json::array({ key_edge("LCTRL") }), 1);
	//帧 2：按下 S
	raw_input(nlohmann::json::array({ key_edge("S") }), 2);
	ASSERT_EQ(commands.size(), std::size_t{1});
	EXPECT_EQ(commands[0]->config["command"], "Save");

	//重载为 Ctrl+S → SaveAs
	object_reload("Player", nlohmann::json::array({
		binding_make("S", "SaveAs", {}, { "CTRL" }) }));
	//帧 3：释放 S
	raw_input(nlohmann::json::array({ key_edge("S", false) }), 3);
	//帧 4：再次按下 S
	raw_input(nlohmann::json::array({ key_edge("S") }), 4);
	ASSERT_EQ(commands.size(), std::size_t{2});
	EXPECT_EQ(commands[1]->config["command"], "SaveAs");
}

//监听与组合共存：监听拷贝仍按主键派发，组合判定互不破坏
TEST_F(Input_Chord_Test, 监听与组合共存)
{
	//创建宿主，W 单键与 Ctrl+W 组合并存
	nlohmann::json overlay = nlohmann::json::object();
	overlay["name"] = "Overlay";
	overlay["mapping"] = nlohmann::json::array({
		binding_make("W", "Move"),
		binding_make("W", "Run", {}, { "CTRL" }) });
	object_create(overlay);
	focus.frame_begin();
	ASSERT_EQ(focus.current_main_get(), "Overlay");

	//子对象监听 W 并自行映射为 Log
	nlohmann::json hud = nlohmann::json::object();
	hud["name"] = "Hud";
	hud["host"] = "Overlay";
	hud["listen"] = nlohmann::json::array({ "W" });
	hud["mapping"] = nlohmann::json::array({ binding_make("W", "Log") });
	object_create(hud);

	//按下 Ctrl + W
	raw_input(nlohmann::json::array({ key_edge("LCTRL"), key_edge("W") }), 1);
	ASSERT_EQ(commands.size(), std::size_t{2});

	//宿主产出组合命令，子对象产出监听命令
	bool has_run = false;
	bool has_log = false;
	for (const auto& command : commands)
	{
		const std::string name = command->config.value("command", std::string{});
		if (name == "Run")
			has_run = true;
		if (name == "Log")
			has_log = true;
		EXPECT_NE(name, "Move");
	}
	EXPECT_TRUE(has_run);
	EXPECT_TRUE(has_log);
}

//单键向后兼容：无 modifiers 的映射行为不变
TEST_F(Input_Chord_Test, 单键向后兼容)
{
	//创建主对象
	nlohmann::json player = nlohmann::json::object();
	player["name"] = "Player";
	player["mapping"] = nlohmann::json::array({ binding_make("W", "Move") });
	object_create(player);
	focus.frame_begin();

	//按下 W
	raw_input(nlohmann::json::array({ key_edge("W") }), 1);
	ASSERT_EQ(commands.size(), std::size_t{1});
	EXPECT_EQ(commands[0]->config["command"], "Move");
}

//名称表与命名常量一致（键位编号单一真源的长期守卫）
TEST_F(Input_Chord_Test, 名称表与常量一致)
{
	//名称与常量的对应项（覆盖五类键位与常用别名）
	struct Name_Key_Case
	{
		//配置名称
		const char* name;
		//期望键位
		Input_Key key;
	};
	const Name_Key_Case cases[]{
		{"A", INPUT_KEY_A}, {"Z", INPUT_KEY_Z},
		{"0", INPUT_KEY_0}, {"9", INPUT_KEY_9},
		{"-", INPUT_KEY_MINUS}, {"Backslash", INPUT_KEY_BACKSLASH},
		{"Escape", INPUT_KEY_ESCAPE}, {"PageDown", INPUT_KEY_PAGE_DOWN},
		{"F12", INPUT_KEY_F12}, {"CapsLock", INPUT_KEY_CAPS_LOCK},
		{"LSHIFT", INPUT_KEY_LSHIFT}, {"RGUI", INPUT_KEY_RGUI},
		{"KP0", INPUT_KEY_KP_0}, {"KP5", INPUT_KEY_KP_5}, {"KP_ENTER", INPUT_KEY_KP_ENTER},
		{"MOUSE_X2", INPUT_KEY_MOUSE_X2}, {"WHEEL_DOWN", INPUT_KEY_WHEEL_DOWN},
		//别名
		{"ESC", INPUT_KEY_ESCAPE}, {"RETURN", INPUT_KEY_ENTER},
		{"PGUP", INPUT_KEY_PAGE_UP}, {"DEL", INPUT_KEY_DELETE},
		{"BACKQUOTE", INPUT_KEY_GRAVE}, {"LCONTROL", INPUT_KEY_LCTRL}
	};

	//逐项解析并与常量比对
	for (const auto& item : cases)
	{
		Input_Key key = 0;
		ASSERT_TRUE(input_mapping::key_parse(nlohmann::json(item.name), key))
			<< "无法解析名称：" << item.name;
		EXPECT_EQ(key, item.key) << "名称与常量不一致：" << item.name;
	}
}

//长按重复：开启 repeat 的绑定响应重复沿并标记载荷
TEST_F(Input_Chord_Test, 重复沿触发)
{
	//创建主对象，删除键绑定开启重复
	nlohmann::json player = nlohmann::json::object();
	player["name"] = "Player";
	player["mapping"] = nlohmann::json::array({
		binding_make("DELETE", "DeleteChar", {}, {}, true) });
	object_create(player);
	focus.frame_begin();

	//发送一条长按重复沿
	raw_input(nlohmann::json::array({ repeat_edge("DELETE") }), 1);
	ASSERT_EQ(commands.size(), std::size_t{1});
	EXPECT_EQ(commands[0]->config["command"], "DeleteChar");
	EXPECT_TRUE(commands[0]->config.value("repeat", false));
}

//长按重复：未开启 repeat 的绑定忽略重复沿
TEST_F(Input_Chord_Test, 重复沿未开启忽略)
{
	//创建主对象，删除键绑定未开启重复
	nlohmann::json player = nlohmann::json::object();
	player["name"] = "Player";
	player["mapping"] = nlohmann::json::array({
		binding_make("DELETE", "DeleteChar") });
	object_create(player);
	focus.frame_begin();

	//发送一条长按重复沿
	raw_input(nlohmann::json::array({ repeat_edge("DELETE") }), 1);
	EXPECT_TRUE(commands.empty());
}

//长按重复：修饰键重复沿不重复触发组合
TEST_F(Input_Chord_Test, 重复沿不影响组合)
{
	//创建主对象，Ctrl+S 组合
	nlohmann::json player = nlohmann::json::object();
	player["name"] = "Player";
	player["mapping"] = nlohmann::json::array({
		binding_make("S", "Save", {}, { "CTRL" }) });
	object_create(player);
	focus.frame_begin();

	//帧 1：按下左 Ctrl
	raw_input(nlohmann::json::array({ key_edge("LCTRL") }), 1);
	//帧 2：Ctrl 重复沿 + S 按下
	raw_input(nlohmann::json::array({ repeat_edge("LCTRL"), key_edge("S") }), 2);
	ASSERT_EQ(commands.size(), std::size_t{1});
	EXPECT_EQ(commands[0]->config["command"], "Save");
}

//语义状态查询：按住键位、修饰掩码与指针快照
TEST_F(Input_Chord_Test, 语义状态查询)
{
	//创建主对象
	nlohmann::json player = nlohmann::json::object();
	player["name"] = "Player";
	object_create(player);
	focus.frame_begin();

	//构造带按住快照与指针的原始帧
	nlohmann::json raw = nlohmann::json::object();
	raw["frame"] = 1;
	raw["keys"] = nlohmann::json::array({ key_edge("W") });
	raw["held"] = nlohmann::json::array({ "LCTRL", "W" });
	nlohmann::json pointer = nlohmann::json::object();
	pointer["x"] = 100.0;
	pointer["y"] = 40.0;
	pointer["dx"] = 3.0;
	pointer["dy"] = -1.0;
	pointer["wheel_x"] = 0.0;
	pointer["wheel_y"] = 2.0;
	raw["pointer"] = pointer;
	raw_frame(raw);

	//键位解析以复用同一套编码
	Input_Key key_ctrl = 0;
	Input_Key key_w = 0;
	ASSERT_TRUE(input_mapping::key_parse(nlohmann::json("LCTRL"), key_ctrl));
	ASSERT_TRUE(input_mapping::key_parse(nlohmann::json("W"), key_w));

	//按住查询
	EXPECT_TRUE(input.key_held_get(key_ctrl));
	EXPECT_TRUE(input.key_held_get(key_w));
	EXPECT_FALSE(input.key_held_get(INPUT_KEY_A));
	//修饰掩码查询
	EXPECT_EQ(input.modifier_mask_get(), INPUT_MODIFIER_CTRL);
	//指针快照查询
	const Input_Cursor_Snapshot cursor = input.cursor_get();
	EXPECT_DOUBLE_EQ(cursor.x, 100.0);
	EXPECT_DOUBLE_EQ(cursor.y, 40.0);
	EXPECT_DOUBLE_EQ(cursor.dx, 3.0);
	EXPECT_DOUBLE_EQ(cursor.dy, -1.0);
	EXPECT_DOUBLE_EQ(cursor.wheel_y, 2.0);
}

//非指针轴：本帧未投递 axes 时恒为 0.0，且不影响指针轴
TEST_F(Input_Chord_Test, 非指针轴缺生产者)
{
	//创建带两类轴绑定的对象
	nlohmann::json camera = nlohmann::json::object();
	camera["name"] = "Camera";
	nlohmann::json axes = nlohmann::json::array();
	nlohmann::json throttle = nlohmann::json::object();
	throttle["axis"] = "Throttle";
	throttle["source"] = "raw_throttle";
	throttle["mode"] = "displacement";
	axes.push_back(throttle);
	nlohmann::json look_x = nlohmann::json::object();
	look_x["axis"] = "Look/X";
	look_x["source"] = "pointer.dx";
	look_x["mode"] = "displacement";
	look_x["scale"] = 0.1;
	look_x["clamp"] = 1.0;
	axes.push_back(look_x);
	camera["axes"] = axes;
	object_create(camera);
	focus.frame_begin();

	//仅投递指针，不投递 axes
	nlohmann::json raw = nlohmann::json::object();
	raw["frame"] = 1;
	raw["keys"] = nlohmann::json::array();
	nlohmann::json pointer = nlohmann::json::object();
	pointer["dx"] = 10.0;
	raw["pointer"] = pointer;
	raw_frame(raw);

	//原始轴恒为 0，指针轴照常生效
	EXPECT_DOUBLE_EQ(input.object_axis_value_get("Camera", "Throttle"), 0.0);
	EXPECT_DOUBLE_EQ(input.object_axis_value_get("Camera", "Look/X"), 1.0);
}

//模态归属：非压栈者的弹栈请求被拒绝
TEST_F(Input_Chord_Test, 模态弹栈归属校验)
{
	//创建两个主对象
	nlohmann::json first = nlohmann::json::object();
	first["name"] = "First";
	object_create(first);
	focus.frame_begin();
	nlohmann::json second = nlohmann::json::object();
	second["name"] = "Second";
	object_create(second);

	//甲模块压栈
	modal_push_by("UI_A", "First");
	ASSERT_EQ(focus.modal_size(), std::size_t{1});

	//乙模块弹栈被拒
	modal_pop_by("UI_B");
	EXPECT_EQ(focus.modal_size(), std::size_t{1});

	//甲模块弹栈成功
	modal_pop_by("UI_A");
	EXPECT_EQ(focus.modal_size(), std::size_t{0});
}
