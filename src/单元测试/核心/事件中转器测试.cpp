//事件中转器测试：覆盖订阅登记、标签匹配、定向投递、发起者排除、重复注册与批量分发
#include <gtest/gtest.h>

//获取事件中转器
#include "src/core/event/Event_Broker/事件中转器.h"

//构造测试事件
static std::shared_ptr<engine::event> make_event(const std::string& sender_object,
	const std::string& target_object, const std::string& category, const std::string& tag)
{
	//空配置包
	const nlohmann::json config = nlohmann::json::object();
	//返回共享事件
	return std::make_shared<engine::event>(sender_object, target_object, category, tag, config);
}

//构造订阅清单
static std::vector<engine::event> make_needed(const std::string& category,
	const std::string& tag)
{
	//单条订阅记录
	return { engine::event("", "", category, tag, nlohmann::json::object()) };
}

//事件中转器测试夹具
class Event_Broker_Test : public ::testing::Test
{
protected:
	//被测中转器
	engine::Event_Broker broker;
};

//订阅登记状态：登记后匹配事件能送达
//（引擎层 e430610 移除了 Event_Broker::target_object_check，登记状态不再可直接查询，
//  此处改为观察可验证的行为：登记后投递一条匹配事件应当送达一次）
TEST_F(Event_Broker_Test, 注册后状态确认)
{
	//收到次数
	int received = 0;
	//登记一个订阅者
	broker.info_register("订阅者", make_needed("输入", "按键"),
		[&received](std::shared_ptr<engine::event>) { ++received; });
	//投递一条匹配事件
	broker.receive(make_event("", "", "输入", "按键"));
	//登记生效：应送达一次
	EXPECT_EQ(received, 1);
}

//订阅登记状态：未登记时投递安静结束
//（同上，target_object_check 已移除；未登记则没有可送达的对象，
//  投递应正常结束而不崩溃——这是该路径现在唯一还能验证的性质）
TEST_F(Event_Broker_Test, 未注册状态确认失败)
{
	//未登记过的名称，直接投递
	EXPECT_NO_THROW(broker.receive(make_event("", "", "输入", "按键")));
}

//标签匹配：同分类同标签的订阅者收到事件
TEST_F(Event_Broker_Test, 标签匹配则投递)
{
	//收到次数
	int received = 0;
	//登记订阅者
	broker.info_register("订阅者", make_needed("输入", "按键"),
		[&received](std::shared_ptr<engine::event>) { ++received; });
	//投递一条匹配事件
	broker.receive(make_event("", "", "输入", "按键"));
	//应收到一次
	EXPECT_EQ(received, 1);
}

//标签不匹配：订阅者收不到事件
TEST_F(Event_Broker_Test, 标签不匹配则不投递)
{
	//收到次数
	int received = 0;
	//登记订阅者
	broker.info_register("订阅者", make_needed("输入", "按键"),
		[&received](std::shared_ptr<engine::event>) { ++received; });
	//投递一条同分类不同标签的事件
	broker.receive(make_event("", "", "输入", "松开"));
	//不应收到
	EXPECT_EQ(received, 0);
}

//分类未注册：事件被直接丢弃
TEST_F(Event_Broker_Test, 未注册分类的事件被丢弃)
{
	//收到次数
	int received = 0;
	//登记订阅者（只订阅输入分类）
	broker.info_register("订阅者", make_needed("输入", "按键"),
		[&received](std::shared_ptr<engine::event>) { ++received; });
	//投递其他分类的事件
	broker.receive(make_event("", "", "物理", "按键"));
	//不应收到
	EXPECT_EQ(received, 0);
}

//空分类事件：被直接丢弃
TEST_F(Event_Broker_Test, 空分类事件被丢弃)
{
	//收到次数
	int received = 0;
	//登记订阅者
	broker.info_register("订阅者", make_needed("输入", "按键"),
		[&received](std::shared_ptr<engine::event>) { ++received; });
	//投递一条无分类事件
	broker.receive(make_event("", "", "", "按键"));
	//不应收到
	EXPECT_EQ(received, 0);
}

//空标签事件：被直接丢弃
TEST_F(Event_Broker_Test, 空标签事件被丢弃)
{
	//收到次数
	int received = 0;
	//登记订阅者
	broker.info_register("订阅者", make_needed("输入", "按键"),
		[&received](std::shared_ptr<engine::event>) { ++received; });
	//投递一条无标签事件
	broker.receive(make_event("", "", "输入", ""));
	//不应收到
	EXPECT_EQ(received, 0);
}

//定向投递：目标已登记时只送给目标
TEST_F(Event_Broker_Test, 定向投递只送目标)
{
	//目标收到次数
	int target_received = 0;
	//旁观者收到次数
	int bystander_received = 0;
	//登记目标与旁观者（订阅同一标签）
	broker.info_register("目标", make_needed("输入", "按键"),
		[&target_received](std::shared_ptr<engine::event>) { ++target_received; });
	broker.info_register("旁观者", make_needed("输入", "按键"),
		[&bystander_received](std::shared_ptr<engine::event>) { ++bystander_received; });
	//投递一条定向事件
	broker.receive(make_event("", "目标", "输入", "按键"));
	//目标应收到
	EXPECT_EQ(target_received, 1);
	//旁观者不应收到
	EXPECT_EQ(bystander_received, 0);
}

//定向投递：目标未登记时回落到标签广播
TEST_F(Event_Broker_Test, 定向目标未登记则广播)
{
	//收到次数
	int received = 0;
	//登记订阅者
	broker.info_register("订阅者", make_needed("输入", "按键"),
		[&received](std::shared_ptr<engine::event>) { ++received; });
	//投递一条目标不存在的定向事件
	broker.receive(make_event("", "不存在的对象", "输入", "按键"));
	//应回落到广播并送达
	EXPECT_EQ(received, 1);
}

//定向投递：直达路径不校验标签
TEST_F(Event_Broker_Test, 定向投递不校验标签)
{
	//收到次数
	int received = 0;
	//登记订阅者（只订阅按键标签）
	broker.info_register("目标", make_needed("输入", "按键"),
		[&received](std::shared_ptr<engine::event>) { ++received; });
	//投递一条标签未被订阅的定向事件
	broker.receive(make_event("", "目标", "输入", "摇杆"));
	//定向路径未做标签过滤，目标仍会收到
	EXPECT_EQ(received, 1);
}

//发起者排除：不把事件回送给发起者
TEST_F(Event_Broker_Test, 发起者不收到自己的事件)
{
	//收到次数
	int received = 0;
	//登记订阅者
	broker.info_register("发送者", make_needed("输入", "按键"),
		[&received](std::shared_ptr<engine::event>) { ++received; });
	//以该名称作为发起者投递
	broker.receive(make_event("发送者", "", "输入", "按键"));
	//自己不应收到
	EXPECT_EQ(received, 0);
}

//发起者排除：发起者未登记时不排除任何人
TEST_F(Event_Broker_Test, 未登记发起者不排除订阅者)
{
	//收到次数
	int received = 0;
	//登记订阅者
	broker.info_register("订阅者", make_needed("输入", "按键"),
		[&received](std::shared_ptr<engine::event>) { ++received; });
	//以未登记名称作为发起者投递
	broker.receive(make_event("路人", "", "输入", "按键"));
	//订阅者应正常收到
	EXPECT_EQ(received, 1);
}

//重复注册：订阅入口被替换为最新一次
TEST_F(Event_Broker_Test, 重复注册替换入口)
{
	//旧入口调用次数
	int old_calls = 0;
	//新入口调用次数
	int new_calls = 0;
	//首次登记
	broker.info_register("订阅者", make_needed("输入", "按键"),
		[&old_calls](std::shared_ptr<engine::event>) { ++old_calls; });
	//二次登记同名订阅者
	broker.info_register("订阅者", make_needed("输入", "按键"),
		[&new_calls](std::shared_ptr<engine::event>) { ++new_calls; });
	//投递一条匹配事件
	broker.receive(make_event("", "", "输入", "按键"));
	//旧入口不应被调用
	EXPECT_EQ(old_calls, 0);
	//新入口应被调用一次
	EXPECT_EQ(new_calls, 1);
}

//重复注册：只投递一次而非累积投递
TEST_F(Event_Broker_Test, 重复注册不产生重复投递)
{
	//收到次数
	int received = 0;
	//连续两次登记同一订阅者与同一标签
	broker.info_register("订阅者", make_needed("输入", "按键"),
		[&received](std::shared_ptr<engine::event>) { ++received; });
	broker.info_register("订阅者", make_needed("输入", "按键"),
		[&received](std::shared_ptr<engine::event>) { ++received; });
	//投递一条匹配事件
	broker.receive(make_event("", "", "输入", "按键"));
	//只应收到一次
	EXPECT_EQ(received, 1);
}

//多订阅者：同一标签的多个订阅者都被送达
TEST_F(Event_Broker_Test, 同标签多订阅者都被送达)
{
	//第一个订阅者收到次数
	int first_received = 0;
	//第二个订阅者收到次数
	int second_received = 0;
	//登记两个订阅者
	broker.info_register("甲", make_needed("输入", "按键"),
		[&first_received](std::shared_ptr<engine::event>) { ++first_received; });
	broker.info_register("乙", make_needed("输入", "按键"),
		[&second_received](std::shared_ptr<engine::event>) { ++second_received; });
	//投递一条匹配事件
	broker.receive(make_event("", "", "输入", "按键"));
	//两者都应收到
	EXPECT_EQ(first_received, 1);
	EXPECT_EQ(second_received, 1);
}

//分类隔离：不同分类的同名标签互不串扰
TEST_F(Event_Broker_Test, 不同分类互不串扰)
{
	//输入分类订阅者收到次数
	int input_received = 0;
	//输出分类订阅者收到次数
	int output_received = 0;
	//登记两个分类的订阅者
	broker.info_register("输入订阅者", make_needed("输入", "按键"),
		[&input_received](std::shared_ptr<engine::event>) { ++input_received; });
	broker.info_register("输出订阅者", make_needed("输出", "按键"),
		[&output_received](std::shared_ptr<engine::event>) { ++output_received; });
	//只投递输入分类事件
	broker.receive(make_event("", "", "输入", "按键"));
	//输入订阅者收到
	EXPECT_EQ(input_received, 1);
	//输出订阅者不受影响
	EXPECT_EQ(output_received, 0);
}

//全订阅标记：以空标签登记即订阅该分类全部标签
TEST_F(Event_Broker_Test, 空标签登记订阅全分类)
{
	//收到次数
	int received = 0;
	//以空标签登记订阅者
	broker.info_register("订阅者", make_needed("输入", ""),
		[&received](std::shared_ptr<engine::event>) { ++received; });
	//投递任意标签的同类事件
	broker.receive(make_event("", "", "输入", "随手写的标签"));
	//应收到
	EXPECT_EQ(received, 1);
}

//空分类登记：登记被跳过但订阅者仍完成映射
TEST_F(Event_Broker_Test, 空分类登记被跳过)
{
	//收到次数
	int received = 0;
	//以空分类登记
	broker.info_register("订阅者", make_needed("", "按键"),
		[&received](std::shared_ptr<engine::event>) { ++received; });
	//订阅者映射的登记状态不再可查（target_object_check 已移除），
	//此处只保留可验证的行为：空分类登记不会带来任何送达
	//投递事件不会送达（无任何分类被登记）
	broker.receive(make_event("", "", "输入", "按键"));
	EXPECT_EQ(received, 0);
}

//批量投递：多事件重载逐条分发
TEST_F(Event_Broker_Test, 批量投递逐条分发)
{
	//收到次数
	int received = 0;
	//登记订阅者
	broker.info_register("订阅者", make_needed("输入", "按键"),
		[&received](std::shared_ptr<engine::event>) { ++received; });
	//批量投递三条匹配事件
	std::vector<std::shared_ptr<engine::event>> events{
		make_event("", "", "输入", "按键"),
		make_event("", "", "输入", "按键"),
		make_event("", "", "输入", "按键")
	};
	broker.receive(events);
	//三条都应被送达
	EXPECT_EQ(received, 3);
}

//批量投递：未匹配事件被跳过而不影响其余事件
TEST_F(Event_Broker_Test, 批量投递跳过未匹配事件)
{
	//收到次数
	int received = 0;
	//登记订阅者
	broker.info_register("订阅者", make_needed("输入", "按键"),
		[&received](std::shared_ptr<engine::event>) { ++received; });
	//批量投递匹配与未匹配混合的事件
	std::vector<std::shared_ptr<engine::event>> events{
		make_event("", "", "输入", "按键"),
		make_event("", "", "输入", "松开"),
		make_event("", "", "输入", "按键")
	};
	broker.receive(events);
	//只有两条匹配事件被送达
	EXPECT_EQ(received, 2);
}

//事件内容透传：订阅者拿到的是同一份事件对象
TEST_F(Event_Broker_Test, 事件内容原样透传)
{
	//接收者留存的配置包
	nlohmann::json captured;
	//登记订阅者并留存配置包
	broker.info_register("订阅者", make_needed("输入", "按键"),
		[&captured](std::shared_ptr<engine::event> evt) { captured = evt->config; });
	//投递带配置包的定向事件
	broker.receive(std::make_shared<engine::event>("", "", "输入", "按键",
		nlohmann::json::object({ {"键码", 87} })));
	//配置包内容应原样保留
	EXPECT_EQ(captured["键码"], 87);
}