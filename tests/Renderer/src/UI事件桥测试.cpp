/*
UI 事件桥测试（L1 无 GPU）
覆盖：事件系统接入与订阅、UI 命中 → MouseFocus → 焦点转移与文本模式、
      模态压栈/弹栈归属、系统语义命令 → UI 控件激活、UI 文本经系统文本会话回传落文本、
      FocusChanged 映射到控件系统焦点。
*/
#include <gtest/gtest.h>
#include <SDL3/SDL.h>

//获取事件中转器、输入管理器与焦点系统
#include "Engine/EngineFoundation/src/event/Event_Broker/事件中转器.h"
#include "Engine/EngineSystem/src/input/mapping/Input_Manager/输入管理器.h"
#include "Engine/EngineSystem/src/input/focus/Focus_Manager/焦点系统.h"
//获取 UI 控件树与事件桥
#include "Engine/EngineRenderer/src/render/UI/用户界面.h"
#include "Engine/EngineRenderer/src/render/UI/UI事件桥.h"

//游戏引擎命名空间
using namespace engine;

namespace
{
	//空回调计数
	struct Action_Counter
	{
		//触发次数
		uint32_t count = 0;
	};

	//按钮点击回调
	void on_click(Ui_Handle, void* user)
	{
		static_cast<Action_Counter*>(user)->count += 1;
	}
}

//UI 事件桥夹具
class Ui_Event_Bridge_Test : public ::testing::Test
{
protected:
	//事件中转站
	Event_Broker broker{};
	//输入管理器
	Input_Manager input{};
	//焦点系统
	Focus_System focus{};
	//UI 控件树
	Ui_Context ui{};
	//UI 事件桥
	Ui_Event_Bridge bridge{};

	//测试准备
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

		//事件桥端口（接收入口由桥自行绑定）
		Event_Terminal& terminal = bridge.event_terminal;
		terminal->bind(Port_ID::Broker_Attacher,
			[this](const std::string& name, const uint64_t& key, auto receiver) -> void
			{ broker.attach(name, key, receiver); });
		terminal->bind(Port_ID::Evt_Sender,
			[this](std::shared_ptr<Event> evt) -> void { broker.receive(evt); });
		ASSERT_TRUE(bridge.attach("Renderer_UI"));

		//创建游戏主对象（首个主对象默认获焦）
		nlohmann::json mapping_item = nlohmann::json::object();
		mapping_item["key"] = "F";
		mapping_item["command"] = "Open";
		nlohmann::json game = nlohmann::json::object();
		game["name"] = "Game";
		game["mapping"] = nlohmann::json::array({ mapping_item });
		object_create(game);

		//创建 UI 主对象与文本 / 模态子对象
		nlohmann::json ui_main = nlohmann::json::object();
		ui_main["name"] = "UI";
		object_create(ui_main);

		nlohmann::json ui_text = nlohmann::json::object();
		ui_text["name"] = "UI.Text";
		ui_text["host"] = "UI";
		ui_text["text_sink"] = true;
		object_create(ui_text);

		nlohmann::json ui_modal = nlohmann::json::object();
		ui_modal["name"] = "UI.Modal";
		ui_modal["host"] = "UI";
		object_create(ui_modal);

		//焦点系统应用首个主对象
		focus.frame_begin();

		//UI 侧配置
		ui.input_root_name_set("UI");
		ui.input_outside_name_set("Game");
	}

	//发送输入对象创建事件
	void object_create(const nlohmann::json& config)
	{
		auto evt = std::make_shared<Event>("Test", "Input_Manager", "Input", "Create", config);
		broker.receive(evt);
	}

	//创建 UI 宿主下的子输入对象
	void ui_object_create(const std::string& name)
	{
		nlohmann::json config = nlohmann::json::object();
		config["name"] = name;
		config["host"] = "UI";
		object_create(config);
	}

	//构造空帧（窗口尺寸 + 指针位置）
	Input_Frame frame_make(float x = 0.0f, float y = 0.0f) const
	{
		Input_Frame frame{};
		frame.frame = 1;
		frame.dt = 0.016;
		frame.window_width = 320;
		frame.window_height = 180;
		frame.pointer.x = x;
		frame.pointer.y = y;
		return frame;
	}

	/*
	点击指定位置
	命中控件则本地获焦并经事件桥上报 MouseFocus；
	焦点转移由系统在下一帧开头统一应用。
	*/
	void click(float x, float y)
	{
		Input_Frame frame = frame_make(x, y);
		frame.pointer.button_pressed[0] = true;
		ui.frame_begin(frame);
		bridge.frame_publish(frame, ui);
		focus.frame_begin();
		bridge.frame_apply(ui);
	}

	//发送一条输入事件到中转站（模拟其他系统 / 应用层上报）
	void event_post(const std::string& tag, const nlohmann::json& config)
	{
		auto evt = std::make_shared<Event>("Test", "Focus_System", "Input", tag, config);
		broker.receive(evt);
	}
};

//UI 文本框命中 → 焦点系统主/子对象转移 + 文本模式开启 + 系统焦点映射
TEST_F(Ui_Event_Bridge_Test, 命中转移到焦点系统)
{
	//创建已绑定系统输入名的文本框
	Ui_Handle text_box = ui.text_box_create(ui.root_get(), Ui_Rect{ 20.0f, 20.0f, 200.0f, 28.0f }, "");
	ASSERT_NE(text_box, 0u);
	ASSERT_TRUE(ui.input_name_set(text_box, "UI.Text"));

	//点击文本框
	Input_Frame frame = frame_make(30.0f, 30.0f);
	frame.pointer.button_pressed[0] = true;
	ui.frame_begin(frame);
	ASSERT_EQ(ui.pressed_input_name_get(), "UI.Text");
	//发布 MouseFocus（焦点转移在系统侧下一帧生效）
	bridge.frame_publish(frame, ui);
	focus.frame_begin();

	//焦点系统应切到 UI 主对象并接管子对象
	EXPECT_EQ(focus.current_main_get(), "UI");
	EXPECT_EQ(focus.current_child_get(), "UI.Text");
	//文本接收者获焦 → 文本模式开启（TextMode 广播已被事件桥缓存）
	EXPECT_TRUE(bridge.system_text_mode());

	//焦点变更应用到控件
	bridge.frame_apply(ui);
	EXPECT_TRUE(ui.system_focused_get(text_box));
}

//UI 模态压栈 / 弹栈（sender 归属校验）
TEST_F(Ui_Event_Bridge_Test, 模态压栈与弹栈)
{
	//模态面板绑定系统输入名
	Ui_Handle modal = ui.panel_create(ui.root_get(), Ui_Rect{ 20.0f, 20.0f, 200.0f, 120.0f },
		Ui_Color{ 0.1f, 0.1f, 0.2f, 1.0f });
	ASSERT_NE(modal, 0u);
	ASSERT_TRUE(ui.input_name_set(modal, "UI.Modal"));
	ASSERT_TRUE(ui.modal_set(modal));

	//发布模态压栈
	Input_Frame frame = frame_make();
	bridge.frame_publish(frame, ui);
	EXPECT_EQ(focus.modal_size(), std::size_t{1});

	//清除模态 → 弹栈
	ui.modal_clear();
	bridge.frame_publish(frame, ui);
	EXPECT_EQ(focus.modal_size(), std::size_t{0});
}

//系统语义命令 → 绑定控件激活
TEST_F(Ui_Event_Bridge_Test, 命令激活控件)
{
	//按钮绑定 F→Open 命令
	Action_Counter counter{};
	Ui_Handle button = ui.button_create(ui.root_get(), Ui_Rect{ 20.0f, 60.0f, 100.0f, 24.0f },
		"Open", on_click, &counter);
	ASSERT_NE(button, 0u);
	ui.command_bind("Open", button);

	//构造 F 按下帧（Game 为当前主对象）
	Input_Frame frame = frame_make();
	frame.key_down[SDL_SCANCODE_F] = 1;
	frame.key_pressed.push_back(SDL_SCANCODE_F);
	ui.frame_begin(frame);
	bridge.frame_publish(frame, ui);
	//命令在发布期间同步广播，随后应用
	bridge.frame_apply(ui);

	EXPECT_EQ(counter.count, 1u) << "系统命令应激活绑定按钮";
	//已绑定命令被 UI 消费，不再留给应用层
	std::string command{};
	EXPECT_FALSE(bridge.command_pop(command));
}

//UI 文本框提交 → TextSession → 系统文本语义回传 → 落文本
TEST_F(Ui_Event_Bridge_Test, 文本会话回传)
{
	//文本框绑定系统输入名
	Ui_Handle text_box = ui.text_box_create(ui.root_get(), Ui_Rect{ 20.0f, 20.0f, 200.0f, 28.0f }, "");
	ASSERT_NE(text_box, 0u);
	ASSERT_TRUE(ui.input_name_set(text_box, "UI.Text"));

	//先让系统焦点切到文本框并开启文本模式
	Input_Frame click_frame = frame_make(30.0f, 30.0f);
	click_frame.pointer.button_pressed[0] = true;
	ui.frame_begin(click_frame);
	bridge.frame_publish(click_frame, ui);
	focus.frame_begin();
	ASSERT_TRUE(bridge.system_text_mode());

	//下一帧：系统文本模式下提交 "A"
	bridge.text_route_prepare(ui);
	EXPECT_TRUE(ui.text_route_get());
	Input_Frame text_frame = frame_make(30.0f, 30.0f);
	text_frame.text.committed = "A";
	ui.frame_begin(text_frame);
	//路由模式下本地不拼接
	std::string content{};
	ASSERT_TRUE(ui.text_get(text_box, content));
	EXPECT_TRUE(content.empty());
	//发布文本会话并被系统回传
	bridge.frame_publish(text_frame, ui);
	bridge.frame_apply(ui);

	ASSERT_TRUE(ui.text_get(text_box, content));
	EXPECT_EQ(content, "A") << "系统文本语义应回落到焦点文本框";
}

//系统焦点不在 UI 主对象时，控件键盘交互应关闭
TEST_F(Ui_Event_Bridge_Test, 键盘总闸由系统焦点控制)
{
	//创建两个带系统输入名的按钮
	Ui_Handle button_a = ui.button_create(ui.root_get(), Ui_Rect{ 20.0f, 20.0f, 80.0f, 24.0f }, "A");
	Ui_Handle button_b = ui.button_create(ui.root_get(), Ui_Rect{ 20.0f, 60.0f, 80.0f, 24.0f }, "B");
	ASSERT_TRUE(ui.input_name_set(button_a, "UI.A"));
	ASSERT_TRUE(ui.input_name_set(button_b, "UI.B"));
	ui_object_create("UI.A");
	ui_object_create("UI.B");

	//系统焦点仍在 Game：总闸关闭，Tab 不得移动控件焦点
	bridge.text_route_prepare(ui);
	EXPECT_FALSE(ui.keyboard_gate_get());
	Input_Frame tab_frame = frame_make();
	tab_frame.key_pressed.push_back(SDL_SCANCODE_TAB);
	ui.frame_begin(tab_frame);
	EXPECT_FALSE(ui.focused_get(button_a));
	EXPECT_FALSE(ui.focused_get(button_b));

	//点击按钮 A 后 UI 成为系统主对象，总闸开启
	click(30.0f, 30.0f);
	EXPECT_EQ(focus.current_main_get(), "UI");
	bridge.text_route_prepare(ui);
	EXPECT_TRUE(ui.keyboard_gate_get());
}

//Tab 切换本地焦点应上报系统子焦点（双焦点收敛的写入口）
TEST_F(Ui_Event_Bridge_Test, Tab焦点上报系统子焦点)
{
	//创建两个带系统输入名的按钮并点击 A 激活 UI
	Ui_Handle button_a = ui.button_create(ui.root_get(), Ui_Rect{ 20.0f, 20.0f, 80.0f, 24.0f }, "A");
	Ui_Handle button_b = ui.button_create(ui.root_get(), Ui_Rect{ 20.0f, 60.0f, 80.0f, 24.0f }, "B");
	ASSERT_TRUE(ui.input_name_set(button_a, "UI.A"));
	ASSERT_TRUE(ui.input_name_set(button_b, "UI.B"));
	ui_object_create("UI.A");
	ui_object_create("UI.B");
	click(30.0f, 30.0f);
	ASSERT_TRUE(ui.focused_get(button_a));
	ASSERT_EQ(focus.current_child_get(), "UI.A");

	//下一帧 Tab：本地焦点移到 B 并上报
	bridge.text_route_prepare(ui);
	Input_Frame tab_frame = frame_make();
	tab_frame.key_pressed.push_back(SDL_SCANCODE_TAB);
	ui.frame_begin(tab_frame);
	ASSERT_TRUE(ui.focused_get(button_b));
	bridge.frame_publish(tab_frame, ui);
	//系统在下一帧开头应用上报
	focus.frame_begin();
	EXPECT_EQ(focus.current_child_get(), "UI.B");
	//系统回写与本地一致，焦点不再变动
	bridge.frame_apply(ui);
	EXPECT_TRUE(ui.focused_get(button_b));
	//上报已被系统确认，不再重复发送
	EXPECT_EQ(focus.current_child_get(), "UI.B");
}

//系统侧子焦点变化应回写到控件本地焦点
TEST_F(Ui_Event_Bridge_Test, 系统子焦点回写本地焦点)
{
	//创建两个带系统输入名的按钮并点击 A
	Ui_Handle button_a = ui.button_create(ui.root_get(), Ui_Rect{ 20.0f, 20.0f, 80.0f, 24.0f }, "A");
	Ui_Handle button_b = ui.button_create(ui.root_get(), Ui_Rect{ 20.0f, 60.0f, 80.0f, 24.0f }, "B");
	ASSERT_TRUE(ui.input_name_set(button_a, "UI.A"));
	ASSERT_TRUE(ui.input_name_set(button_b, "UI.B"));
	ui_object_create("UI.A");
	ui_object_create("UI.B");
	click(30.0f, 30.0f);
	ASSERT_TRUE(ui.focused_get(button_a));

	//模拟其他来源把系统子焦点改到 B
	nlohmann::json config = nlohmann::json::object();
	config["host"] = "UI";
	config["child"] = "UI.B";
	event_post("ChildFocus", config);
	focus.frame_begin();
	ASSERT_EQ(focus.current_child_get(), "UI.B");

	//回写后本地焦点跟随系统
	bridge.frame_apply(ui);
	EXPECT_TRUE(ui.focused_get(button_b));
	EXPECT_FALSE(ui.focused_get(button_a));
}

//系统焦点离开 UI 主对象时应清除控件本地焦点
TEST_F(Ui_Event_Bridge_Test, 系统焦点离开UI清除本地焦点)
{
	//点击带系统输入名的按钮激活 UI
	Ui_Handle button_a = ui.button_create(ui.root_get(), Ui_Rect{ 20.0f, 20.0f, 80.0f, 24.0f }, "A");
	ASSERT_TRUE(ui.input_name_set(button_a, "UI.A"));
	ui_object_create("UI.A");
	click(30.0f, 30.0f);
	ASSERT_TRUE(ui.focused_get(button_a));
	ASSERT_EQ(focus.current_main_get(), "UI");

	//模拟游戏对象夺回焦点
	nlohmann::json config = nlohmann::json::object();
	config["object"] = "Game";
	event_post("MouseFocus", config);
	focus.frame_begin();
	ASSERT_EQ(focus.current_main_get(), "Game");

	//本地焦点被清除，键盘总闸随之关闭
	bridge.frame_apply(ui);
	EXPECT_FALSE(ui.focused_get(button_a));
	bridge.text_route_prepare(ui);
	EXPECT_FALSE(ui.keyboard_gate_get());
}

//未绑定系统输入名的控件只持有本地焦点，不产生系统子焦点
TEST_F(Ui_Event_Bridge_Test, 未命名控件不参与系统焦点)
{
	//创建未绑定输入名的按钮
	Ui_Handle plain = ui.button_create(ui.root_get(), Ui_Rect{ 20.0f, 20.0f, 80.0f, 24.0f }, "Plain");
	ASSERT_NE(plain, 0u);

	//点击后本地获焦，系统子焦点保持为空（MouseFocus 落到 UI 根名）
	click(30.0f, 30.0f);
	EXPECT_TRUE(ui.focused_get(plain));
	EXPECT_EQ(focus.current_main_get(), "UI");
	EXPECT_TRUE(focus.current_child_get().empty()) << "未命名控件不应产生系统子焦点";
}

//焦点从文本框切到按钮应关闭系统文本模式
TEST_F(Ui_Event_Bridge_Test, 焦点切到按钮关闭文本模式)
{
	//创建文本框与按钮
	Ui_Handle text_box = ui.text_box_create(ui.root_get(), Ui_Rect{ 20.0f, 20.0f, 200.0f, 28.0f }, "");
	Ui_Handle button = ui.button_create(ui.root_get(), Ui_Rect{ 20.0f, 60.0f, 80.0f, 24.0f }, "B");
	ASSERT_TRUE(ui.input_name_set(text_box, "UI.Text"));
	ASSERT_TRUE(ui.input_name_set(button, "UI.B"));
	ui_object_create("UI.B");

	//点击文本框：文本接收者获焦，文本模式开启
	click(30.0f, 30.0f);
	ASSERT_EQ(focus.current_child_get(), "UI.Text");
	EXPECT_TRUE(bridge.system_text_mode());

	//点击按钮：子焦点换人，文本模式关闭
	click(30.0f, 70.0f);
	EXPECT_EQ(focus.current_child_get(), "UI.B");
	EXPECT_FALSE(bridge.system_text_mode());
	EXPECT_TRUE(ui.focused_get(button));
	EXPECT_FALSE(ui.focused_get(text_box));
}
