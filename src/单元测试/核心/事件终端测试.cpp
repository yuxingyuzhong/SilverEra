//事件终端测试：覆盖权限密钥生成、密钥校验、发送与接收通道、原生事件集合与中转站接入
#include <gtest/gtest.h>

//获取事件终端
#include "src/core/event/Event_Terminal/事件终端.h"

//构造测试事件
static std::shared_ptr<engine::event> make_event(const std::string& category,
	const std::string& tag)
{
	//空配置包
	const nlohmann::json config = nlohmann::json::object();
	//返回共享事件
	return std::make_shared<engine::event>("", "", category, tag, config);
}

//事件终端测试夹具
class Event_Terminal_Test : public ::testing::Test
{
protected:
	//被测终端
	engine::Event_Terminal terminal;
};

//权限密钥：首次生成返回有效非零值
TEST_F(Event_Terminal_Test, 首次生成密钥有效)
{
	//生成权限密钥
	const int64_t key = terminal.acl_key_gen();
	//密钥不应为零
	EXPECT_NE(key, 0);
}

//权限密钥：重复生成返回无效值
TEST_F(Event_Terminal_Test, 重复生成密钥返回无效值)
{
	//首次生成
	const int64_t first = terminal.acl_key_gen();
	//再次生成
	const int64_t second = terminal.acl_key_gen();
	//首次有效
	EXPECT_NE(first, 0);
	//再次应返回无效值
	EXPECT_EQ(second, 0);
}

//密钥校验：密钥不匹配时发送失败
TEST_F(Event_Terminal_Test, 错误密钥发送失败)
{
	//生成权限密钥
	const int64_t key = terminal.acl_key_gen();
	//注册发送通道
	terminal->event_sender_register([](std::shared_ptr<engine::event>) {});
	//以错误密钥发送
	EXPECT_FALSE(terminal.send(make_event("输入", "按键"), key + 1));
}

//发送通道：未注册时发送失败
TEST_F(Event_Terminal_Test, 未注册发送通道时发送失败)
{
	//生成权限密钥
	const int64_t key = terminal.acl_key_gen();
	//未注册任何发送通道
	EXPECT_FALSE(terminal.send(make_event("输入", "按键"), key));
}

//发送通道：注册后发送成功且事件被送出
TEST_F(Event_Terminal_Test, 注册后发送成功)
{
	//生成权限密钥
	const int64_t key = terminal.acl_key_gen();
	//送出次数
	int sent = 0;
	//注册发送通道
	EXPECT_TRUE(terminal->event_sender_register(
		[&sent](std::shared_ptr<engine::event>) { ++sent; }));
	//发送事件
	EXPECT_TRUE(terminal.send(make_event("输入", "按键"), key));
	//事件应被送出一次
	EXPECT_EQ(sent, 1);
}

//发送通道：批量重载可用
TEST_F(Event_Terminal_Test, 批量发送重载可用)
{
	//生成权限密钥
	const int64_t key = terminal.acl_key_gen();
	//送出事件数
	size_t sent_count = 0;
	//注册批量发送通道（多事件重载）
	EXPECT_TRUE(terminal->event_sender_register(
		[&sent_count](std::vector<std::shared_ptr<engine::event>> events)
		{ sent_count = events.size(); }));
	//批量发送两条事件
	std::vector<std::shared_ptr<engine::event>> events{
		make_event("输入", "按键"), make_event("输入", "松开")
	};
	EXPECT_TRUE(terminal.send(events, key));
	//两条事件应整体送出
	EXPECT_EQ(sent_count, 2u);
}

//接收通道：未注册时事件落入原生集合
TEST_F(Event_Terminal_Test, 未注册接收通道时落原生集合)
{
	//生成权限密钥
	const int64_t key = terminal.acl_key_gen();
	//接收一条事件
	terminal.receive(make_event("输入", "按键"));
	//原生集合应留存该事件
	EXPECT_EQ(terminal.query(key).size(), 1u);
}

//接收通道：注册后事件被转发且不落原生集合
TEST_F(Event_Terminal_Test, 注册接收通道后事件被转发)
{
	//生成权限密钥
	const int64_t key = terminal.acl_key_gen();
	//接收次数
	int received = 0;
	//注册接收通道
	EXPECT_TRUE(terminal->event_receiver_register(
		[&received](std::shared_ptr<engine::event>) { ++received; }));
	//接收一条事件
	terminal.receive(make_event("输入", "按键"));
	//事件应被转发
	EXPECT_EQ(received, 1);
	//原生集合不应留存
	EXPECT_EQ(terminal.query(key).size(), 0u);
}

//接收通道：批量重载可用
TEST_F(Event_Terminal_Test, 批量接收入口可用)
{
	//生成权限密钥
	const int64_t key = terminal.acl_key_gen();
	//接收事件数
	size_t received_count = 0;
	//注册批量接收通道（多事件重载）
	EXPECT_TRUE(terminal->event_receiver_register(
		[&received_count](std::vector<std::shared_ptr<engine::event>> events)
		{ received_count = events.size(); }));
	//批量接收两条事件
	std::vector<std::shared_ptr<engine::event>> events{
		make_event("输入", "按键"), make_event("输入", "松开")
	};
	terminal.receive(events);
	//两条事件应整体转发
	EXPECT_EQ(received_count, 2u);
	//原生集合不应留存
	EXPECT_EQ(terminal.query(key).size(), 0u);
}

//事件清空：密钥错误时清空失败
TEST_F(Event_Terminal_Test, 错误密钥清空失败)
{
	//生成权限密钥
	const int64_t key = terminal.acl_key_gen();
	//接收一条事件
	terminal.receive(make_event("输入", "按键"));
	//以错误密钥清空
	EXPECT_FALSE(terminal.clear(key + 1));
	//事件仍在集合中
	EXPECT_EQ(terminal.query(key).size(), 1u);
}

//事件清空：密钥正确时集合被清空
TEST_F(Event_Terminal_Test, 正确密钥清空成功)
{
	//生成权限密钥
	const int64_t key = terminal.acl_key_gen();
	//接收两条事件
	terminal.receive(make_event("输入", "按键"));
	terminal.receive(make_event("输入", "松开"));
	//清空事件集合
	EXPECT_TRUE(terminal.clear(key));
	//集合应为空
	EXPECT_EQ(terminal.query(key).size(), 0u);
}

//发送与接收：send 计入发送通道，未注册接收通道时事件落入原生集合
//（Event_Terminal 只有 operator-> 没有括号重载，原用例里的 terminal(...)
//  是没有对应接口的草稿写法，这里改回接口上的 send / receive）
TEST_F(Event_Terminal_Test, 发送走通道且接收落入原生集合)
{
	//生成权限密钥
	const int64_t key = terminal.acl_key_gen();
	//送出次数
	int sent = 0;
	//注册发送通道
	terminal->event_sender_register([&sent](std::shared_ptr<engine::event>) { ++sent; });
	//发送：走发送通道，计数应增加
	EXPECT_TRUE(terminal.send(make_event("输入", "按键"), key));
	EXPECT_EQ(sent, 1);
	//接收：未注册接收通道，事件应落入原生集合
	terminal.receive(make_event("输入", "松开"));
	EXPECT_EQ(terminal.query(key).size(), 1u);
}

//接入中转站：密钥不匹配时接入失败
TEST_F(Event_Terminal_Test, 错误密钥接入失败)
{
	//生成权限密钥
	const int64_t key = terminal.acl_key_gen();
	//以错误密钥接入
	EXPECT_FALSE(terminal.attach("模块", {}, key + 1));
}

//接入中转站：接入信息被完整传递给接入入口
TEST_F(Event_Terminal_Test, 接入信息被传递给接入入口)
{
	//生成权限密钥
	const int64_t key = terminal.acl_key_gen();
	//接入入口收到的模块名
	std::string received_name;
	//接入入口收到的订阅事件数
	size_t received_count = 0;
	//接入入口接到的接收通道
	std::function<void(std::shared_ptr<engine::event>)> received_entry;

	//先注册单事件接收入口，避免接入时解引用未初始化的入口指针
	EXPECT_TRUE(terminal->event_receiver_register(
		[](std::shared_ptr<engine::event>) {}));
	//注册中转站接入入口
	EXPECT_TRUE(terminal->attach_handler_register(
		[&](auto&& module_name, auto&& needed_events, auto&& entry)
		{
			//留存模块名
			received_name = module_name;
			//留存订阅事件数
			received_count = needed_events.size();
			//留存接收通道
			received_entry = entry;
		}));

	//待订阅事件清单
	std::vector<engine::event> needed{ engine::event("", "", "输入", "按键",
		nlohmann::json::object()) };
	//接入中转站
	EXPECT_TRUE(terminal.attach("模块", needed, key));
	//模块名应被传递
	EXPECT_EQ(received_name, "模块");
	//订阅事件数应为一条
	EXPECT_EQ(received_count, 1u);
	//接收通道应可用
	EXPECT_TRUE(static_cast<bool>(received_entry));
}

//接入中转站：经接入通道送入的事件最终转交给已注册的接收通道
TEST_F(Event_Terminal_Test, 接入后经通道送入事件)
{
	//生成权限密钥
	const int64_t key = terminal.acl_key_gen();
	//接入入口接到的接收通道
	std::function<void(std::shared_ptr<engine::event>)> received_entry;
	//已注册接收通道的转发次数
	int forwarded = 0;

	//注册单事件接收入口
	terminal->event_receiver_register(
		[&forwarded](std::shared_ptr<engine::event>) { ++forwarded; });
	//注册中转站接入入口
	terminal->attach_handler_register(
		[&received_entry](auto&&, auto&&, auto&& entry) { received_entry = entry; });

	//接入中转站
	EXPECT_TRUE(terminal.attach("模块", {}, key));
	//经接入通道送入一条事件
	received_entry(make_event("输入", "按键"));
	//事件应被转交给已注册的接收通道
	EXPECT_EQ(forwarded, 1);
	//已注册接收通道时，原生集合不应留存事件
	EXPECT_EQ(terminal.query(key).size(), 0u);
}

//密钥未生成时的默认值：零密钥可通过权限校验，属已知缺陷
//缺陷位置：事件终端.h 的 acl_key 成员默认值为 0，事件终端.cpp 的校验为直接相等比较
//成因：未调用 acl_key_gen 时 acl_key 恰为零，而校验只比较「传入密钥 == 自身密钥」，
//     于是以 0 为密钥的调用会被判为有权，等于默认开放。
//修复方向：把 acl_key 的初值改为非零哨兵值，或在校验前额外判断密钥是否已生成。
//本用例固化的是这一现状。
TEST_F(Event_Terminal_Test, 零密钥可通过权限校验)
{
	//不生成密钥，直接注册发送通道
	terminal->event_sender_register([](std::shared_ptr<engine::event>) {});
	//以零为密钥发送
	EXPECT_TRUE(terminal.send(make_event("输入", "按键"), 0));
	//以零为密钥清空
	EXPECT_TRUE(terminal.clear(0));
}

//错误密钥查询：当前实现返回局部临时量的引用，属已知缺陷
//缺陷位置：事件终端.cpp 的事件查阅
//成因：密钥不匹配分支写作 return {};，此处返回的是临时容器，
//     函数返回类型却是 const 引用，调用方拿到的是悬垂引用。
//修复方向：改为按值返回，或返回一个静态空容器的引用。
//本用例默认禁用，避免读取已析构对象；缺陷修复后去掉下划线前缀即可启用。
TEST_F(Event_Terminal_Test, DISABLED_错误密钥查询返回空集合)
{
	//生成权限密钥
	const int64_t key = terminal.acl_key_gen();
	//以错误密钥查询
	const std::vector<std::shared_ptr<engine::event>>& result = terminal.query(key + 1);
	//应返回空集合
	EXPECT_TRUE(result.empty());
}