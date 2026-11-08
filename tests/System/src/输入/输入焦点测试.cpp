/*
输入处理系统与焦点管理系统测试
覆盖：创建与挂载、注入键内部转移、焦点延后一帧、Escape 弹栈、
      监听拷贝、注入冲突丢弃、监听表校验、宿主重载、级联销毁
*/
#include "Engine/EngineFoundation/common/前置头文件包含.h"
#include <gtest/gtest.h>

//获取事件中转器
#include "Engine/EngineFoundation/src/event/Event_Broker/事件中转器.h"
//获取输入处理系统
#include "Engine/EngineSystem/src/input/mapping/Input_Manager/输入管理器.h"
//获取焦点管理系统
#include "Engine/EngineSystem/src/input/focus/Focus_Manager/焦点系统.h"

//游戏引擎命名空间
using namespace engine;

//输入焦点测试夹具
class Input_Focus_Test : public ::testing::Test
{
protected:
	//事件中转站
	Event_Broker broker{};
	//输入处理系统
	Input_Manager input{};
	//焦点管理系统
	Focus_System focus{};

	//测试准备
	void SetUp(void) override
	{
		//绑定输入系统中转站接入端口
		input.event_terminal->bind(Port_ID::Broker_Attacher,
			[this](const std::string& name, const uint64_t& key, auto receiver) -> void
			{
				this->broker.attach(name, key, receiver);
			});
		//绑定输入系统事件发送端口（订阅登记需要）
		input.event_terminal->bind(Port_ID::Evt_Sender,
			[this](std::shared_ptr<Event> evt) -> void
			{
				this->broker.receive(evt);
			});
		//接入输入系统
		ASSERT_TRUE(input.attach());

		//绑定焦点系统中转站接入端口
		focus.event_terminal->bind(Port_ID::Broker_Attacher,
			[this](const std::string& name, const uint64_t& key, auto receiver) -> void
			{
				this->broker.attach(name, key, receiver);
			});
		//绑定焦点系统事件发送端口
		focus.event_terminal->bind(Port_ID::Evt_Sender,
			[this](std::shared_ptr<Event> evt) -> void
			{
				this->broker.receive(evt);
			});
		//接入焦点系统
		ASSERT_TRUE(focus.attach());
	}

	//构造映射项
	static nlohmann::json binding_make(const std::string& key, const std::string& command,
		const std::string& target = {})
	{
		//映射项
		nlohmann::json item = nlohmann::json::object();
		item["key"] = key;
		item["command"] = command;
		//若指定目标对象
		if (!target.empty())
			item["target"] = target;
		return item;
	}

	//发送对象创建事件
	void object_create(const nlohmann::json& config)
	{
		auto evt = std::make_shared<Event>("Test", "Input_Manager", "Input", "Create", config);
		broker.receive(evt);
	}

	//发送对象销毁事件
	void object_destroy(const std::string& name)
	{
		nlohmann::json config = nlohmann::json::object();
		config["object"] = name;
		auto evt = std::make_shared<Event>("Test", "Input_Manager", "Input", "Destroy", config);
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

	//发送原始输入
	void raw_input(const nlohmann::json& keys, const uint64_t& frame = 1)
	{
		nlohmann::json config = nlohmann::json::object();
		config["frame"] = frame;
		config["keys"] = keys;
		auto evt = std::make_shared<Event>("Renderer", "Focus_System", "Input", "Raw", config);
		broker.receive(evt);
	}

	//发送鼠标焦点事件
	void mouse_focus(const std::string& name)
	{
		nlohmann::json config = nlohmann::json::object();
		config["object"] = name;
		auto evt = std::make_shared<Event>("UI", "Focus_System", "Input", "MouseFocus", config);
		broker.receive(evt);
	}

	//发送原始输入帧（完整载荷）
	void raw_frame(const nlohmann::json& config)
	{
		auto evt = std::make_shared<Event>("Renderer", "Focus_System", "Input", "Raw", config);
		broker.receive(evt);
	}

	//发送模态压栈
	void modal_push(const std::string& name)
	{
		nlohmann::json config = nlohmann::json::object();
		config["object"] = name;
		auto evt = std::make_shared<Event>("UI", "Focus_System", "Input", "ModalPush", config);
		broker.receive(evt);
	}

	//发送模态弹栈
	void modal_pop(void)
	{
		auto evt = std::make_shared<Event>("UI", "Focus_System", "Input", "ModalPop",
			nlohmann::json::object());
		broker.receive(evt);
	}

	//发送重载事件（任意字段）
	void object_reload_raw(const nlohmann::json& config)
	{
		auto evt = std::make_shared<Event>("Test", "Input_Manager", "Input", "Reload", config);
		broker.receive(evt);
	}

	//发送配置加载事件
	void config_load(const nlohmann::json& config)
	{
		auto evt = std::make_shared<Event>("Test", "Input_Manager", "Config", "Load", config);
		broker.receive(evt);
	}

	//构造单条按键边沿
	static nlohmann::json key_edge(const std::string& key, bool pressed = true)
	{
		nlohmann::json edge = nlohmann::json::object();
		edge["key"] = key;
		edge["pressed"] = pressed;
		return edge;
	}
};

//创建与挂载：注入的 Enter 键让宿主把焦点交给子对象，且转移延后一帧
TEST_F(Input_Focus_Test, 创建挂载与子对象内部转移)
{
	//创建主对象
	nlohmann::json player = nlohmann::json::object();
	player["name"] = "Player";
	player["mapping"] = nlohmann::json::array({ binding_make("W", "Move"), binding_make("Escape", "Escape") });
	object_create(player);
	//应用首个主对象默认焦点
	focus.frame_begin();
	ASSERT_EQ(focus.current_main_get(), "Player");

	//创建挂载到主对象的子对象，并注入 Enter 键
	nlohmann::json backpack = nlohmann::json::object();
	backpack["name"] = "Backpack";
	backpack["host"] = "Player";
	backpack["added_keys"] = nlohmann::json::array({ binding_make("E", "Enter", "Backpack") });
	object_create(backpack);
	ASSERT_TRUE(input.object_exist("Backpack"));
	ASSERT_TRUE(input.object_is_child_of("Backpack", "Player"));
	ASSERT_EQ(input.object_injected_count("Backpack"), std::size_t{1});

	//发送注入键的按下沿
	nlohmann::json keys = nlohmann::json::array({ key_edge("E") });
	raw_input(keys);
	//转移尚未生效
	EXPECT_EQ(focus.current_child_get(), "");
	//帧首应用后子对象接管
	focus.frame_begin();
	EXPECT_EQ(focus.current_main_get(), "Player");
	EXPECT_EQ(focus.current_child_get(), "Backpack");
}

//Escape 弹栈：鼠标命中另一主对象压栈，Escape 回到上一个主对象
TEST_F(Input_Focus_Test, Escape弹主对象栈)
{
	//创建主对象甲
	nlohmann::json player = nlohmann::json::object();
	player["name"] = "Player";
	player["mapping"] = nlohmann::json::array({ binding_make("Escape", "Escape") });
	object_create(player);
	focus.frame_begin();
	ASSERT_EQ(focus.current_main_get(), "Player");
	ASSERT_EQ(focus.stack_size(), std::size_t{1});

	//创建主对象乙
	nlohmann::json pause = nlohmann::json::object();
	pause["name"] = "Pause";
	pause["mapping"] = nlohmann::json::array({ binding_make("Escape", "Escape") });
	object_create(pause);

	//鼠标命中主对象乙
	mouse_focus("Pause");
	focus.frame_begin();
	ASSERT_EQ(focus.current_main_get(), "Pause");
	ASSERT_EQ(focus.stack_size(), std::size_t{2});

	//发送 Escape
	nlohmann::json keys = nlohmann::json::array({ key_edge("Escape") });
	raw_input(keys);
	//弹栈尚未生效
	EXPECT_EQ(focus.current_main_get(), "Pause");
	//帧首应用后回到主对象甲
	focus.frame_begin();
	EXPECT_EQ(focus.current_main_get(), "Player");
	EXPECT_EQ(focus.stack_size(), std::size_t{1});
}

//监听拷贝：子对象监听宿主键位，命中后宿主照常处理，子对象另行映射
TEST_F(Input_Focus_Test, 监听表拷贝输入)
{
	//创建主对象甲
	nlohmann::json player = nlohmann::json::object();
	player["name"] = "Player";
	object_create(player);
	focus.frame_begin();
	ASSERT_EQ(focus.current_main_get(), "Player");

	//创建主对象乙并携带 W 映射
	nlohmann::json overlay = nlohmann::json::object();
	overlay["name"] = "Overlay";
	overlay["mapping"] = nlohmann::json::array({ binding_make("W", "Move") });
	object_create(overlay);

	//创建子对象：监听 W，并自行把 W 映射为 Escape
	nlohmann::json hud = nlohmann::json::object();
	hud["name"] = "Hud";
	hud["host"] = "Overlay";
	hud["listen"] = nlohmann::json::array({ "W" });
	hud["mapping"] = nlohmann::json::array({ binding_make("W", "Escape") });
	object_create(hud);
	ASSERT_EQ(input.object_listening_count("Hud"), std::size_t{1});

	//切到主对象乙
	mouse_focus("Overlay");
	focus.frame_begin();
	ASSERT_EQ(focus.current_main_get(), "Overlay");

	//发送 W：宿主处理 Move，监听的子对象处理 Escape
	nlohmann::json keys = nlohmann::json::array({ key_edge("W") });
	raw_input(keys);
	//子对象产出的 Escape 触发弹栈
	focus.frame_begin();
	EXPECT_EQ(focus.current_main_get(), "Player");
	EXPECT_EQ(focus.current_child_get(), "");
}

//注入冲突：宿主优先，子对象之间先注册者优先
TEST_F(Input_Focus_Test, 注入键冲突丢弃)
{
	//创建宿主
	nlohmann::json host = nlohmann::json::object();
	host["name"] = "Host";
	host["mapping"] = nlohmann::json::array({ binding_make("W", "Move"),
		binding_make("E", "Enter", "Host") });
	object_create(host);

	//子对象甲：注入键与宿主冲突
	nlohmann::json child_a = nlohmann::json::object();
	child_a["name"] = "ChildA";
	child_a["host"] = "Host";
	child_a["added_keys"] = nlohmann::json::array({ binding_make("W", "Enter", "ChildA") });
	object_create(child_a);
	EXPECT_EQ(input.object_injected_count("ChildA"), std::size_t{0});

	//子对象乙：注入键不冲突，生效
	nlohmann::json child_b = nlohmann::json::object();
	child_b["name"] = "ChildB";
	child_b["host"] = "Host";
	child_b["added_keys"] = nlohmann::json::array({ binding_make("F", "Enter", "ChildB") });
	object_create(child_b);
	EXPECT_EQ(input.object_injected_count("ChildB"), std::size_t{1});

	//子对象丙：注入键与先注册的子对象乙冲突
	nlohmann::json child_c = nlohmann::json::object();
	child_c["name"] = "ChildC";
	child_c["host"] = "Host";
	child_c["added_keys"] = nlohmann::json::array({ binding_make("F", "Enter", "ChildC") });
	object_create(child_c);
	EXPECT_EQ(input.object_injected_count("ChildC"), std::size_t{0});
}

//监听表校验与宿主重载：只保留宿主自身映射表存在的键位，并重解析注入键
TEST_F(Input_Focus_Test, 监听表校验与宿主重载)
{
	//创建宿主，仅登记 A
	nlohmann::json host = nlohmann::json::object();
	host["name"] = "Host";
	host["mapping"] = nlohmann::json::array({ binding_make("A", "Move") });
	object_create(host);

	//子对象：监听 A 与宿主不存在的 C，并注入 B
	nlohmann::json child = nlohmann::json::object();
	child["name"] = "Child";
	child["host"] = "Host";
	child["listen"] = nlohmann::json::array({ "A", "C" });
	child["added_keys"] = nlohmann::json::array({ binding_make("B", "Enter", "Child") });
	object_create(child);
	//C 被丢弃，仅保留 A
	EXPECT_EQ(input.object_listening_count("Child"), std::size_t{1});
	//B 未与宿主冲突，注入生效
	EXPECT_EQ(input.object_injected_count("Child"), std::size_t{1});

	//宿主重载：把 B 纳入自身映射表
	object_reload("Host", nlohmann::json::array({ binding_make("A", "Move"), binding_make("B", "Move") }));
	//宿主优先：B 的注入被丢弃
	EXPECT_EQ(input.object_injected_count("Child"), std::size_t{0});

	//宿主重载为空表：B 重新可用，A 监听失效
	object_reload("Host", nlohmann::json::array());
	EXPECT_EQ(input.object_injected_count("Child"), std::size_t{1});
	EXPECT_EQ(input.object_listening_count("Child"), std::size_t{0});
}

//级联销毁：宿主卸载带走子对象，焦点栈同步清理
TEST_F(Input_Focus_Test, 级联销毁与焦点清理)
{
	//创建两个主对象
	nlohmann::json player = nlohmann::json::object();
	player["name"] = "Player";
	object_create(player);
	focus.frame_begin();

	nlohmann::json pause = nlohmann::json::object();
	pause["name"] = "Pause";
	object_create(pause);

	//挂载子对象
	nlohmann::json hud = nlohmann::json::object();
	hud["name"] = "Hud";
	hud["host"] = "Pause";
	object_create(hud);
	ASSERT_TRUE(input.object_exist("Hud"));

	//切到 Pause
	mouse_focus("Pause");
	focus.frame_begin();
	ASSERT_EQ(focus.current_main_get(), "Pause");
	ASSERT_EQ(focus.stack_size(), std::size_t{2});

	//销毁 Pause：Hud 一并销毁，焦点回到 Player
	object_destroy("Pause");
	EXPECT_FALSE(input.object_exist("Hud"));
	EXPECT_FALSE(input.object_exist("Pause"));
	EXPECT_EQ(focus.current_main_get(), "Player");
	EXPECT_EQ(focus.stack_size(), std::size_t{1});
}

//鼠标命中宿主：焦点从子对象回到主对象自身（延后一帧生效）
TEST_F(Input_Focus_Test, 鼠标命中宿主回到主对象)
{
	//创建主对象
	nlohmann::json player = nlohmann::json::object();
	player["name"] = "Player";
	object_create(player);
	focus.frame_begin();
	ASSERT_EQ(focus.current_main_get(), "Player");

	//挂载子对象并让子对象接管
	nlohmann::json hud = nlohmann::json::object();
	hud["name"] = "Hud";
	hud["host"] = "Player";
	object_create(hud);
	mouse_focus("Hud");
	focus.frame_begin();
	ASSERT_EQ(focus.current_child_get(), "Hud");

	//鼠标命中宿主自身
	mouse_focus("Player");
	//清空尚未生效
	EXPECT_EQ(focus.current_child_get(), "Hud");
	focus.frame_begin();
	//帧首应用后回到宿主
	EXPECT_EQ(focus.current_child_get(), "");
	EXPECT_EQ(focus.current_main_get(), "Player");
}

//模态闸门：闸门外的主对象与子对象均被拒绝
TEST_F(Input_Focus_Test, 模态闸门阻断闸门外焦点)
{
	//创建主对象甲
	nlohmann::json player = nlohmann::json::object();
	player["name"] = "Player";
	object_create(player);
	focus.frame_begin();
	ASSERT_EQ(focus.current_main_get(), "Player");

	//创建主对象乙与子对象
	nlohmann::json pause = nlohmann::json::object();
	pause["name"] = "Pause";
	object_create(pause);
	nlohmann::json hud = nlohmann::json::object();
	hud["name"] = "Hud";
	hud["host"] = "Pause";
	object_create(hud);

	//切到 Pause 并开模态
	mouse_focus("Pause");
	focus.frame_begin();
	ASSERT_EQ(focus.current_main_get(), "Pause");
	modal_push("Pause");
	EXPECT_EQ(focus.modal_size(), std::size_t{1});

	//闸门外主对象被拒绝
	mouse_focus("Player");
	focus.frame_begin();
	EXPECT_EQ(focus.current_main_get(), "Pause");
	EXPECT_EQ(focus.current_child_get(), "");

	//闸门内子对象允许接管
	mouse_focus("Hud");
	focus.frame_begin();
	EXPECT_EQ(focus.current_child_get(), "Hud");

	//弹栈后恢复放行
	modal_pop();
	EXPECT_EQ(focus.modal_size(), std::size_t{0});
	mouse_focus("Player");
	focus.frame_begin();
	EXPECT_EQ(focus.current_main_get(), "Player");
}

//不可聚焦对象：注入的 Enter 与鼠标命中均被拒绝
TEST_F(Input_Focus_Test, 不可聚焦对象拒绝转移)
{
	//创建宿主
	nlohmann::json host = nlohmann::json::object();
	host["name"] = "Host";
	host["mapping"] = nlohmann::json::array({ binding_make("W", "Move") });
	object_create(host);
	focus.frame_begin();
	ASSERT_EQ(focus.current_main_get(), "Host");

	//创建不可聚焦子对象并注入 Enter
	nlohmann::json child = nlohmann::json::object();
	child["name"] = "Child";
	child["host"] = "Host";
	child["focusable"] = false;
	child["added_keys"] = nlohmann::json::array({ binding_make("E", "Enter", "Child") });
	object_create(child);
	ASSERT_FALSE(input.object_focusable_get("Child"));

	//注入键触发 Enter 也不得转移
	nlohmann::json keys = nlohmann::json::array({ key_edge("E") });
	raw_input(keys);
	focus.frame_begin();
	EXPECT_EQ(focus.current_child_get(), "");

	//鼠标命中同样被拒绝
	mouse_focus("Child");
	focus.frame_begin();
	EXPECT_EQ(focus.current_child_get(), "");
}

//禁用对象：原始输入被丢弃；重载启用后恢复
TEST_F(Input_Focus_Test, 禁用对象拒绝输入与能力同步)
{
	//创建主对象甲
	nlohmann::json player = nlohmann::json::object();
	player["name"] = "Player";
	player["mapping"] = nlohmann::json::array({ binding_make("Escape", "Escape") });
	object_create(player);
	focus.frame_begin();

	//创建禁用主对象乙
	nlohmann::json locked = nlohmann::json::object();
	locked["name"] = "Locked";
	locked["enabled"] = false;
	locked["mapping"] = nlohmann::json::array({ binding_make("E", "Escape") });
	object_create(locked);
	mouse_focus("Locked");
	focus.frame_begin();
	ASSERT_EQ(focus.current_main_get(), "Locked");
	ASSERT_FALSE(input.object_enabled_get("Locked"));

	//禁用时原始输入被丢弃，Escape 不弹栈
	nlohmann::json keys = nlohmann::json::array({ key_edge("E") });
	raw_input(keys);
	focus.frame_begin();
	EXPECT_EQ(focus.current_main_get(), "Locked");

	//重载为启用
	nlohmann::json reload = nlohmann::json::object();
	reload["object"] = "Locked";
	reload["enabled"] = true;
	object_reload_raw(reload);
	EXPECT_TRUE(input.object_enabled_get("Locked"));

	//启用后 Escape 生效
	raw_input(keys);
	focus.frame_begin();
	EXPECT_EQ(focus.current_main_get(), "Player");
}

//轴与指针映射：位移型按缩放钳制，速率型乘时间步长
TEST_F(Input_Focus_Test, 轴与指针映射)
{
	//创建带轴绑定的对象
	nlohmann::json camera = nlohmann::json::object();
	camera["name"] = "Camera";
	nlohmann::json axes = nlohmann::json::array();
	nlohmann::json look_x = nlohmann::json::object();
	look_x["axis"] = "Look/X";
	look_x["source"] = "pointer.dx";
	look_x["mode"] = "displacement";
	look_x["scale"] = 0.1;
	look_x["clamp"] = 1.0;
	axes.push_back(look_x);
	nlohmann::json look_y = nlohmann::json::object();
	look_y["axis"] = "Look/Y";
	look_y["source"] = "pointer.dy";
	look_y["mode"] = "rate";
	look_y["scale"] = 0.5;
	look_y["clamp"] = 1.0;
	axes.push_back(look_y);
	camera["axes"] = axes;
	object_create(camera);
	focus.frame_begin();

	//发送带指针位移的原始帧
	nlohmann::json raw = nlohmann::json::object();
	raw["frame"] = 1;
	raw["dt"] = 0.1;
	raw["keys"] = nlohmann::json::array();
	nlohmann::json pointer = nlohmann::json::object();
	pointer["dx"] = 10.0;
	pointer["dy"] = 4.0;
	raw["pointer"] = pointer;
	raw_frame(raw);

	//位移型：10 × 0.1 = 1.0（钳制到 1）
	EXPECT_DOUBLE_EQ(input.object_axis_value_get("Camera", "Look/X"), 1.0);
	//速率型：4 × 0.5 = 2.0（钳制到 1）
	EXPECT_DOUBLE_EQ(input.object_axis_value_get("Camera", "Look/Y"), 1.0);

	//死区：重载为 0.5 后小幅位移归零
	nlohmann::json reload = nlohmann::json::object();
	reload["object"] = "Camera";
	nlohmann::json deadzone_axes = nlohmann::json::array();
	nlohmann::json deadzone_item = nlohmann::json::object();
	deadzone_item["axis"] = "Look/X";
	deadzone_item["source"] = "pointer.dx";
	deadzone_item["mode"] = "displacement";
	deadzone_item["scale"] = 0.1;
	deadzone_item["deadzone"] = 0.5;
	deadzone_item["clamp"] = 1.0;
	deadzone_axes.push_back(deadzone_item);
	reload["axes"] = deadzone_axes;
	object_reload_raw(reload);

	nlohmann::json raw_small = nlohmann::json::object();
	raw_small["frame"] = 2;
	raw_small["dt"] = 0.1;
	raw_small["keys"] = nlohmann::json::array();
	nlohmann::json pointer_small = nlohmann::json::object();
	pointer_small["dx"] = 0.3;
	raw_small["pointer"] = pointer_small;
	raw_frame(raw_small);
	EXPECT_DOUBLE_EQ(input.object_axis_value_get("Camera", "Look/X"), 0.0);
}

//配置加载接线：Config/Load 创建对象；二次加载按名更新
TEST_F(Input_Focus_Test, 配置加载接线)
{
	//首轮配置
	nlohmann::json config = nlohmann::json::object();
	nlohmann::json objects = nlohmann::json::array();
	nlohmann::json main_item = nlohmann::json::object();
	main_item["name"] = "CfgMain";
	main_item["mapping"] = nlohmann::json::array({ binding_make("Escape", "Escape") });
	objects.push_back(main_item);
	nlohmann::json child_item = nlohmann::json::object();
	child_item["name"] = "CfgChild";
	child_item["host"] = "CfgMain";
	child_item["added_keys"] = nlohmann::json::array({ binding_make("E", "Enter", "CfgChild") });
	objects.push_back(child_item);
	config["objects"] = objects;
	config_load(config);

	EXPECT_TRUE(input.object_exist("CfgMain"));
	EXPECT_TRUE(input.object_exist("CfgChild"));
	EXPECT_TRUE(input.object_is_child_of("CfgChild", "CfgMain"));

	//二次配置：按名更新能力位
	nlohmann::json config_again = nlohmann::json::object();
	nlohmann::json objects_again = nlohmann::json::array();
	nlohmann::json child_again = nlohmann::json::object();
	child_again["name"] = "CfgChild";
	child_again["focusable"] = false;
	objects_again.push_back(child_again);
	config_again["objects"] = objects_again;
	config_load(config_again);

	EXPECT_TRUE(input.object_exist("CfgChild"));
	EXPECT_FALSE(input.object_focusable_get("CfgChild"));
}

//文本模式：键位映射挂起，切回普通对象后恢复
TEST_F(Input_Focus_Test, 文本模式挂起键位映射)
{
	//创建普通主对象
	nlohmann::json player = nlohmann::json::object();
	player["name"] = "Player";
	player["mapping"] = nlohmann::json::array({ binding_make("Escape", "Escape") });
	object_create(player);
	focus.frame_begin();
	ASSERT_EQ(focus.current_main_get(), "Player");

	//创建文本接收主对象
	nlohmann::json text_box = nlohmann::json::object();
	text_box["name"] = "TextBox";
	text_box["text_sink"] = true;
	text_box["mapping"] = nlohmann::json::array({ binding_make("Escape", "Escape") });
	object_create(text_box);
	mouse_focus("TextBox");
	focus.frame_begin();
	ASSERT_EQ(focus.current_main_get(), "TextBox");
	ASSERT_TRUE(focus.text_mode_get());
	ASSERT_TRUE(input.object_text_sink_get("TextBox"));

	//文本模式下 Escape 不进映射，不弹栈
	nlohmann::json keys = nlohmann::json::array({ key_edge("Escape") });
	raw_input(keys);
	focus.frame_begin();
	EXPECT_EQ(focus.current_main_get(), "TextBox");

	//切回普通主对象，文本模式关闭
	mouse_focus("Player");
	focus.frame_begin();
	EXPECT_FALSE(focus.text_mode_get());
	EXPECT_EQ(focus.current_main_get(), "Player");

	//此时 Escape 恢复映射，弹栈回到 TextBox
	raw_input(keys);
	focus.frame_begin();
	EXPECT_EQ(focus.current_main_get(), "TextBox");
}
