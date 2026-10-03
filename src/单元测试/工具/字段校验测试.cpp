//字段校验测试：覆盖布尔、整型、浮点、字符串与容器四类萃取的存在性、类型与空值判定
#include <gtest/gtest.h>
#include <string>
#include <vector>

//获取json字段可用性校验工具
#include "Engine/EngineCore/src/tools/Detail/json字段可用性校验.h"

//字段校验测试夹具
class Field_Check_Test : public ::testing::Test
{
public:
	//构造一个包含常见字段的配置对象
	static nlohmann::json make_config(void)
	{
		nlohmann::json config = nlohmann::json::object();
		//布尔字段（真）
		config["flag_on"] = true;
		//布尔字段（假）
		config["flag_off"] = false;
		//整型字段
		config["count"] = 3;
		//整型负值字段
		config["offset"] = -8;
		//浮点字段
		config["scale"] = 1.5;
		//浮点负值字段
		config["bias"] = -0.25;
		//非空字符串字段
		config["name"] = std::string("主炮");
		//空字符串字段
		config["empty_text"] = std::string();
		//非空数组字段
		config["points"] = nlohmann::json::array({ 1, 2, 3 });
		//空数组字段
		config["empty_list"] = nlohmann::json::array();
		//非空对象字段
		config["tuple"] = nlohmann::json::object({ {"X", 1}, {"Y", 2} });
		//空对象字段
		config["empty_obj"] = nlohmann::json::object();
		return config;
	}
};

// ———— 布尔萃取 ————

//布尔字段为真：校验通过
TEST_F(Field_Check_Test, 布尔真值通过)
{
	//被测配置
	const nlohmann::json config = make_config();
	//布尔字段为真
	EXPECT_TRUE(engine::detail::field_check<bool>(config, "flag_on"));
}

//布尔字段为假：取值虽为假但类型正确，校验通过
TEST_F(Field_Check_Test, 布尔假值通过)
{
	//被测配置
	const nlohmann::json config = make_config();
	//布尔字段为假同样属于合法布尔
	EXPECT_TRUE(engine::detail::field_check<bool>(config, "flag_off"));
}

//布尔字段类型不符：整数被拒
TEST_F(Field_Check_Test, 布尔字段为整数被拒)
{
	//被测配置
	const nlohmann::json config = make_config();
	//整型字段按布尔萃取应被拒
	EXPECT_FALSE(engine::detail::field_check<bool>(config, "count"));
}

//布尔字段类型不符：整数零不被当作布尔假
TEST_F(Field_Check_Test, 布尔字段不接受零值整数)
{
	//单独的零值整数配置
	nlohmann::json config = nlohmann::json::object();
	config["zero"] = 0;
	//整数零不等价于布尔假
	EXPECT_FALSE(engine::detail::field_check<bool>(config, "zero"));
}

//布尔字段缺失：校验被拒
TEST_F(Field_Check_Test, 布尔字段缺失被拒)
{
	//被测配置
	const nlohmann::json config = make_config();
	//不存在的字段名
	EXPECT_FALSE(engine::detail::field_check<bool>(config, "no_such_field"));
}

// ———— 整型萃取 ————

//整型字段：正数校验通过
TEST_F(Field_Check_Test, 整型正数通过)
{
	//被测配置
	const nlohmann::json config = make_config();
	//整型字段
	EXPECT_TRUE(engine::detail::field_check<int>(config, "count"));
}

//整型字段：负数校验通过
TEST_F(Field_Check_Test, 整型负数通过)
{
	//被测配置
	const nlohmann::json config = make_config();
	//整型负值字段
	EXPECT_TRUE(engine::detail::field_check<int64_t>(config, "offset"));
}

//整型字段类型不符：浮点被拒
TEST_F(Field_Check_Test, 整型字段为浮点被拒)
{
	//被测配置
	const nlohmann::json config = make_config();
	//浮点字段按整型萃取应被拒
	EXPECT_FALSE(engine::detail::field_check<int>(config, "scale"));
}

//整型字段类型不符：字符串被拒
TEST_F(Field_Check_Test, 整型字段为字符串被拒)
{
	//被测配置
	const nlohmann::json config = make_config();
	//字符串字段按整型萃取应被拒
	EXPECT_FALSE(engine::detail::field_check<int>(config, "name"));
}

//整型字段类型不符：布尔被拒
TEST_F(Field_Check_Test, 整型字段为布尔被拒)
{
	//被测配置
	const nlohmann::json config = make_config();
	//布尔字段按整型萃取应被拒
	EXPECT_FALSE(engine::detail::field_check<int>(config, "flag_on"));
}

//整型字段缺失：校验被拒
TEST_F(Field_Check_Test, 整型字段缺失被拒)
{
	//被测配置
	const nlohmann::json config = make_config();
	//不存在的字段名
	EXPECT_FALSE(engine::detail::field_check<uint32_t>(config, "absent_int"));
}

// ———— 浮点萃取 ————

//浮点字段：正数校验通过
TEST_F(Field_Check_Test, 浮点正数通过)
{
	//被测配置
	const nlohmann::json config = make_config();
	//浮点字段
	EXPECT_TRUE(engine::detail::field_check<double>(config, "scale"));
}

//浮点字段：负数校验通过
TEST_F(Field_Check_Test, 浮点负数通过)
{
	//被测配置
	const nlohmann::json config = make_config();
	//浮点负值字段
	EXPECT_TRUE(engine::detail::field_check<double>(config, "bias"));
}

//浮点字段类型不符：整数被拒
TEST_F(Field_Check_Test, 浮点字段为整数被拒)
{
	//被测配置
	const nlohmann::json config = make_config();
	//整型字段按浮点萃取应被拒
	EXPECT_FALSE(engine::detail::field_check<double>(config, "count"));
}

//浮点字段类型不符：字符串被拒
TEST_F(Field_Check_Test, 浮点字段为字符串被拒)
{
	//被测配置
	const nlohmann::json config = make_config();
	//字符串字段按浮点萃取应被拒
	EXPECT_FALSE(engine::detail::field_check<float>(config, "name"));
}

//浮点字段缺失：校验被拒
TEST_F(Field_Check_Test, 浮点字段缺失被拒)
{
	//被测配置
	const nlohmann::json config = make_config();
	//不存在的字段名
	EXPECT_FALSE(engine::detail::field_check<double>(config, "absent_float"));
}

// ———— 字符串萃取 ————

//字符串字段：内容非空校验通过
TEST_F(Field_Check_Test, 非空字符串通过)
{
	//被测配置
	const nlohmann::json config = make_config();
	//非空字符串字段
	EXPECT_TRUE(engine::detail::field_check<std::string>(config, "name"));
}

//字符串字段：空内容被拒
TEST_F(Field_Check_Test, 空字符串被拒)
{
	//被测配置
	const nlohmann::json config = make_config();
	//空字符串字段
	EXPECT_FALSE(engine::detail::field_check<std::string>(config, "empty_text"));
}

//字符串字段类型不符：数字被拒
TEST_F(Field_Check_Test, 字符串字段为数字被拒)
{
	//被测配置
	const nlohmann::json config = make_config();
	//整型字段按字符串萃取应被拒
	EXPECT_FALSE(engine::detail::field_check<std::string>(config, "count"));
}

//字符串字段缺失：校验被拒
TEST_F(Field_Check_Test, 字符串字段缺失被拒)
{
	//被测配置
	const nlohmann::json config = make_config();
	//不存在的字段名
	EXPECT_FALSE(engine::detail::field_check<std::string>(config, "absent_text"));
}

// ———— 容器萃取 ————

//容器字段：非空数组校验通过
TEST_F(Field_Check_Test, 非空数组通过)
{
	//被测配置
	const nlohmann::json config = make_config();
	//非空数组按整型容器萃取
	EXPECT_TRUE(engine::detail::field_check<std::vector<int>>(config, "points"));
}

//容器字段：空数组被拒
TEST_F(Field_Check_Test, 空数组被拒)
{
	//被测配置
	const nlohmann::json config = make_config();
	//空数组
	EXPECT_FALSE(engine::detail::field_check<std::vector<int>>(config, "empty_list"));
}

//容器字段：非空对象校验通过
TEST_F(Field_Check_Test, 非空对象通过)
{
	//被测配置
	const nlohmann::json config = make_config();
	//非空对象按json萃取（走容器分支）
	EXPECT_TRUE(engine::detail::field_check<nlohmann::json>(config, "tuple"));
}

//容器字段：空对象被拒
TEST_F(Field_Check_Test, 空对象被拒)
{
	//被测配置
	const nlohmann::json config = make_config();
	//空对象
	EXPECT_FALSE(engine::detail::field_check<nlohmann::json>(config, "empty_obj"));
}

//容器字段类型不符：字符串被拒
TEST_F(Field_Check_Test, 容器字段为字符串被拒)
{
	//被测配置
	const nlohmann::json config = make_config();
	//字符串字段按整型容器萃取应被拒
	EXPECT_FALSE(engine::detail::field_check<std::vector<int>>(config, "name"));
}

//容器字段类型不符：数组元素类型不匹配被拒
TEST_F(Field_Check_Test, 数组元素类型不符被拒)
{
	//元素为字符串的数组
	nlohmann::json config = nlohmann::json::object();
	config["names"] = nlohmann::json::array({ "甲", "乙" });
	//按整型容器萃取应被拒
	EXPECT_FALSE(engine::detail::field_check<std::vector<int>>(config, "names"));
}

//容器字段缺失：校验被拒
TEST_F(Field_Check_Test, 容器字段缺失被拒)
{
	//被测配置
	const nlohmann::json config = make_config();
	//不存在的字段名
	EXPECT_FALSE(engine::detail::field_check<std::vector<int>>(config, "absent_list"));
}

// ———— 组合场景 ————

//嵌套对象：内层字段可被单独校验
TEST_F(Field_Check_Test, 嵌套对象内层字段可校验)
{
	//被测配置
	const nlohmann::json config = make_config();
	//取出嵌套对象后校验其内层整型字段
	EXPECT_TRUE(engine::detail::field_check<int>(config.at("tuple"), "X"));
	//取出嵌套对象后校验其内层缺失字段
	EXPECT_FALSE(engine::detail::field_check<int>(config.at("tuple"), "Z"));
}

//空配置对象：任意字段校验均被拒
TEST_F(Field_Check_Test, 空配置任意字段被拒)
{
	//空对象配置
	const nlohmann::json config = nlohmann::json::object();
	//整型字段不存在
	EXPECT_FALSE(engine::detail::field_check<int>(config, "count"));
	//字符串字段不存在
	EXPECT_FALSE(engine::detail::field_check<std::string>(config, "name"));
}

//空字段名：以空串为字段名时被拒
TEST_F(Field_Check_Test, 空字段名被拒)
{
	//被测配置
	const nlohmann::json config = make_config();
	//字段名不存在
	EXPECT_FALSE(engine::detail::field_check<int>(config, ""));
}