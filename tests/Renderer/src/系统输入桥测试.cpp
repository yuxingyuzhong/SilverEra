/*
系统输入桥测试（L1 无 GPU）
覆盖：SDL 扫描码 / 鼠标 / 滚轮 → 系统中立键位位域翻译、
      Input/Raw 载荷字段（frame / dt / keys / held / pointer / text_mode）、
      渲染层构造的 Raw 经真实中转站 + 焦点系统 + 输入管理器端到端产出语义命令。
*/
#include <gtest/gtest.h>

//获取事件中转器、输入管理器与焦点系统
#include "Engine/EngineFoundation/src/event/Event_Broker/事件中转器.h"
#include "Engine/EngineSystem/src/input/mapping/Input_Manager/输入管理器.h"
#include "Engine/EngineSystem/src/input/focus/Focus_Manager/焦点系统.h"
//获取渲染层输入快照与系统输入桥
#include "Engine/EngineRenderer/src/render/Input/输入聚合.h"
#include "Engine/EngineRenderer/src/render/Input/系统输入桥.h"

//游戏引擎命名空间
using namespace engine;

namespace
{
	//取键类的位域数字
	uint32_t class_of(const Input_Key& key)
	{
		return static_cast<uint32_t>(input_key_class_get(key));
	}

	//取类内编号
	uint32_t code_of(const Input_Key& key)
	{
		return input_key_code_get(key);
	}
}

//键位翻译：主键盘 / 控制 / 功能 / 小键盘 / 鼠标 / 滚轮
TEST(Input_Bridge_Test, 键位翻译)
{
	//字母 W → 主键盘 23（SDL 26 - A(4) + 1）
	Input_Key key = input_bridge::key_of_scancode(SDL_SCANCODE_W);
	EXPECT_EQ(class_of(key), static_cast<uint32_t>(Input_Key_Class::main));
	EXPECT_EQ(code_of(key), 23u);
	//数字 0 → 主键盘 27（0~9 连续区段，0 在 1 之前）
	key = input_bridge::key_of_scancode(SDL_SCANCODE_0);
	EXPECT_EQ(key, INPUT_KEY_0);
	EXPECT_EQ(code_of(key), 27u);
	//Escape → 控制类 1；左 Shift → 控制类 34
	EXPECT_EQ(code_of(input_bridge::key_of_scancode(SDL_SCANCODE_ESCAPE)), 1u);
	key = input_bridge::key_of_scancode(SDL_SCANCODE_LSHIFT);
	EXPECT_EQ(class_of(key), static_cast<uint32_t>(Input_Key_Class::control));
	EXPECT_EQ(code_of(key), 34u);
	//F5 → 控制类 14
	EXPECT_EQ(code_of(input_bridge::key_of_scancode(SDL_SCANCODE_F5)), 14u);
	//小键盘 1 → 小键盘 2（小键盘 0 为 1）
	key = input_bridge::key_of_scancode(SDL_SCANCODE_KP_1);
	EXPECT_EQ(class_of(key), static_cast<uint32_t>(Input_Key_Class::keypad));
	EXPECT_EQ(code_of(key), 2u);
	//鼠标左键 / 滚轮向上
	key = input_bridge::key_of_mouse_button(1);
	EXPECT_EQ(class_of(key), static_cast<uint32_t>(Input_Key_Class::mouse));
	EXPECT_EQ(code_of(key), 1u);
	key = input_bridge::key_of_wheel(true);
	EXPECT_EQ(class_of(key), static_cast<uint32_t>(Input_Key_Class::wheel));
	EXPECT_EQ(code_of(key), 1u);
	//未登记扫描码返回 0
	EXPECT_EQ(input_bridge::key_of_scancode(SDL_SCANCODE_UNKNOWN), 0u);
}

//平台翻译与命名常量一致（键位编号单一真源在渲染侧的长期守卫）
TEST(Input_Bridge_Test, 翻译与常量一致)
{
	//SDL 扫描码与命名常量的对应项（覆盖五类键位）
	struct Scan_Key_Case
	{
		//平台扫描码
		uint32_t scancode;
		//期望键位
		Input_Key key;
	};
	const Scan_Key_Case cases[]{
		{SDL_SCANCODE_A, INPUT_KEY_A}, {SDL_SCANCODE_Z, INPUT_KEY_Z},
		{SDL_SCANCODE_0, INPUT_KEY_0}, {SDL_SCANCODE_1, INPUT_KEY_1},
		{SDL_SCANCODE_SLASH, INPUT_KEY_SLASH}, {SDL_SCANCODE_GRAVE, INPUT_KEY_GRAVE},
		{SDL_SCANCODE_ESCAPE, INPUT_KEY_ESCAPE}, {SDL_SCANCODE_RETURN, INPUT_KEY_ENTER},
		{SDL_SCANCODE_PAGEUP, INPUT_KEY_PAGE_UP}, {SDL_SCANCODE_DELETE, INPUT_KEY_DELETE},
		{SDL_SCANCODE_F1, INPUT_KEY_F1}, {SDL_SCANCODE_F12, INPUT_KEY_F12},
		{SDL_SCANCODE_LSHIFT, INPUT_KEY_LSHIFT}, {SDL_SCANCODE_RGUI, INPUT_KEY_RGUI},
		{SDL_SCANCODE_KP_0, INPUT_KEY_KP_0}, {SDL_SCANCODE_KP_5, INPUT_KEY_KP_5},
		{SDL_SCANCODE_KP_ENTER, INPUT_KEY_KP_ENTER}
	};

	//逐项翻译并与常量比对
	for (const auto& item : cases)
	{
		EXPECT_EQ(input_bridge::key_of_scancode(item.scancode), item.key)
			<< "扫描码翻译与常量不一致：" << item.scancode;
	}
	//鼠标与滚轮
	EXPECT_EQ(input_bridge::key_of_mouse_button(1), INPUT_KEY_MOUSE_LEFT);
	EXPECT_EQ(input_bridge::key_of_mouse_button(5), INPUT_KEY_MOUSE_X2);
	EXPECT_EQ(input_bridge::key_of_wheel(true), INPUT_KEY_WHEEL_UP);
	EXPECT_EQ(input_bridge::key_of_wheel(false), INPUT_KEY_WHEEL_DOWN);
}

//长按重复沿透传：边沿带 repeat 标记，语义命令是否产出由系统侧绑定决定
TEST(Input_Bridge_Test, 重复沿透传)
{
	//构造一帧：W 重复沿两轮，A 按下
	Input_Frame frame{};
	frame.frame = 3;
	frame.dt = 0.016;
	frame.key_down[SDL_SCANCODE_W] = 1;
	frame.key_pressed.push_back(SDL_SCANCODE_A);
	frame.key_repeat.push_back(SDL_SCANCODE_W);
	frame.key_repeat.push_back(SDL_SCANCODE_W);

	//构造载荷
	nlohmann::json payload;
	ASSERT_TRUE(input_bridge::raw_build(frame, false, payload));

	//边沿总数：A 按下 1 条 + W 重复 2 条
	ASSERT_TRUE(payload["keys"].is_array());
	ASSERT_EQ(payload["keys"].size(), std::size_t{3});
	//首条为 A 按下且不带 repeat
	EXPECT_EQ(code_of(payload["keys"][0]["key"].get<Input_Key>()), 1u);
	EXPECT_FALSE(payload["keys"][0].contains("repeat"));
	//后两条为 W 重复沿且带 repeat
	for (std::size_t index = 1; index < payload["keys"].size(); ++index)
	{
		EXPECT_EQ(code_of(payload["keys"][index]["key"].get<Input_Key>()), 23u);
		EXPECT_TRUE(payload["keys"][index].value("repeat", false));
	}
	//按住快照仍只含 W
	ASSERT_TRUE(payload["held"].is_array());
	EXPECT_EQ(payload["held"].size(), std::size_t{1});
}

//载荷字段：边沿 / 按住快照 / 指针 / 文本模式
TEST(Input_Bridge_Test, 载荷字段)
{
	//构造一帧：W 按下、A 抬起、滚轮向上两齿
	Input_Frame frame{};
	frame.frame = 7;
	frame.dt = 0.016;
	frame.key_down[SDL_SCANCODE_W] = 1;
	frame.key_pressed.push_back(SDL_SCANCODE_W);
	frame.key_released.push_back(SDL_SCANCODE_A);
	frame.pointer.x = 100.0f;
	frame.pointer.y = 50.0f;
	frame.pointer.dx = 4.0f;
	frame.pointer.dy = -2.0f;
	frame.pointer.wheel_up_ticks = 2;

	//构造载荷
	nlohmann::json payload;
	ASSERT_TRUE(input_bridge::raw_build(frame, true, payload));

	//帧号与步长
	EXPECT_EQ(payload["frame"], 7);
	EXPECT_DOUBLE_EQ(payload["dt"].get<double>(), 0.016);
	//边沿：W 按下 1 条 + A 抬起 1 条 + 滚轮 2 齿 × 2 条
	ASSERT_TRUE(payload["keys"].is_array());
	EXPECT_EQ(payload["keys"].size(), std::size_t{6});
	//首条为 W 按下（主键盘 23）
	const nlohmann::json& first = payload["keys"][0];
	EXPECT_EQ(code_of(first["key"].get<Input_Key>()), 23u);
	EXPECT_TRUE(first["pressed"].get<bool>());
	//按住快照只含 W
	ASSERT_TRUE(payload["held"].is_array());
	EXPECT_EQ(payload["held"].size(), std::size_t{1});
	EXPECT_EQ(code_of(payload["held"][0].get<Input_Key>()), 23u);
	//指针
	EXPECT_FLOAT_EQ(payload["pointer"]["x"].get<float>(), 100.0f);
	EXPECT_FLOAT_EQ(payload["pointer"]["dx"].get<float>(), 4.0f);
	//文本模式
	EXPECT_TRUE(payload["text_mode"].get<bool>());
}

//端到端：渲染层构造的 Raw 经真实系统链路产出语义命令
class Input_Bridge_EndToEnd_Test : public ::testing::Test
{
protected:
	//事件中转站
	Event_Broker broker{};
	//输入管理器
	Input_Manager input{};
	//焦点系统
	Focus_System focus{};
	//命令采集终端
	Event_Terminal capture{};
	//采集到的语义命令
	std::vector<std::string> commands{};

	//测试准备：绑定三个终端的中转站端口、接入并订阅命令
	void SetUp(void) override
	{
		//输入管理器端口
		input.event_terminal->bind(Port_ID::Broker_Attacher,
			[this](const std::string& name, const uint64_t& key, auto receiver) -> void
			{ broker.attach(name, key, receiver); });
		input.event_terminal->bind(Port_ID::Evt_Sender,
			[this](std::shared_ptr<Event> evt) -> void { broker.receive(evt); });
		ASSERT_TRUE(input.attach());

		//焦点系统端口
		focus.event_terminal->bind(Port_ID::Broker_Attacher,
			[this](const std::string& name, const uint64_t& key, auto receiver) -> void
			{ broker.attach(name, key, receiver); });
		focus.event_terminal->bind(Port_ID::Evt_Sender,
			[this](std::shared_ptr<Event> evt) -> void { broker.receive(evt); });
		ASSERT_TRUE(focus.attach());

		//采集终端端口与订阅
		auto capture_key = capture.key();
		ASSERT_TRUE(capture_key.has_value());
		Event_Terminal& capture_ref = capture;
		capture_ref->bind(Port_ID::Evt_Receiver,
			[this](std::shared_ptr<Event> evt) -> void
			{ commands.push_back(evt->config.value("command", std::string{})); });
		capture_ref->bind(Port_ID::Broker_Attacher,
			[this](const std::string& name, const uint64_t& key, auto receiver) -> void
			{ broker.attach(name, key, receiver); });
		capture_ref->bind(Port_ID::Evt_Sender,
			[this](std::shared_ptr<Event> evt) -> void { broker.receive(evt); });
		ASSERT_TRUE(capture.attach("Capture", capture_key.value()));

		//订阅 Input/Command
		nlohmann::json sub = nlohmann::json::object();
		sub["object"] = "Capture";
		sub["acl_key"] = static_cast<uint64_t>(capture_key.value());
		nlohmann::json list = nlohmann::json::array();
		nlohmann::json item = nlohmann::json::object();
		item["category"] = "Input";
		item["tag"] = "Command";
		list.push_back(item);
		sub["subscribed_events"] = list;
		auto sub_evt = capture.build("", "", "Subscribed_Events", "Add");
		ASSERT_TRUE(sub_evt);
		sub_evt->config = sub;
		ASSERT_TRUE(capture.send(sub_evt, capture_key.value()));
	}

	//发送输入对象创建事件
	void object_create(const nlohmann::json& config)
	{
		auto evt = std::make_shared<Event>("Test", "Input_Manager", "Input", "Create", config);
		broker.receive(evt);
	}
};

//渲染层 Raw → 焦点系统 → 输入管理器 → 语义命令
TEST_F(Input_Bridge_EndToEnd_Test, 命令产出)
{
	//创建映射对象（W → Move）
	nlohmann::json mapping_item = nlohmann::json::object();
	mapping_item["key"] = "W";
	mapping_item["command"] = "Move";
	nlohmann::json player = nlohmann::json::object();
	player["name"] = "Player";
	player["mapping"] = nlohmann::json::array({ mapping_item });
	object_create(player);
	//首个主对象获焦
	focus.frame_begin();
	ASSERT_EQ(focus.current_main_get(), "Player");

	//构造渲染层帧（W 按下并按住）
	Input_Frame frame{};
	frame.frame = 1;
	frame.dt = 0.016;
	frame.key_down[SDL_SCANCODE_W] = 1;
	frame.key_pressed.push_back(SDL_SCANCODE_W);

	//构造 Raw 载荷并投给焦点系统
	nlohmann::json payload;
	ASSERT_TRUE(input_bridge::raw_build(frame, false, payload));
	auto raw_evt = std::make_shared<Event>("Renderer", "Focus_System", "Input", "Raw", payload);
	broker.receive(raw_evt);

	//应产出一条 Move 命令
	ASSERT_EQ(commands.size(), std::size_t{1});
	EXPECT_EQ(commands.front(), "Move");
}
