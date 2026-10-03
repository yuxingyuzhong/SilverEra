//事件结构体测试：覆盖默认构造、含参构造、相等比较语义与哈希特化
#include <gtest/gtest.h>

//获取事件结构体
#include "Engine/EngineCore/src/core/event/Event/事件.h"

//构造测试事件
static engine::Event make_event(const std::string& sender_object,
	const std::string& target_object, const std::string& category,
	const std::string& tag, const nlohmann::json& config)
{
	//按含参构造生成事件
	return engine::Event(sender_object, target_object, category, tag, config);
}

//事件测试夹具
class Event_Test : public ::testing::Test
{
};

//默认构造：字符串字段为空、配置包为空对象语意的 null
TEST_F(Event_Test, 默认构造各字段为空)
{
	//默认构造的事件
	engine::Event evt;
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
	const engine::Event evt = make_event("发送者", "接收者", "输入", "按键",
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
	const engine::Event first = make_event("甲", "乙", "输入", "按键", nlohmann::json::object());
	const engine::Event second = make_event("甲", "乙", "输入", "按键", nlohmann::json::object());
	//应判定相等
	EXPECT_TRUE(first == second);
}

//相等比较：发起者不参与比较
TEST_F(Event_Test, 发起者不参与相等比较)
{
	//仅发起者不同
	const engine::Event first = make_event("甲", "乙", "输入", "按键", nlohmann::json::object());
	const engine::Event second = make_event("丙", "乙", "输入", "按键", nlohmann::json::object());
	//仍判定相等
	EXPECT_TRUE(first == second);
}

//相等比较：分类不同即不相等
TEST_F(Event_Test, 分类不同不相等)
{
	//分类不同
	const engine::Event first = make_event("甲", "乙", "输入", "按键", nlohmann::json::object());
	const engine::Event second = make_event("甲", "乙", "输出", "按键", nlohmann::json::object());
	//应判定不相等
	EXPECT_FALSE(first == second);
}

//相等比较：标签不同即不相等
TEST_F(Event_Test, 标签不同不相等)
{
	//标签不同
	const engine::Event first = make_event("甲", "乙", "输入", "按键", nlohmann::json::object());
	const engine::Event second = make_event("甲", "乙", "输入", "松开", nlohmann::json::object());
	//应判定不相等
	EXPECT_FALSE(first == second);
}

//相等比较：目标不同即不相等
TEST_F(Event_Test, 目标不同不相等)
{
	//目标不同
	const engine::Event first = make_event("甲", "乙", "输入", "按键", nlohmann::json::object());
	const engine::Event second = make_event("甲", "丙", "输入", "按键", nlohmann::json::object());
	//应判定不相等
	EXPECT_FALSE(first == second);
}

//相等比较：配置包不同即不相等
TEST_F(Event_Test, 配置包不同不相等)
{
	//配置包内容不同
	const engine::Event first = make_event("甲", "乙", "输入", "按键",
		nlohmann::json::object({ {"键码", 65} }));
	const engine::Event second = make_event("甲", "乙", "输入", "按键",
		nlohmann::json::object({ {"键码", 66} }));
	//应判定不相等
	EXPECT_FALSE(first == second);
}

//哈希一致：相等事件哈希值相同
TEST_F(Event_Test, 相等事件哈希一致)
{
	//两份同内容事件
	const engine::Event first = make_event("甲", "乙", "输入", "按键",
		nlohmann::json::object({ {"键码", 65} }));
	const engine::Event second = make_event("甲", "乙", "输入", "按键",
		nlohmann::json::object({ {"键码", 65} }));
	//哈希值应一致
	EXPECT_EQ(std::hash<engine::Event>{}(first), std::hash<engine::Event>{}(second));
}

//哈希特化：哈希同样忽略发起者
TEST_F(Event_Test, 哈希忽略发起者)
{
	//仅发起者不同
	const engine::Event first = make_event("甲", "乙", "输入", "按键", nlohmann::json::object());
	const engine::Event second = make_event("丙", "乙", "输入", "按键", nlohmann::json::object());
	//哈希值应一致
	EXPECT_EQ(std::hash<engine::Event>{}(first), std::hash<engine::Event>{}(second));
}

//哈希容器：相等事件在无序集合中只保留一份
TEST_F(Event_Test, 无序集合去重)
{
	//以事件为键的无序集合
	std::unordered_set<engine::Event> event_set;
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
	std::unordered_set<engine::Event> event_set;
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
	const engine::Event evt = make_event("甲", "乙", "移动", "寻路", config);
	//嵌套数值应保留
	EXPECT_DOUBLE_EQ(evt.config["位置"]["x"].get<double>(), 1.5);
	//嵌套数组应保留
	EXPECT_EQ(evt.config["路径"].size(), 2u);
	//中文内容应保留
	EXPECT_EQ(evt.config["路径"][1].get<std::string>(), "终点");
}

//默认构造：配置包为空语意的 null 而非空对象
TEST_F(Event_Test, 默认构造配置包为空语意)
{
	//默认构造的事件
	engine::Event evt;
	//配置包应为 null
	EXPECT_TRUE(evt.config.is_null());
	//配置包不应为空对象
	EXPECT_FALSE(evt.config.is_object());
}

//两参构造：仅写入分类与标签，其余字段保持默认
TEST_F(Event_Test, 两参构造仅写入分类与标签)
{
	//仅传分类与标签构造
	const engine::Event evt("输入", "按键");
	//分类被赋值
	EXPECT_EQ(evt.category, "输入");
	//标签被赋值
	EXPECT_EQ(evt.tag, "按键");
	//发起者保持为空
	EXPECT_TRUE(evt.sender_object.empty());
	//目标保持为空
	EXPECT_TRUE(evt.target_object.empty());
	//配置包保持为 null
	EXPECT_TRUE(evt.config.is_null());
}

//四参构造：可传入空源与空目标，分类与标签照常写入
TEST_F(Event_Test, 四参构造可传空源与空目标)
{
	//空源与空目标的构造
	const engine::Event evt("", "", "输入", "按键");
	//发起者为空
	EXPECT_TRUE(evt.sender_object.empty());
	//目标为空
	EXPECT_TRUE(evt.target_object.empty());
	//分类被赋值
	EXPECT_EQ(evt.category, "输入");
	//标签被赋值
	EXPECT_EQ(evt.tag, "按键");
	//配置包保持为 null
	EXPECT_TRUE(evt.config.is_null());
}

//五元构造：五个字段与入参逐项一致（含配置包整体相等）
TEST_F(Event_Test, 五元构造各字段写入一致)
{
	//入参配置包
	const nlohmann::json config = nlohmann::json::object({ {"键码", 90} });
	//五元构造事件
	const engine::Event evt("发送者", "接收者", "输入", "按键", config);
	//发起者一致
	EXPECT_EQ(evt.sender_object, "发送者");
	//目标一致
	EXPECT_EQ(evt.target_object, "接收者");
	//分类一致
	EXPECT_EQ(evt.category, "输入");
	//标签一致
	EXPECT_EQ(evt.tag, "按键");
	//配置包整体一致
	EXPECT_EQ(evt.config, config);
}

//载荷为空对象：判定为对象且内容为空，而不同于 null
TEST_F(Event_Test, 载荷为空对象时为对象且为空)
{
	//空对象配置包
	const nlohmann::json config = nlohmann::json::object();
	//构造携带空对象的事件
	const engine::Event evt("甲", "乙", "输入", "按键", config);
	//配置包应为对象
	EXPECT_TRUE(evt.config.is_object());
	//配置包内容应为空
	EXPECT_TRUE(evt.config.empty());
	//配置包不应为 null
	EXPECT_FALSE(evt.config.is_null());
}

//载荷含深层嵌套结构：多层对象与数组原样保留
TEST_F(Event_Test, 载荷含深层嵌套结构可读)
{
	//多层嵌套配置包
	nlohmann::json config = nlohmann::json::object();
	config["层一"]["层二"]["层三"] = 42;
	config["序列"] = nlohmann::json::array({ 1, 2, 3 });
	//构造携带深层嵌套的事件
	const engine::Event evt("甲", "乙", "输入", "按键", config);
	//三层数值应保留
	EXPECT_EQ(evt.config["层一"]["层二"]["层三"], 42);
	//数组规模应保留
	EXPECT_EQ(evt.config["序列"].size(), 3u);
	//数组末元素应保留
	EXPECT_EQ(evt.config["序列"][2], 3);
}

//超长标签：千字节级标签可完整承载
TEST_F(Event_Test, 超长标签可承载)
{
	//千字节级长标签
	const std::string long_tag(1024, 'L');
	//构造携带长标签的事件
	const engine::Event evt("甲", "乙", "输入", long_tag, nlohmann::json::object());
	//标签长度应完整保留
	EXPECT_EQ(evt.tag.size(), 1024u);
	//标签内容应逐字一致
	EXPECT_EQ(evt.tag, long_tag);
}

//中文标签：多字节中文标签可完整承载
TEST_F(Event_Test, 中文标签可承载并保留)
{
	//中文标签事件
	const engine::Event evt("甲", "乙", "输入", "按键按下事件", nlohmann::json::object());
	//中文标签内容应保留
	EXPECT_EQ(evt.tag, "按键按下事件");
	//标签字节长度应与源串一致
	EXPECT_EQ(evt.tag.size(), std::string("按键按下事件").size());
}

//标签互不相同：不同标签各自保留而互不覆盖
TEST_F(Event_Test, 标签互不相同各自保留)
{
	//按键标签事件
	const engine::Event first("甲", "乙", "输入", "按键", nlohmann::json::object());
	//松开标签事件
	const engine::Event second("甲", "乙", "输入", "松开", nlohmann::json::object());
	//首个标签应保留
	EXPECT_EQ(first.tag, "按键");
	//次个标签应保留
	EXPECT_EQ(second.tag, "松开");
	//两者标签不相等
	EXPECT_NE(first.tag, second.tag);
}

//拷贝构造：拷贝件与原件各字段一致
TEST_F(Event_Test, 拷贝构造后字段一致)
{
	//原始事件
	const engine::Event origin("甲", "乙", "输入", "按键",
		nlohmann::json::object({ {"键码", 65} }));
	//拷贝构造副本
	engine::Event copy = origin;
	//发起者一致
	EXPECT_EQ(copy.sender_object, origin.sender_object);
	//目标一致
	EXPECT_EQ(copy.target_object, origin.target_object);
	//分类一致
	EXPECT_EQ(copy.category, origin.category);
	//标签一致
	EXPECT_EQ(copy.tag, origin.tag);
	//配置包一致
	EXPECT_EQ(copy.config, origin.config);
	//副本与原件相等
	EXPECT_TRUE(copy == origin);
}

//载荷改写独立性：改写原件的配置包不影响拷贝件
TEST_F(Event_Test, 载荷改写后两份事件独立)
{
	//原始事件
	engine::Event origin("甲", "乙", "输入", "按键",
		nlohmann::json::object({ {"键码", 65} }));
	//拷贝构造副本
	engine::Event copy = origin;
	//改写原件配置包
	origin.config["键码"] = 99;
	//原件应反映改写
	EXPECT_EQ(origin.config["键码"], 99);
	//副本应保持原值
	EXPECT_EQ(copy.config["键码"], 65);
}

//重复构造同名事件：两份同名事件互相独立，改写其一不影响另一
TEST_F(Event_Test, 重复构造同名事件互相独立)
{
	//首份同名事件
	engine::Event first = make_event("甲", "乙", "输入", "按键", nlohmann::json::object());
	//次份同名事件
	engine::Event second = make_event("甲", "乙", "输入", "按键", nlohmann::json::object());
	//改写首份的标签
	first.tag = "松开";
	//首份应反映改写
	EXPECT_EQ(first.tag, "松开");
	//次份应保持原标签
	EXPECT_EQ(second.tag, "按键");
	//两份已不再相等
	EXPECT_FALSE(first == second);
}