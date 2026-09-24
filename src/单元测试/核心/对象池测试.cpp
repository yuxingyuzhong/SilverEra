//对象池测试：覆盖稳定模式下的对象新建、ID分配、索引复用、查找、卸载，以及排序模式（现状不可用，见下）
#include <gtest/gtest.h>

//获取对象池
#include "src/core/object/Object_Pool/对象池.h"

//派生测试对象
class Test_Object : public engine::Object
{
public:
	//附带字段，供排序投影使用
	uint64_t payload = 0;
};

//对象池测试夹具
class Object_Pool_Test : public ::testing::Test
{
};

//对象新建：返回非零ID（零号ID为非法实体预留）
TEST_F(Object_Pool_Test, 新建对象返回非零ID)
{
	//默认构造的对象池
	engine::Object_Pool<Test_Object> pool;
	//新建一个对象
	const uint64_t ID = pool.build();
	//ID 不应为零
	EXPECT_NE(ID, 0u);
}

//对象新建：新对象被标记有效且数据可写
TEST_F(Object_Pool_Test, 新建对象标记有效)
{
	//默认构造的对象池
	engine::Object_Pool<Test_Object> pool;
	//新建一个对象
	const uint64_t ID = pool.build();
	//查找目标对象
	const auto it = pool.find(ID);
	//应命中
	ASSERT_NE(it, pool.end());
	//对象应被标记有效
	EXPECT_TRUE(it->valid());
	//对象ID应与返回ID一致
	EXPECT_EQ(it->ID(), ID);
	//派生字段应可写可读
	it->payload = 42;
	EXPECT_EQ(pool.find(ID)->payload, 42u);
}

//对象新建：多次新建的ID互不相同
TEST_F(Object_Pool_Test, 多次新建ID互不相同)
{
	//默认构造的对象池
	engine::Object_Pool<Test_Object> pool;
	//连续新建三个对象
	const uint64_t first = pool.build();
	const uint64_t second = pool.build();
	const uint64_t third = pool.build();
	//三个ID应互不相同
	EXPECT_NE(first, second);
	EXPECT_NE(second, third);
	EXPECT_NE(first, third);
	//容器规模应为三
	EXPECT_EQ(pool.data().size(), 3u);
}

//对象查找：不存在的ID返回超尾迭代器
TEST_F(Object_Pool_Test, 查找不存在ID返回超尾)
{
	//默认构造的对象池
	engine::Object_Pool<Test_Object> pool;
	//新建一个对象
	pool.build();
	//查找一个未分配过的ID
	EXPECT_EQ(pool.find(9999), pool.end());
}

//对象卸载：卸载后查找返回超尾
TEST_F(Object_Pool_Test, 卸载后查找返回超尾)
{
	//默认构造的对象池
	engine::Object_Pool<Test_Object> pool;
	//新建两个对象
	const uint64_t first = pool.build();
	const uint64_t second = pool.build();
	//卸载第一个对象
	pool.unload(first);
	//已卸载对象应查不到
	EXPECT_EQ(pool.find(first), pool.end());
	//其余对象不受影响
	EXPECT_NE(pool.find(second), pool.end());
}

//对象卸载：卸载只清有效标记，记录本身留在容器中
TEST_F(Object_Pool_Test, 卸载仅清有效标记)
{
	//默认构造的对象池
	engine::Object_Pool<Test_Object> pool;
	//新建一个对象
	const uint64_t ID = pool.build();
	//卸载该对象
	pool.unload(ID);
	//容器规模不变
	ASSERT_EQ(pool.data().size(), 1u);
	//记录仍在容器中
	EXPECT_EQ(pool.data()[0].ID(), ID);
	//但有效标记已被清除
	EXPECT_FALSE(pool.data()[0].valid());
}

//对象卸载：卸载不存在的ID只告警不抛异常
TEST_F(Object_Pool_Test, 卸载不存在ID不抛异常)
{
	//默认构造的对象池
	engine::Object_Pool<Test_Object> pool;
	//新建一个对象
	pool.build();
	//卸载一个未分配过的ID
	EXPECT_NO_THROW(pool.unload(8888));
	//原有对象不受影响
	EXPECT_EQ(pool.data().size(), 1u);
}

//对象卸载：多对象重载逐个卸载
TEST_F(Object_Pool_Test, 多对象卸载重载)
{
	//默认构造的对象池
	engine::Object_Pool<Test_Object> pool;
	//新建三个对象
	const uint64_t first = pool.build();
	const uint64_t second = pool.build();
	const uint64_t third = pool.build();
	//一次性卸载前两个
	pool.unload(std::vector<uint64_t>{ first, second });
	//前两个应查不到
	EXPECT_EQ(pool.find(first), pool.end());
	EXPECT_EQ(pool.find(second), pool.end());
	//第三个仍可查找到
	EXPECT_NE(pool.find(third), pool.end());
}

//索引复用：卸载后新建会复用被回收的位置，且新对象可正常查找
//注意：稳定模式下该路径会读取联合体中未激活的 min_valid_index 成员，属未定义行为，
//     因此这里只用「不抛异常」与可查找性做约束，不对内部索引取值下断言。
TEST_F(Object_Pool_Test, 卸载后新建复用索引)
{
	//默认构造的对象池
	engine::Object_Pool<Test_Object> pool;
	//新建两个对象
	const uint64_t first = pool.build();
	pool.build();
	//卸载第一个对象
	pool.unload(first);
	//再次新建（应复用被回收的索引）
	uint64_t reused = 0;
	EXPECT_NO_THROW(reused = pool.build());
	//新对象应可查找到
	EXPECT_NE(pool.find(reused), pool.end());
	//新对象ID不应为零
	EXPECT_NE(reused, 0u);
}

//非整数Key：当前实现不可编译，属已知缺陷，无法写成运行期用例
//缺陷位置：对象池.h 的 find
//成因：整数Key与非整数Key的分支判定写作运行期 if (!is_sorted && is_key_integral)，
//     两个分支都会被实例化，其中整数分支的 object_index_map.find(key) 要求 Key 可转为 uint64_t；
//     于是 Object_Pool<T, std::string> 只要调用 find 就会编译失败（C2665），
//     模板参数 Key 名义上可选非整数，实际不可用。
//修复方向：把分支判定换成 if constexpr，使不可达分支不参与实例化。

//排序模式：整体不可用，属已知缺陷，以下用例全部禁用
//缺陷位置：对象池.h 的 sort_order_set
//成因：对象索引映射与排序定位信息（投影字段、比较方式、有效索引起点）共用同一块联合体存储。
//     首次调用时先析构索引映射、原地构造投影字段，紧接着却去读同一块存储上的
//     min_valid_index.has_value()；此时该标志位来自 std::function 的内部字节，并非由 optional 构造，
//     实测会被判为「已有值」，于是整段「计算有效索引起点」的初始化被跳过，
//     随后 objects.begin() + min_valid_index.value() 直接使用这块未初始化内存。
//实测表现：无论对象池是否为空，首次设置排序方式都会让 MSVC 的调试迭代器断言
//     "cannot seek vector iterator after end"，进程被直接终止。
//修复方向：把三个排序定位字段从联合体中拆出来单独存放，或改为显式的模式标记加独立成员，
//     使「是否已计算有效索引起点」不再依赖未构造对象的标志位。
//下面用例在被修复前一律不执行；修复后去掉下划线前缀即为排序模式的回归用例。

//排序模式：设置排序方式后仍可按ID查找
TEST_F(Object_Pool_Test, DISABLED_排序模式按ID查找)
{
	//默认构造的对象池
	engine::Object_Pool<Test_Object> pool;
	//新建三个对象
	pool.build();
	const uint64_t second = pool.build();
	pool.build();
	//以ID为投影字段升序排列
	pool.sort_order_set(false, [](const Test_Object& object) { return object.ID(); });
	//目标对象应可查找到
	const auto it = pool.find(second);
	ASSERT_NE(it, pool.end());
	//命中的对象ID应与查找键一致
	EXPECT_EQ(it->ID(), second);
}

//排序模式：降序排列同样可按ID查找
TEST_F(Object_Pool_Test, DISABLED_排序模式降序查找)
{
	//默认构造的对象池
	engine::Object_Pool<Test_Object> pool;
	//新建三个对象
	pool.build();
	const uint64_t second = pool.build();
	pool.build();
	//以ID为投影字段降序排列
	pool.sort_order_set(true, [](const Test_Object& object) { return object.ID(); });
	//目标对象应可查找到
	EXPECT_NE(pool.find(second), pool.end());
}

//排序模式：按自定义投影字段查找
TEST_F(Object_Pool_Test, DISABLED_排序模式按投影字段查找)
{
	//默认构造的对象池
	engine::Object_Pool<Test_Object> pool;
	//新建三个对象并写入互不相同的投影值
	const uint64_t first = pool.build();
	const uint64_t second = pool.build();
	const uint64_t third = pool.build();
	pool.find(first)->payload = 30;
	pool.find(second)->payload = 10;
	pool.find(third)->payload = 20;
	//以派生字段为投影字段升序排列
	pool.sort_order_set(false, [](const Test_Object& object) { return object.payload; });
	//按投影值查找
	const auto it = pool.find(10);
	//应命中且落在预期对象上
	ASSERT_NE(it, pool.end());
	EXPECT_EQ(it->ID(), second);
	//按另一个投影值查找
	const auto other = pool.find(30);
	ASSERT_NE(other, pool.end());
	EXPECT_EQ(other->ID(), first);
}

//排序模式：卸载后查找返回超尾
TEST_F(Object_Pool_Test, DISABLED_排序模式卸载后不可查找)
{
	//默认构造的对象池
	engine::Object_Pool<Test_Object> pool;
	//新建三个对象
	pool.build();
	const uint64_t second = pool.build();
	const uint64_t third = pool.build();
	//以ID为投影字段升序排列
	pool.sort_order_set(false, [](const Test_Object& object) { return object.ID(); });
	//卸载中间对象
	pool.unload(second);
	//已卸载对象应查不到
	EXPECT_EQ(pool.find(second), pool.end());
	//其余对象仍可查找到
	EXPECT_NE(pool.find(third), pool.end());
}

//排序模式：新建对象后仍应能找到它
TEST_F(Object_Pool_Test, DISABLED_排序模式新建后可查找)
{
	//默认构造的对象池
	engine::Object_Pool<Test_Object> pool;
	//新建两个对象
	pool.build();
	pool.build();
	//以ID为投影字段降序排列
	pool.sort_order_set(true, [](const Test_Object& object) { return object.ID(); });
	//排序模式下新建一个对象
	const uint64_t newest = pool.build();
	//新对象应可查找到
	EXPECT_NE(pool.find(newest), pool.end());
}

//排序重置：重置后可按ID查找到原有对象
TEST_F(Object_Pool_Test, DISABLED_排序重置后原对象可查找)
{
	//默认构造的对象池
	engine::Object_Pool<Test_Object> pool;
	//新建两个对象
	const uint64_t first = pool.build();
	const uint64_t second = pool.build();
	//以ID为投影字段升序排列
	pool.sort_order_set(false, [](const Test_Object& object) { return object.ID(); });
	//重置排列方式
	pool.sort_order_reset();
	//两个对象都应可查找到
	EXPECT_NE(pool.find(first), pool.end());
	EXPECT_NE(pool.find(second), pool.end());
}

//排序重置：已卸载对象会被重新写回索引映射
//缺陷位置：对象池.h 的 sort_order_reset
//成因：重建索引映射时以 ID() > 0 判定记录有效，而卸载只清有效标记、保留对象 ID，
//     于是排序模式下卸载过的记录会被重新写回索引映射，查找时又能命中。
//修复方向：改用 valid() 判定记录有效，或在卸载时把对象 ID 一并清零。
TEST_F(Object_Pool_Test, DISABLED_重置排序后已卸载对象仍被命中)
{
	//默认构造的对象池
	engine::Object_Pool<Test_Object> pool;
	//新建两个对象
	const uint64_t first = pool.build();
	pool.build();
	//以ID为投影字段升序排列
	pool.sort_order_set(false, [](const Test_Object& object) { return object.ID(); });
	//卸载首个对象
	pool.unload(first);
	//重置排列方式
	pool.sort_order_reset();
	//期望：已卸载对象查不到（当前实现仍会命中）
	EXPECT_EQ(pool.find(first), pool.end());
}

//空池排序：没有记录可排时不应改动对象池
TEST_F(Object_Pool_Test, DISABLED_空池设置排序方式)
{
	//默认构造的对象池（不含任何对象）
	engine::Object_Pool<Test_Object> pool;
	//设置排序方式
	pool.sort_order_set(false, [](const Test_Object& object) { return object.ID(); });
	//记录数量应保持不变
	EXPECT_TRUE(pool.data().empty());
}

//对象清除：两个模式下的分支写反，属已知缺陷，用例全部禁用
//缺陷位置：对象池.h 的 clear
//成因：条件写作 if (is_sorted) 清空索引映射，而稳定模式下索引映射才是激活成员；
//     于是稳定模式会去改写未激活的投影字段，排序模式反过来对未激活的映射调用 clear。
//     两种模式都会踩到联合体中未构造的成员，属未定义行为，实际执行极可能直接崩溃。
//修复方向：与 sort_order_set 一并把排序定位字段从联合体中拆出来，让两侧分支各自作用于正确成员。
//下面用例在被修复前一律不执行。

//对象清除：稳定模式下清空
TEST_F(Object_Pool_Test, DISABLED_稳定模式清空对象池)
{
	//默认构造的对象池
	engine::Object_Pool<Test_Object> pool;
	//新建两个对象
	pool.build();
	pool.build();
	//清空对象池
	pool.clear();
	//容器应为空
	EXPECT_TRUE(pool.data().empty());
}

//对象清除：排序模式下清空
TEST_F(Object_Pool_Test, DISABLED_排序模式清空对象池)
{
	//默认构造的对象池
	engine::Object_Pool<Test_Object> pool;
	//新建两个对象
	pool.build();
	pool.build();
	//以ID为投影字段升序排列
	pool.sort_order_set(false, [](const Test_Object& object) { return object.ID(); });
	//清空对象池
	pool.clear();
	//容器应为空
	EXPECT_TRUE(pool.data().empty());
}
