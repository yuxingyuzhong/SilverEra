//对象池测试：覆盖稳定模式下的对象新建、ID分配、索引复用、查找、卸载，以及排序模式与投影字段定位
#include <gtest/gtest.h>

//获取对象池
#include "Engine/EngineCore/src/core/object/Object_Pool/对象池.h"

//派生测试对象
class Test_Object : public engine::Object
{
public:
	//附带字段，供排序投影使用
	uint64_t payload = 0;
};

//派生测试对象(附带字符串定位字段)
class Tagged_Object : public engine::Object
{
public:
	//字符串定位字段
	std::string tag;
};

//定位投影器：以派生字段定位
//（以对象ID定位由对象池默认投影器 Default_Projector 承担，无需另写）
struct Payload_Projector
{
	uint64_t operator()(const Test_Object& object) const
	{
		return object.payload;
	}
};

//定位投影器：以字符串字段定位（键为非整数类型）
struct Tag_Projector
{
	std::string operator()(const Tagged_Object& object) const
	{
		return object.tag;
	}
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

//对象卸载：记录留在容器中，ID 被回收清零
//引擎语义：unload 回收对象 ID 并置零、清有效标记，记录本身仍留在容器里。
TEST_F(Object_Pool_Test, 卸载后记录保留但ID清零)
{
	//默认构造的对象池
	engine::Object_Pool<Test_Object> pool;
	//新建一个对象
	const uint64_t ID = pool.build();
	//卸载该对象
	pool.unload(ID);
	//容器规模不变
	ASSERT_EQ(pool.data().size(), 1u);
	//ID 已被回收清零
	EXPECT_EQ(pool.data()[0].ID(), 0u);
	//有效标记已被清除
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
//稳定模式下卸载会回收容器下标，新建按下标分配器的后进先出机制复用该位置。
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

//定位字段：定位键改为构造时传入的投影字段，定位映射以投影字段为键
//投影字段可为任意可比较类型，非整数键已可用（见文末字符串定位字段用例）

//排序模式：排列方式由 order_set 设置，投影字段在构造时固定
//不再需要随排序方式重复传入投影

//排序模式：设置排序方式后仍可按ID查找
TEST_F(Object_Pool_Test, 排序模式按ID查找)
{
	//默认构造的对象池
	engine::Object_Pool<Test_Object> pool;
	//新建三个对象
	pool.build();
	const uint64_t second = pool.build();
	pool.build();
	//以ID为投影字段升序排列
	pool.order_set(std::ranges::less{});
	//目标对象应可查找到
	const auto it = pool.find(second);
	ASSERT_NE(it, pool.end());
	//命中的对象ID应与查找键一致
	EXPECT_EQ(it->ID(), second);
}

//排序模式：降序排列同样可按ID查找
TEST_F(Object_Pool_Test, 排序模式降序查找)
{
	//默认构造的对象池
	engine::Object_Pool<Test_Object> pool;
	//新建三个对象
	pool.build();
	const uint64_t second = pool.build();
	pool.build();
	//以ID为投影字段降序排列
	pool.order_set(std::ranges::greater{});
	//目标对象应可查找到
	EXPECT_NE(pool.find(second), pool.end());
}

//排序模式：以派生字段为投影字段查找
TEST_F(Object_Pool_Test, 排序模式按投影字段查找)
{
	//以派生字段定位的对象池
	engine::Object_Pool<Test_Object, Payload_Projector> pool;
	//新建三个对象
	const uint64_t first = pool.build();
	const uint64_t second = pool.build();
	const uint64_t third = pool.build();
	//直接写入互不相同的投影值
	pool.data()[0].payload = 30;
	pool.data()[1].payload = 10;
	pool.data()[2].payload = 20;
	//以投影字段升序排列
	pool.order_set(std::ranges::less{});
	//按投影值查找
	const auto it = pool.find(10);
	//应命中且落在预期对象上
	ASSERT_NE(it, pool.end());
	EXPECT_EQ(it->ID(), second);
	//按另一个投影值查找
	const auto other = pool.find(30);
	ASSERT_NE(other, pool.end());
	EXPECT_EQ(other->ID(), first);
	//剩余的投影值同样可定位到对应对象
	const auto rest = pool.find(20);
	ASSERT_NE(rest, pool.end());
	EXPECT_EQ(rest->ID(), third);
}

//排序模式：卸载后查找返回超尾
TEST_F(Object_Pool_Test, 排序模式卸载后不可查找)
{
	//默认构造的对象池
	engine::Object_Pool<Test_Object> pool;
	//新建三个对象
	pool.build();
	const uint64_t second = pool.build();
	const uint64_t third = pool.build();
	//以ID为投影字段升序排列
	pool.order_set(std::ranges::less{});
	//卸载中间对象
	pool.unload(second);
	//已卸载对象应查不到
	EXPECT_EQ(pool.find(second), pool.end());
	//其余对象仍可查找到
	EXPECT_NE(pool.find(third), pool.end());
}

//排序模式：新建对象后仍应能找到它
TEST_F(Object_Pool_Test, 排序模式新建后可查找)
{
	//默认构造的对象池
	engine::Object_Pool<Test_Object> pool;
	//新建两个对象
	pool.build();
	pool.build();
	//以ID为投影字段降序排列
	pool.order_set(std::ranges::greater{});
	//排序模式下新建一个对象
	const uint64_t newest = pool.build();
	//新对象应可查找到
	EXPECT_NE(pool.find(newest), pool.end());
}

//排序重置：重置后可按ID查找到原有对象
TEST_F(Object_Pool_Test, 排序重置后原对象可查找)
{
	//默认构造的对象池
	engine::Object_Pool<Test_Object> pool;
	//新建两个对象
	const uint64_t first = pool.build();
	const uint64_t second = pool.build();
	//以ID为投影字段升序排列
	pool.order_set(std::ranges::less{});
	//重置排列方式
	pool.order_reset();
	//两个对象都应可查找到
	EXPECT_NE(pool.find(first), pool.end());
	EXPECT_NE(pool.find(second), pool.end());
}

//排列方式重置：已卸载对象不会再被写回定位映射
//order_reset 重建映射时以 valid() 判定记录有效，仅清有效标记的已卸载记录不会被误收录。
TEST_F(Object_Pool_Test, 重置排序后已卸载对象查不到)
{
	//默认构造的对象池
	engine::Object_Pool<Test_Object> pool;
	//新建两个对象
	const uint64_t first = pool.build();
	pool.build();
	//以ID为投影字段升序排列
	pool.order_set(std::ranges::less{});
	//卸载首个对象
	pool.unload(first);
	//重置排列方式
	pool.order_reset();
	//期望：已卸载对象查不到
	EXPECT_EQ(pool.find(first), pool.end());
}

//空池排序：没有记录可排时不应改动对象池
TEST_F(Object_Pool_Test, 空池设置排序方式)
{
	//默认构造的对象池（不含任何对象）
	engine::Object_Pool<Test_Object> pool;
	//设置排序方式
	pool.order_set(std::ranges::less{});
	//记录数量应保持不变
	EXPECT_TRUE(pool.data().empty());
}

//对象清除：两个模式下的分支各归其位
//clear 在稳定模式清空定位映射，在排序模式重置比较方式与有效索引起点。

//对象清除：稳定模式下清空
TEST_F(Object_Pool_Test, 稳定模式清空对象池)
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
TEST_F(Object_Pool_Test, 排序模式清空对象池)
{
	//默认构造的对象池
	engine::Object_Pool<Test_Object> pool;
	//新建两个对象
	pool.build();
	pool.build();
	//以ID为投影字段升序排列
	pool.order_set(std::ranges::less{});
	//清空对象池
	pool.clear();
	//容器应为空
	EXPECT_TRUE(pool.data().empty());
}

//空池卸载：对空池卸载任意ID只告警，不崩溃
TEST_F(Object_Pool_Test, 空池卸载不崩溃)
{
	//默认构造的空对象池
	engine::Object_Pool<Test_Object> pool;
	//卸载一个从未分配过的ID应不抛异常
	EXPECT_NO_THROW(pool.unload(1));
	//池内仍应无任何记录
	EXPECT_TRUE(pool.data().empty());
	//查询该ID应返回超尾
	EXPECT_EQ(pool.find(1), pool.end());
}

//重复装载：连续新建不会产生重复ID
TEST_F(Object_Pool_Test, 重复装载不产生重复ID)
{
	//默认构造的对象池
	engine::Object_Pool<Test_Object> pool;
	//用于登记已分配ID
	std::unordered_set<uint64_t> IDs;
	//连续新建多个对象
	const int count = 64;
	for (int i = 0; i < count; i++)
	{
		//每次新建一个对象
		const uint64_t ID = pool.build();
		//返回的ID应成功登记（即此前未出现过）
		EXPECT_TRUE(IDs.insert(ID).second);
	}
	//分配出的ID数量应与新建次数一致
	EXPECT_EQ(IDs.size(), static_cast<size_t>(count));
}

//大批量装载卸载：卸载不清除记录，存活对象仍可查找
TEST_F(Object_Pool_Test, 大批量装载卸载后计数正确)
{
	//默认构造的对象池
	engine::Object_Pool<Test_Object> pool;
	//记录全部ID
	std::vector<uint64_t> IDs;
	//批量新建
	const int count = 100;
	for (int i = 0; i < count; i++)
		IDs.push_back(pool.build());
	//记录数量应与新建次数一致
	EXPECT_EQ(pool.data().size(), static_cast<size_t>(count));
	//卸载序号为偶数的对象
	for (int i = 0; i < count; i += 2)
		pool.unload(IDs[i]);
	//卸载只清有效标记，容器规模应保持不变
	EXPECT_EQ(pool.data().size(), static_cast<size_t>(count));
	//逐一核对存活与已卸载对象
	for (int i = 0; i < count; i++)
	{
		//偶数序号已被卸载，应查不到
		if (i % 2 == 0)
			EXPECT_EQ(pool.find(IDs[i]), pool.end());
		//奇数序号仍在存活，应可查到
		else
			EXPECT_NE(pool.find(IDs[i]), pool.end());
	}
}

//卸载全部后再装载：清空后仍可新建并查找到对象
TEST_F(Object_Pool_Test, 卸载全部后再装载)
{
	//默认构造的对象池
	engine::Object_Pool<Test_Object> pool;
	//新建三个对象
	std::vector<uint64_t> IDs;
	for (int i = 0; i < 3; i++)
		IDs.push_back(pool.build());
	//逐个卸载全部对象
	for (const uint64_t& ID : IDs)
		pool.unload(ID);
	//全部对象都应查不到
	for (const uint64_t& ID : IDs)
		EXPECT_EQ(pool.find(ID), pool.end());
	//再次新建对象
	const uint64_t reused = pool.build();
	//新对象应可查找到
	EXPECT_NE(pool.find(reused), pool.end());
	//新对象ID不应为零
	EXPECT_NE(reused, 0u);
}

//非法ID查询：保留的零号ID与未分配ID都返回超尾
TEST_F(Object_Pool_Test, 非法ID查询返回超尾)
{
	//默认构造的对象池
	engine::Object_Pool<Test_Object> pool;
	//新建一个对象
	pool.build();
	//为非法实体预留的零号ID应查不到
	EXPECT_EQ(pool.find(0), pool.end());
	//从未分配过的较大ID应查不到
	EXPECT_EQ(pool.find(99999), pool.end());
}

//查询已卸载对象：卸载后按原ID查找返回超尾
TEST_F(Object_Pool_Test, 查询已卸载对象返回超尾)
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
	//未卸载对象应仍可查到
	EXPECT_NE(pool.find(second), pool.end());
}

//边界ID：零与64位极大值的查询与卸载都不崩溃
TEST_F(Object_Pool_Test, 边界ID查询与卸载不崩溃)
{
	//默认构造的对象池
	engine::Object_Pool<Test_Object> pool;
	//新建一个对象
	pool.build();
	//零号边界ID应查不到
	EXPECT_EQ(pool.find(0), pool.end());
	//64位无符号极大值ID应查不到
	EXPECT_EQ(pool.find(UINT64_MAX), pool.end());
	//卸载零号ID不应抛异常
	EXPECT_NO_THROW(pool.unload(0));
	//卸载极大值ID不应抛异常
	EXPECT_NO_THROW(pool.unload(UINT64_MAX));
	//原有对象应不受影响
	EXPECT_EQ(pool.data().size(), 1u);
}

//容量上限附近：大批量新建后首中尾对象均可见
TEST_F(Object_Pool_Test, 容量上限附近批量装载均可见)
{
	//默认构造的对象池
	engine::Object_Pool<Test_Object> pool;
	//一次规模较大的批量新建
	const int count = 1024;
	//记录全部ID
	std::vector<uint64_t> IDs;
	IDs.reserve(count);
	for (int i = 0; i < count; i++)
		IDs.push_back(pool.build());
	//容器规模应与新建次数一致
	EXPECT_EQ(pool.data().size(), static_cast<size_t>(count));
	//抽查首、中、尾三段对象是否均可查到
	EXPECT_NE(pool.find(IDs.front()), pool.end());
	EXPECT_NE(pool.find(IDs[count / 2]), pool.end());
	EXPECT_NE(pool.find(IDs.back()), pool.end());
}

//ID复用：卸载后新建会复用被回收的ID
TEST_F(Object_Pool_Test, 多次装载卸载后ID复用)
{
	//默认构造的对象池
	engine::Object_Pool<Test_Object> pool;
	//新建三个对象
	const uint64_t first = pool.build();
	const uint64_t second = pool.build();
	const uint64_t third = pool.build();
	//卸载中间对象
	pool.unload(second);
	//再次新建应复用被回收的ID
	const uint64_t reused = pool.build();
	//复用ID应与被卸载ID一致
	EXPECT_EQ(reused, second);
	//复用ID对应对象应可查到
	EXPECT_NE(pool.find(reused), pool.end());
	//其余原有对象不受影响
	EXPECT_NE(pool.find(first), pool.end());
	EXPECT_NE(pool.find(third), pool.end());
}

//析构：装满对象后对象池析构不应崩溃
TEST_F(Object_Pool_Test, 装满对象后析构不崩溃)
{
	//内层作用域用于触发生命周期结束时的析构
	{
		//默认构造的对象池
		engine::Object_Pool<Test_Object> pool;
		//装满大量对象
		for (int i = 0; i < 256; i++)
			pool.build();
		//池内应有对应数量的记录
		EXPECT_EQ(pool.data().size(), 256u);
	}
	//能执行到此说明析构过程未崩溃
	SUCCEED();
}

//定位字段：非整数键可用（字符串投影字段）
TEST_F(Object_Pool_Test, 字符串定位字段可用)
{
	//以字符串字段定位的对象池
	engine::Object_Pool<Tagged_Object, Tag_Projector> pool;
	//新建两个对象
	pool.build();
	pool.build();
	//直接写入互不相同的定位字段
	pool.data()[0].tag = "alpha";
	pool.data()[1].tag = "beta";
	//切换为排序模式后按定位字段检索
	pool.order_set(std::ranges::less{});
	//按定位字段查找
	const auto it = pool.find(std::string("beta"));
	//应命中且落在预期对象上
	ASSERT_NE(it, pool.end());
	EXPECT_EQ(it->tag, "beta");
	//未登记的字符串应查不到
	EXPECT_EQ(pool.find(std::string("gamma")), pool.end());
}

//对象清除：清空后ID分配起点恢复
TEST_F(Object_Pool_Test, 清空后ID起点恢复)
{
	//默认构造的对象池
	engine::Object_Pool<Test_Object> pool;
	//新建三个对象使ID游标前进
	pool.build();
	pool.build();
	pool.build();
	//清空对象池
	pool.clear();
	//清空后首个新建对象的ID应回到起点 1
	const uint64_t ID = pool.build();
	EXPECT_EQ(ID, 1u);
	//容器中只应保留新建的一个对象
	EXPECT_EQ(pool.data().size(), 1u);
}

//排列方式重置：稳定模式下调用的无副作用
TEST_F(Object_Pool_Test, 稳定模式重置排列方式无副作用)
{
	//默认构造的对象池
	engine::Object_Pool<Test_Object> pool;
	//新建两个对象
	const uint64_t first = pool.build();
	const uint64_t second = pool.build();
	//稳定模式下重置排列方式
	pool.order_reset();
	//两个对象仍可按ID查找到
	EXPECT_NE(pool.find(first), pool.end());
	EXPECT_NE(pool.find(second), pool.end());
}

//排序模式：卸载后新建的对象仍可查找
TEST_F(Object_Pool_Test, 排序模式卸载后新建可查找)
{
	//默认构造的对象池
	engine::Object_Pool<Test_Object> pool;
	//新建三个对象
	const uint64_t first = pool.build();
	const uint64_t second = pool.build();
	const uint64_t third = pool.build();
	//以ID为投影字段升序排列
	pool.order_set(std::ranges::less{});
	//卸载中间对象
	pool.unload(second);
	//排序模式下新建一个对象（复用被回收的下标）
	const uint64_t newest = pool.build();
	//新对象应可查找到
	EXPECT_NE(pool.find(newest), pool.end());
	//其余存活对象仍可查找到
	EXPECT_NE(pool.find(first), pool.end());
	EXPECT_NE(pool.find(third), pool.end());
	//卸载只清有效标记，新建复用被回收下标，容器规模不变
	EXPECT_EQ(pool.data().size(), 3u);
}
