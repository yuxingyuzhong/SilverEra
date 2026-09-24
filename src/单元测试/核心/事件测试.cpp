//事件结构体测试：覆盖默认构造、含参构造、相等比较语义与哈希特化
#include <gtest/gtest.h>

//获取事件结构体
#include "src/core/event/Event/事件.h"

//构造测试事件
static engine::event make_event(const std::string& sender_object,
	const std::string& target_object, const std::string& category,
	const std::string& tag, const nlohmann::json& config)
{
	//按含参构造生成事件
	return engine::event(sender_object, target_object, category, tag, config);
}

//事件测试夹具
class Event_Test : public ::testing::Test
{
};

//默认构造：字符串字段为空、配置包为空对象语意的 null
TEST_F(Event_Test, 默认构造各字段为空)
{
	//默认构造的事件
	engine::event evt;
	//发起者为空
	EXPECT_TRUE(evt.sender_object.empty());
	//目标为空
	EXPECT_TRUE(evt.target_object.empty());
	//分类为空
	EXPECT_TRUE(evt.category.empty());
	//标签为空
	EXPECT_TRUE(evt.tag.empty());
	//配置包为空
	EXPECT_TRUE(evt.config.is_null());
}

//含参构造：五个字段按顺序赋值
TEST_F(Event_Test, 含参构造按序赋值)
{
	//带配置包的事件
	const engine::event evt = make_event("发送者", "接收者", "输入", "按键",
		nlohmann::json::object({ {"键码", 65} }));
	//发起者被赋值
	EXPECT_EQ(evt.sender_object, "发送者");
	//目标被赋值
	EXPECT_EQ(evt.target_object, "接收者");
	//分类被赋值
	EXPECT_EQ(evt.category, "输入");
	//标签被赋值
	EXPECT_EQ(evt.tag, "按键");
	//配置包内容被赋值
	EXPECT_EQ(evt.config["键码"], 65);
}

//相等比较：四项关键字段一致即相等
TEST_F(Event_Test, 关键字段一致即相等)
{
	//两份同内容事件
	const engine::event first = make_event("甲", "乙", "输入", "按键", nlohmann::json::object());
	const engine::event second = make_event("甲", "乙", "输入", "按键", nlohmann::json::object());
	//应判定相等
	EXPECT_TRUE(first == second);
}

//相等比较：发起者不参与比较
TEST_F(Event_Test, 发起者不参与相等比较)
{
	//仅发起者不同
	const engine::event first = make_event("甲", "乙", "输入", "按键", nlohmann::json::object());
	const engine::event second = make_event("丙", "乙", "输入", "按键", nlohmann::json::object());
	//仍判定相等
	EXPECT_TRUE(first == second);
}

//相等比较：分类不同即不相等
TEST_F(Event_Test, 分类不同不相等)
{
	//分类不同
	const engine::event first = make_event("甲", "乙", "输入", "按键", nlohmann::json::object());
	const engine::event second = make_event("甲", "乙", "输出", "按键", nlohmann::json::object());
	//应判定不相等
	EXPECT_FALSE(first == second);
}

//相等比较：标签不同即不相等
TEST_F(Event_Test, 标签不同不相等)
{
	//标签不同
	const engine::event first = make_event("甲", "乙", "输入", "按键", nlohmann::json::object());
	const engine::event second = make_event("甲", "乙", "输入", "松开", nlohmann::json::object());
	//应判定不相等
	EXPECT_FALSE(first == second);
}

//相等比较：目标不同即不相等
TEST_F(Event_Test, 目标不同不相等)
{
	//目标不同
	const engine::event first = make_event("甲", "乙", "输入", "按键", nlohmann::json::object());
	const engine::event second = make_event("甲", "丙", "输入", "按键", nlohmann::json::object());
	//应判定不相等
	EXPECT_FALSE(first == second);
}

//相等比较：配置包不同即不相等
TEST_F(Event_Test, 配置包不同不相等)
{
	//配置包内容不同
	const engine::event first = make_event("甲", "乙", "输入", "按键",
		nlohmann::json::object({ {"键码", 65} }));
	const engine::event second = make_event("甲", "乙", "输入", "按键",
		nlohmann::json::object({ {"键码", 66} }));
	//应判定不相等
	EXPECT_FALSE(first == second);
}

//哈希一致：相等事件哈希值相同
TEST_F(Event_Test, 相等事件哈希一致)
{
	//两份同内容事件
	const engine::event first = make_event("甲", "乙", "输入", "按键",
		nlohmann::json::object({ {"键码", 65} }));
	const engine::event second = make_event("甲", "乙", "输入", "按键",
		nlohmann::json::object({ {"键码", 65} }));
	//哈希值应一致
	EXPECT_EQ(std::hash<engine::event>{}(first), std::hash<engine::event>{}(second));
}

//哈希特化：哈希同样忽略发起者
TEST_F(Event_Test, 哈希忽略发起者)
{
	//仅发起者不同
	const engine::event first = make_event("甲", "乙", "输入", "按键", nlohmann::json::object());
	const engine::event second = make_event("丙", "乙", "输入", "按键", nlohmann::json::object());
	//哈希值应一致
	EXPECT_EQ(std::hash<engine::event>{}(first), std::hash<engine::event>{}(second));
}

//哈希容器：相等事件在无序集合中只保留一份
TEST_F(Event_Test, 无序集合去重)
{
	//以事件为键的无序集合
	std::unordered_set<engine::event> event_set;
	//插入两份同内容事件
	event_set.insert(make_event("甲", "乙", "输入", "按键", nlohmann::json::object()));
	event_set.insert(make_event("甲", "乙", "输入", "按键", nlohmann::json::object()));
	//集合规模应为一
	EXPECT_EQ(event_set.size(), 1u);
}

//哈希容器：不同标签互不覆盖
TEST_F(Event_Test, 无序集合区分标签)
{
	//以事件为键的无序集合
	std::unordered_set<engine::event> event_set;
	//插入两份标签不同的事件
	event_set.insert(make_event("甲", "乙", "输入", "按键", nlohmann::json::object()));
	event_set.insert(make_event("甲", "乙", "输入", "松开", nlohmann::json::object()));
	//集合规模应为二
	EXPECT_EQ(event_set.size(), 2u);
}

//配置包可承载嵌套结构：多层内容原样保留
TEST_F(Event_Test, 配置包支持嵌套结构)
{
	//含嵌套数组与对象的配置包
	nlohmann::json config = nlohmann::json::object();
	config["位置"] = nlohmann::json::object({ {"x", 1.5}, {"y", -2.5} });
	config["路径"] = std::vector<std::string>{ "起点", "终点" };
	//构造携带该配置包的事件
	const engine::event evt = make_event("甲", "乙", "移动", "寻路", config);
	//嵌套数值应保留
	EXPECT_DOUBLE_EQ(evt.config["位置"]["x"].get<double>(), 1.5);
	//嵌套数组应保留
	EXPECT_EQ(evt.config["路径"].size(), 2u);
	//中文内容应保留
	EXPECT_EQ(evt.config["路径"][1].get<std::string>(), "终点");
}