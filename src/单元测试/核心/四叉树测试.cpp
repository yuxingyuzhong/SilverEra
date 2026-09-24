//四叉树测试：覆盖构造与状态、范围计算、树扩大、单点检索、范围检索与析构回收
#include <gtest/gtest.h>

//获取四叉树
#include "src/core/spatial/partition/Quadtree/四叉树.h"

//四叉树测试夹具
class Quadtree_Test : public ::testing::Test
{
public:
	//释放检索结果，避免用例内内存泄漏
	static void chunk_clean(std::vector<engine::tree_chunk_data<int>*>& receiver)
	{
		for (auto* item : receiver)
			delete item;
		receiver.clear();
	}
};

// ———— 构造与状态 ————

//默认构造：四个字段取结构体缺省值
TEST_F(Quadtree_Test, 默认构造的初始状态)
{
	//默认构造的四叉树
	engine::Quadtree<int> tree;
	//读取状态
	const engine::tree_state& state = tree.tree_state_get();
	//边长取默认值
	EXPECT_EQ(state.size, 256u);
	//区块单元取默认值
	EXPECT_EQ(state.block_size, 16u);
	//边长上限取 2 的 63 次方
	EXPECT_EQ(state.max_size, 9223372036854775808ull);
	//根坐标取默认的小数偏移
	EXPECT_EQ(state.root, (engine::coord2D_double(0.5, 0.5)));
}

//自定义构造：尺寸与根坐标写入状态
TEST_F(Quadtree_Test, 自定义构造写入状态)
{
	//指定边长与根坐标构造
	engine::Quadtree<int> tree(64, { 0.0, 0.0 });
	//读取状态
	const engine::tree_state& state = tree.tree_state_get();
	//边长被写入
	EXPECT_EQ(state.size, 64u);
	//根坐标被写入
	EXPECT_EQ(state.root, (engine::coord2D_double(0.0, 0.0)));
	//未指定项保持缺省
	EXPECT_EQ(state.block_size, 16u);
}

//设置：区块大小与边长上限可写入
TEST_F(Quadtree_Test, 设置区块与上限)
{
	//默认构造的四叉树
	engine::Quadtree<int> tree;
	//改写区块单元大小
	tree.set_block_size(8);
	//改写边长上限
	tree.set_max_size(1024);
	//两项设置均生效
	EXPECT_EQ(tree.tree_state_get().block_size, 8u);
	EXPECT_EQ(tree.tree_state_get().max_size, 1024u);
}

//状态获取：返回的是内部状态引用，可观察到后续变更
TEST_F(Quadtree_Test, 状态获取为内部引用)
{
	//默认构造的四叉树
	engine::Quadtree<int> tree;
	//取得状态引用
	const engine::tree_state& state = tree.tree_state_get();
	//设置区块大小
	tree.set_block_size(32);
	//引用应反映新值
	EXPECT_EQ(state.block_size, 32u);
}

// ———— 范围计算（公开工具）————

//管理范围：边长 256 时覆盖 [-127, 128]
TEST_F(Quadtree_Test, 管理范围计算256)
{
	//默认构造的四叉树
	engine::Quadtree<int> tree;
	//范围接收器
	engine::coord2D_range range{};
	//计算管理范围
	tree.manage_range_calcu(range, { 0.5, 0.5 }, 256);
	//左右边界覆盖 256 个整数单位
	EXPECT_EQ(range.left, -127);
	EXPECT_EQ(range.right, 128);
	//上下边界同上
	EXPECT_EQ(range.down, -127);
	EXPECT_EQ(range.up, 128);
}

//管理范围：边长 16 时覆盖 [-7, 8]
TEST_F(Quadtree_Test, 管理范围计算16)
{
	//默认构造的四叉树
	engine::Quadtree<int> tree;
	//范围接收器
	engine::coord2D_range range{};
	//计算管理范围
	tree.manage_range_calcu(range, { 0.5, 0.5 }, 16);
	//四个边界
	EXPECT_EQ(range.left, -7);
	EXPECT_EQ(range.right, 8);
	EXPECT_EQ(range.down, -7);
	EXPECT_EQ(range.up, 8);
}

//管理范围：根坐标平移时范围随之平移
TEST_F(Quadtree_Test, 管理范围随根坐标平移)
{
	//默认构造的四叉树
	engine::Quadtree<int> tree;
	//范围接收器
	engine::coord2D_range range{};
	//以根坐标 8.5 计算管理范围
	tree.manage_range_calcu(range, { 8.5, 8.5 }, 16);
	//四个边界
	EXPECT_EQ(range.left, 1);
	EXPECT_EQ(range.right, 16);
	EXPECT_EQ(range.down, 1);
	EXPECT_EQ(range.up, 16);
}

//待查询范围：边界按区块大小向外对齐
TEST_F(Quadtree_Test, 待查询范围向外对齐)
{
	//默认构造的四叉树
	engine::Quadtree<int> tree;
	//待对齐的范围
	engine::coord2D_range range{ -1, 6, 6, -1 };
	//执行格式化
	tree.target_range_format(range, { 0.5, 0.5 }, 16);
	//左边界向下对齐至 16 的倍数
	EXPECT_EQ(range.left, -15);
	//右边界向上对齐
	EXPECT_EQ(range.right, 16);
	//上边界向上对齐
	EXPECT_EQ(range.up, 16);
	//下边界向下对齐
	EXPECT_EQ(range.down, -15);
}

//待查询范围：已对齐的边界不再变动
TEST_F(Quadtree_Test, 已对齐范围保持不变)
{
	//默认构造的四叉树
	engine::Quadtree<int> tree;
	//恰好落在区块边界上的范围
	engine::coord2D_range range{ -15, 0, 0, -15 };
	//执行格式化
	tree.target_range_format(range, { 0.5, 0.5 }, 16);
	//四个边界均保持原值
	EXPECT_EQ(range.left, -15);
	EXPECT_EQ(range.right, 0);
	EXPECT_EQ(range.up, 0);
	EXPECT_EQ(range.down, -15);
}

//可查询范围：范围内的查询不申请扩大
TEST_F(Quadtree_Test, 可查询范围无需扩大)
{
	//默认构造的四叉树
	engine::Quadtree<int> tree;
	//可查询范围接收器
	engine::coord2D_range seekable{};
	//初始化为树的管理范围
	tree.manage_range_calcu(seekable, { 0.5, 0.5 }, 256);
	//待查询范围完全落在树内
	engine::coord2D_range target{ -15, 0, 0, -15 };
	//求交集
	engine::coord2D_double register_coord = tree.seekable_range_calcu(target, seekable);
	//返回树根坐标表示无需扩大
	EXPECT_EQ(register_coord, (engine::coord2D_double(0.5, 0.5)));
	//可查询范围被收窄为待查询范围
	EXPECT_EQ(seekable.left, -15);
	EXPECT_EQ(seekable.right, 0);
	EXPECT_EQ(seekable.up, 0);
	EXPECT_EQ(seekable.down, -15);
}

//可查询范围：越界时返回原边界作为扩大标记
TEST_F(Quadtree_Test, 可查询范围请求扩大)
{
	//默认构造的四叉树
	engine::Quadtree<int> tree;
	//可查询范围接收器
	engine::coord2D_range seekable{};
	//初始化为树的管理范围
	tree.manage_range_calcu(seekable, { 0.5, 0.5 }, 256);
	//待查询范围向右越界
	engine::coord2D_range target{ 100, 200, 0, -15 };
	//求交集
	engine::coord2D_double register_coord = tree.seekable_range_calcu(target, seekable);
	//返回坐标非树根坐标
	EXPECT_NE(register_coord, (engine::coord2D_double(0.5, 0.5)));
	//标记值取自原可查询范围的右边界
	EXPECT_DOUBLE_EQ(register_coord.X, 128.0);
	//未越界的轴保持树根坐标
	EXPECT_DOUBLE_EQ(register_coord.Y, 0.5);
	//落在树内的左边界被收窄
	EXPECT_EQ(seekable.left, 100);
}

// ———— 树扩大 ————

//树扩大：每次调用使边长翻倍
TEST_F(Quadtree_Test, 扩大使边长翻倍)
{
	//默认构造的四叉树
	engine::Quadtree<int> tree;
	//第一次扩大
	EXPECT_TRUE(tree.tree_expand());
	//边长翻倍
	EXPECT_EQ(tree.tree_state_get().size, 512u);
	//第二次扩大
	EXPECT_TRUE(tree.tree_expand());
	//边长再次翻倍
	EXPECT_EQ(tree.tree_state_get().size, 1024u);
}

//树扩大：原地语义保留已有区块
TEST_F(Quadtree_Test, 扩大保留原有区块)
{
	//默认构造的四叉树
	engine::Quadtree<int> tree;
	//先建立一条检索路径
	engine::tree_chunk_data<int>* before = nullptr;
	tree.block_seek(before, { 0, 0 }, true);
	ASSERT_NE(before, nullptr);
	//记录扩展前命中的区块存储
	int* storage_before = before->ptr_data;
	//手动扩大一次
	ASSERT_TRUE(tree.tree_expand());
	//同一坐标再次检索
	engine::tree_chunk_data<int>* after = nullptr;
	tree.block_seek(after, { 0, 0 }, true);
	ASSERT_NE(after, nullptr);
	//仍落在同一块区块存储上
	EXPECT_EQ(after->ptr_data, storage_before);
	delete before;
	delete after;
}

// ———— 单点检索 ————

//单点检索：稳定模式首次查询即建立路径
TEST_F(Quadtree_Test, 稳定模式建立路径)
{
	//默认构造的四叉树
	engine::Quadtree<int> tree;
	//检索结果
	engine::tree_chunk_data<int>* receiver = nullptr;
	//查询原点所在区块
	tree.block_seek(receiver, { 0, 0 }, true);
	ASSERT_NE(receiver, nullptr);
	//区块中心为叶子范围中点
	EXPECT_DOUBLE_EQ(receiver->node.X, -7.5);
	EXPECT_DOUBLE_EQ(receiver->node.Y, -7.5);
	//数据指针指向叶子存储
	EXPECT_NE(receiver->ptr_data, nullptr);
	delete receiver;
}

//单点检索：重复查询命中同一区块存储
TEST_F(Quadtree_Test, 稳定模式重复查询命中同一存储)
{
	//默认构造的四叉树
	engine::Quadtree<int> tree;
	//两次检索结果
	engine::tree_chunk_data<int>* first = nullptr;
	engine::tree_chunk_data<int>* second = nullptr;
	//同一坐标查询两次
	tree.block_seek(first, { 0, 0 }, true);
	tree.block_seek(second, { 0, 0 }, true);
	ASSERT_NE(first, nullptr);
	ASSERT_NE(second, nullptr);
	//两次指向同一叶子
	EXPECT_EQ(first->ptr_data, second->ptr_data);
	delete first;
	delete second;
}

//单点检索：不同区块相互独立
TEST_F(Quadtree_Test, 不同区块相互独立)
{
	//默认构造的四叉树
	engine::Quadtree<int> tree;
	//两个不同区块的检索结果
	engine::tree_chunk_data<int>* west = nullptr;
	engine::tree_chunk_data<int>* east = nullptr;
	//原点与 (10,10) 分属不同区块
	tree.block_seek(west, { 0, 0 }, true);
	tree.block_seek(east, { 10, 10 }, true);
	ASSERT_NE(west, nullptr);
	ASSERT_NE(east, nullptr);
	//两块叶子互不相同
	EXPECT_NE(west->ptr_data, east->ptr_data);
	//区块中心各自独立
	EXPECT_DOUBLE_EQ(west->node.X, -7.5);
	EXPECT_DOUBLE_EQ(east->node.X, 8.5);
	delete west;
	delete east;
}

//单点检索：同一区块内的坐标命中同一叶子
TEST_F(Quadtree_Test, 同区块坐标命中同一叶子)
{
	//默认构造的四叉树
	engine::Quadtree<int> tree;
	//同一区块内的两个检索结果
	engine::tree_chunk_data<int>* origin = nullptr;
	engine::tree_chunk_data<int>* corner = nullptr;
	//(0,0) 与 (-15,-15) 同属 [-15,0] 区块
	tree.block_seek(origin, { 0, 0 }, true);
	tree.block_seek(corner, { -15, -15 }, true);
	ASSERT_NE(origin, nullptr);
	ASSERT_NE(corner, nullptr);
	//两个坐标落到同一叶子存储
	EXPECT_EQ(origin->ptr_data, corner->ptr_data);
	//区块中心一致
	EXPECT_DOUBLE_EQ(origin->node.X, corner->node.X);
	delete origin;
	delete corner;
}

//单点检索：区块边界两侧分属不同叶子
TEST_F(Quadtree_Test, 区块边界两侧分属不同叶子)
{
	//默认构造的四叉树
	engine::Quadtree<int> tree;
	//边界两侧的检索结果
	engine::tree_chunk_data<int>* inside = nullptr;
	engine::tree_chunk_data<int>* outside = nullptr;
	//(0,0) 在 [-15,0] 内，(-16,-16) 已跨到相邻区块
	tree.block_seek(inside, { 0, 0 }, true);
	tree.block_seek(outside, { -16, -16 }, true);
	ASSERT_NE(inside, nullptr);
	ASSERT_NE(outside, nullptr);
	//两者属于不同叶子
	EXPECT_NE(inside->ptr_data, outside->ptr_data);
	delete inside;
	delete outside;
}

//单点检索：非稳定模式不建立路径
TEST_F(Quadtree_Test, 非稳定模式不建立路径)
{
	//默认构造的四叉树
	engine::Quadtree<int> tree;
	//检索结果
	engine::tree_chunk_data<int>* receiver = nullptr;
	//非稳定模式查询未建立过的坐标
	tree.block_seek(receiver, { 0, 0 }, false);
	//路径不存在，取不到区块
	EXPECT_EQ(receiver, nullptr);
}

//单点检索：非稳定模式可命中已建立的路径
TEST_F(Quadtree_Test, 非稳定模式命中已建立路径)
{
	//默认构造的四叉树
	engine::Quadtree<int> tree;
	//先用稳定模式建立路径
	engine::tree_chunk_data<int>* created = nullptr;
	tree.block_seek(created, { 0, 0 }, true);
	ASSERT_NE(created, nullptr);
	//同区块的另一坐标走非稳定模式
	engine::tree_chunk_data<int>* hit = nullptr;
	tree.block_seek(hit, { -15, -15 }, false);
	ASSERT_NE(hit, nullptr);
	//命中同一叶子存储
	EXPECT_EQ(hit->ptr_data, created->ptr_data);
	delete created;
	delete hit;
}

//单点检索：边长等于区块大小时直接落在根区块
TEST_F(Quadtree_Test, 最小树直接落在根区块)
{
	//边长为 16 的退化为单区块的四叉树
	engine::Quadtree<int> tree(16, { 0.5, 0.5 });
	//检索结果
	engine::tree_chunk_data<int>* receiver = nullptr;
	//查询任意树内坐标
	tree.block_seek(receiver, { 0, 0 }, true);
	ASSERT_NE(receiver, nullptr);
	//管理范围为 [-7,8]，区块中心为 0.5
	EXPECT_DOUBLE_EQ(receiver->node.X, 0.5);
	EXPECT_DOUBLE_EQ(receiver->node.Y, 0.5);
	delete receiver;
}

// ———— 越界与扩大联动 ————

//越界检索：回调放行时自动扩大一次
TEST_F(Quadtree_Test, 越界检索经回调放行后扩大)
{
	//默认构造的四叉树
	engine::Quadtree<int> tree;
	//回调一律放行
	tree.set_callback_manage([](engine::coord2D_double, engine::coord2D_int) { return true; });
	//检索结果
	engine::tree_chunk_data<int>* receiver = nullptr;
	//查询超出当前管理范围的坐标
	tree.block_seek(receiver, { 200, 200 }, true);
	//边长被扩大一次
	EXPECT_EQ(tree.tree_state_get().size, 512u);
	//扩大后该坐标落在树内并取到区块
	ASSERT_NE(receiver, nullptr);
	//数据指针有效
	EXPECT_NE(receiver->ptr_data, nullptr);
	delete receiver;
}

//越界检索：回调拒绝时不扩大
TEST_F(Quadtree_Test, 越界检索被回调拒绝)
{
	//默认构造的四叉树
	engine::Quadtree<int> tree;
	//回调一律拒绝
	tree.set_callback_manage([](engine::coord2D_double, engine::coord2D_int) { return false; });
	//检索结果
	engine::tree_chunk_data<int>* receiver = nullptr;
	//查询超出当前管理范围的坐标
	tree.block_seek(receiver, { 200, 200 }, true);
	//边长未变
	EXPECT_EQ(tree.tree_state_get().size, 256u);
	//未取到区块
	EXPECT_EQ(receiver, nullptr);
}

//越界检索：未注册回调时仍会自动扩大
TEST_F(Quadtree_Test, 越界检索无回调时扩大)
{
	//默认构造的四叉树
	engine::Quadtree<int> tree;
	//检索结果
	engine::tree_chunk_data<int>* receiver = nullptr;
	//不注册回调直接查询越界坐标
	tree.block_seek(receiver, { 200, 200 }, true);
	//边长被扩大一次
	EXPECT_EQ(tree.tree_state_get().size, 512u);
	delete receiver;
}

//越界检索：已达边长上限时不再扩大
TEST_F(Quadtree_Test, 达到边长上限不再扩大)
{
	//默认构造的四叉树
	engine::Quadtree<int> tree;
	//把上限压到当前边长
	tree.set_max_size(256);
	//检索结果
	engine::tree_chunk_data<int>* receiver = nullptr;
	//查询越界坐标
	tree.block_seek(receiver, { 200, 200 }, true);
	//边长保持不变
	EXPECT_EQ(tree.tree_state_get().size, 256u);
	//未取到区块
	EXPECT_EQ(receiver, nullptr);
}

//越界检索：达到上限时回调收到一次通报
TEST_F(Quadtree_Test, 达到上限时回调收到通报)
{
	//默认构造的四叉树
	engine::Quadtree<int> tree;
	//把上限压到当前边长
	tree.set_max_size(256);
	//记录回调次数
	int call_count = 0;
	//注册计数回调
	tree.set_callback_manage([&call_count](engine::coord2D_double, engine::coord2D_int)
		{
			call_count++;
			return false;
		});
	//检索结果
	engine::tree_chunk_data<int>* receiver = nullptr;
	//查询越界坐标
	tree.block_seek(receiver, { 500, 500 }, true);
	//上限分支下回调被调用一次，仅作通报
	EXPECT_EQ(call_count, 1);
	//未取到区块
	EXPECT_EQ(receiver, nullptr);
}

//越界检索：单次调用最多扩大一次边长
TEST_F(Quadtree_Test, 越界检索单次调用只扩大一次)
{
	//默认构造的四叉树
	engine::Quadtree<int> tree;
	//检索结果
	engine::tree_chunk_data<int>* receiver = nullptr;
	//查询远超出管理范围的坐标
	tree.block_seek(receiver, { 100000, 100000 }, true);
	//单次调用只把边长翻一倍
	EXPECT_EQ(tree.tree_state_get().size, 512u);
	delete receiver;
}

// ———— 范围检索 ————

//范围检索：格式化后覆盖单个区块时只返回一个区块
TEST_F(Quadtree_Test, 范围检索单区块)
{
	//默认构造的四叉树
	engine::Quadtree<int> tree;
	//检索结果集合
	std::vector<engine::tree_chunk_data<int>*> receiver;
	//查询原点所在区块
	engine::coord2D_range range{ -15, 0, 0, -15 };
	tree.range_seek(receiver, range, true);
	//恰好命中一个区块
	ASSERT_EQ(receiver.size(), 1u);
	//区块中心与单点检索一致
	EXPECT_DOUBLE_EQ(receiver[0]->node.X, -7.5);
	EXPECT_DOUBLE_EQ(receiver[0]->node.Y, -7.5);
	chunk_clean(receiver);
}

//范围检索：跨两个区块时返回四个区块
TEST_F(Quadtree_Test, 范围检索跨区块)
{
	//默认构造的四叉树
	engine::Quadtree<int> tree;
	//检索结果集合
	std::vector<engine::tree_chunk_data<int>*> receiver;
	//查询横跨四块区块的范围
	engine::coord2D_range range{ -15, 16, 16, -15 };
	tree.range_seek(receiver, range, true);
	//应取回四个区块
	ASSERT_EQ(receiver.size(), 4u);
	//逐块校验中心坐标
	bool has_sw = false;
	bool has_se = false;
	bool has_nw = false;
	bool has_ne = false;
	for (auto* item : receiver)
	{
		if (item->node.X == -7.5 && item->node.Y == -7.5)
			has_sw = true;
		if (item->node.X == 8.5 && item->node.Y == -7.5)
			has_se = true;
		if (item->node.X == -7.5 && item->node.Y == 8.5)
			has_nw = true;
		if (item->node.X == 8.5 && item->node.Y == 8.5)
			has_ne = true;
	}
	//四块区块齐全且互不重复
	EXPECT_TRUE(has_sw);
	EXPECT_TRUE(has_se);
	EXPECT_TRUE(has_nw);
	EXPECT_TRUE(has_ne);
	chunk_clean(receiver);
}

//范围检索：结果与单点检索指向同一叶子
TEST_F(Quadtree_Test, 范围检索与单点检索一致)
{
	//默认构造的四叉树
	engine::Quadtree<int> tree;
	//先做一次单点检索
	engine::tree_chunk_data<int>* point = nullptr;
	tree.block_seek(point, { 0, 0 }, true);
	ASSERT_NE(point, nullptr);
	//再做同区块的范围检索
	std::vector<engine::tree_chunk_data<int>*> receiver;
	engine::coord2D_range range{ -15, 0, 0, -15 };
	tree.range_seek(receiver, range, true);
	ASSERT_EQ(receiver.size(), 1u);
	//两条路径指向同一叶子存储
	EXPECT_EQ(receiver[0]->ptr_data, point->ptr_data);
	delete point;
	chunk_clean(receiver);
}

//范围检索：非稳定模式不建立路径
TEST_F(Quadtree_Test, 范围检索非稳定模式)
{
	//默认构造的四叉树
	engine::Quadtree<int> tree;
	//检索结果集合
	std::vector<engine::tree_chunk_data<int>*> receiver;
	//非稳定模式查询新区块
	engine::coord2D_range range{ -15, 0, 0, -15 };
	tree.range_seek(receiver, range, false);
	//节点尚未建立，无区块可取
	EXPECT_TRUE(receiver.empty());
	chunk_clean(receiver);
}

//范围检索：越界范围触发扩大
TEST_F(Quadtree_Test, 范围检索越界触发扩大)
{
	//默认构造的四叉树
	engine::Quadtree<int> tree;
	//回调一律放行
	tree.set_callback_manage([](engine::coord2D_double, engine::coord2D_int) { return true; });
	//检索结果集合
	std::vector<engine::tree_chunk_data<int>*> receiver;
	//查询向右越界的范围
	engine::coord2D_range range{ 100, 200, 16, -15 };
	tree.range_seek(receiver, range, true);
	//边长被扩大
	EXPECT_GT(tree.tree_state_get().size, 256u);
	//越界部分被纳入后取到区块
	EXPECT_FALSE(receiver.empty());
	chunk_clean(receiver);
}

// ———— 析构 ————

//析构：未检索的树可安全析构
TEST_F(Quadtree_Test, 空树析构)
{
	//作用域内构造并立即离开
	{
		engine::Quadtree<int> tree;
	}
	//执行至此说明析构未崩溃
	SUCCEED();
}

//析构：建立路径后可安全析构
TEST_F(Quadtree_Test, 建立路径后析构)
{
	//检索结果在作用域外持有
	engine::tree_chunk_data<int>* receiver = nullptr;
	//作用域内检索后离开
	{
		engine::Quadtree<int> tree;
		tree.block_seek(receiver, { 0, 0 }, true);
	}
	//执行至此说明回收路径未崩溃
	ASSERT_NE(receiver, nullptr);
	delete receiver;
}

//析构：反复扩大后可安全析构
TEST_F(Quadtree_Test, 反复扩大后析构)
{
	//作用域内构造
	{
		engine::Quadtree<int> tree;
		//连续扩大两次
		tree.tree_expand();
		tree.tree_expand();
	}
	//执行至此说明回收未崩溃
	SUCCEED();
}
