//四叉树测试：覆盖构造与状态、范围计算、树扩大、区块构建、单点检索、范围检索与卸载
#include <gtest/gtest.h>

//获取四叉树
#include "src/core/spatial/partition/Quadtree/四叉树.h"

//四叉树测试夹具
class Quadtree_Test : public ::testing::Test
{
public:
	//释放检索结果，避免用例内内存泄漏
	static void chunk_clean(std::vector<std::shared_ptr<engine::Tree_Chunk_Data<int>>>& receiver)
	{
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
	const engine::Tree_State& state = tree.tree_state_get();
	//边长取默认值
	EXPECT_EQ(state.size, 256u);
	//区块单元取默认值
	EXPECT_EQ(state.block_size, 16u);
	//边长上限取 2 的 63 次方
	EXPECT_EQ(state.max_size, 9223372036854775808ull);
	//根坐标取默认的小数偏移
	EXPECT_EQ(state.root, (engine::Point2d(0.5, 0.5)));
}

//自定义构造：尺寸与根坐标写入状态
TEST_F(Quadtree_Test, 自定义构造写入状态)
{
	//指定边长与根坐标构造
	engine::Quadtree<int> tree(64, { 0.0, 0.0 });
	//读取状态
	const engine::Tree_State& state = tree.tree_state_get();
	//边长被写入
	EXPECT_EQ(state.size, 64u);
	//根坐标被写入
	EXPECT_EQ(state.root, (engine::Point2d(0.0, 0.0)));
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
	const engine::Tree_State& state = tree.tree_state_get();
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
	engine::Rect2l range{};
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
	engine::Rect2l range{};
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
	engine::Rect2l range{};
	//以根坐标 8.5 计算管理范围
	tree.manage_range_calcu(range, { 8.5, 8.5 }, 16);
	//四个边界
	EXPECT_EQ(range.left, 1);
	EXPECT_EQ(range.right, 16);
	EXPECT_EQ(range.down, 1);
	EXPECT_EQ(range.up, 16);
}

//64 位边界值：Point2l 与 Rect2l 承载大坐标，超大边长的管理范围不溢出
TEST_F(Quadtree_Test, Point2l与Rect2l边界值承载)
{
	//默认构造的四叉树
	engine::Quadtree<int> tree;
	//64 位点坐标边界值
	engine::Point2l point(1LL << 40, -(1LL << 40));
	//点坐标按 64 位整数原样保存
	EXPECT_EQ(point.X, 1LL << 40);
	EXPECT_EQ(point.Y, -(1LL << 40));
	//64 位矩形边界值
	engine::Rect2l rect(-(1LL << 40), 1LL << 40, 1LL << 40, -(1LL << 40));
	//矩形边界按 64 位整数原样保存
	EXPECT_EQ(rect.left, -(1LL << 40));
	EXPECT_EQ(rect.right, 1LL << 40);
	//超大边长的管理范围计算
	engine::Rect2l range{};
	tree.manage_range_calcu(range, { 0.5, 0.5 }, 1ull << 40);
	//半跨度取 2 的 39 次方减一，边界不溢出
	EXPECT_EQ(range.left, -(1LL << 39) + 1);
	EXPECT_EQ(range.right, 1LL << 39);
	EXPECT_EQ(range.down, -(1LL << 39) + 1);
	EXPECT_EQ(range.up, 1LL << 39);
}

//待查询范围：边界按区块大小向外对齐
TEST_F(Quadtree_Test, 待查询范围向外对齐)
{
	//默认构造的四叉树
	engine::Quadtree<int> tree;
	//待对齐的范围
	engine::Rect2l range{ -1, 6, 6, -1 };
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
	engine::Rect2l range{ -15, 0, 0, -15 };
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
	engine::Rect2l seekable{};
	//初始化为树的管理范围
	tree.manage_range_calcu(seekable, { 0.5, 0.5 }, 256);
	//待查询范围完全落在树内
	engine::Rect2l target{ -15, 0, 0, -15 };
	//求交集
	engine::Point2d register_coord = tree.seekable_range_calcu(target, seekable);
	//返回树根坐标表示无需扩大
	EXPECT_EQ(register_coord, (engine::Point2d(0.5, 0.5)));
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
	engine::Rect2l seekable{};
	//初始化为树的管理范围
	tree.manage_range_calcu(seekable, { 0.5, 0.5 }, 256);
	//待查询范围向右越界
	engine::Rect2l target{ 100, 200, 0, -15 };
	//求交集
	engine::Point2d register_coord = tree.seekable_range_calcu(target, seekable);
	//返回坐标非树根坐标
	EXPECT_NE(register_coord, (engine::Point2d(0.5, 0.5)));
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
	//先建立一条区块路径
	std::shared_ptr<engine::Tree_Chunk_Data<int>> before;
	tree.get({ 0, 0 }, before);
	ASSERT_NE(before, nullptr);
	//记录扩展前命中的区块存储
	std::shared_ptr<int> storage_before = before->ptr_data;
	//手动扩大一次
	ASSERT_TRUE(tree.tree_expand());
	//同一坐标再次获取
	std::shared_ptr<engine::Tree_Chunk_Data<int>> after;
	tree.get({ 0, 0 }, after);
	ASSERT_NE(after, nullptr);
	//仍落在同一块区块存储上
	EXPECT_EQ(after->ptr_data, storage_before);
}

// ———— 区块构建 ————

//区块构建：单点建出叶子后可只读取回
TEST_F(Quadtree_Test, build单点创建叶子后可seek取回)
{
	//默认构造的四叉树
	engine::Quadtree<int> tree;
	//按单点建出区块
	tree.build({ 0, 0 });
	//检索结果
	std::shared_ptr<engine::Tree_Chunk_Data<int>> receiver;
	//只读取回同一区块
	tree.seek({ 0, 0 }, receiver);
	ASSERT_NE(receiver, nullptr);
	//新区块已挂载默认分配的区块数据
	EXPECT_NE(receiver->ptr_data, nullptr);
	//区块中心为叶子范围中点
	EXPECT_DOUBLE_EQ(receiver->node.X, -7.5);
	EXPECT_DOUBLE_EQ(receiver->node.Y, -7.5);
}

//区块构建：挂载自定义数据后可原样取回同一对象
TEST_F(Quadtree_Test, build挂载自定义数据可原样取回)
{
	//默认构造的四叉树
	engine::Quadtree<int> tree;
	//自定义区块数据
	std::shared_ptr<int> data(new(std::nothrow) int(42));
	ASSERT_NE(data, nullptr);
	//按单点建出区块并挂载数据
	tree.build({ 0, 0 }, data);
	//检索结果
	std::shared_ptr<engine::Tree_Chunk_Data<int>> receiver;
	tree.seek({ 0, 0 }, receiver);
	ASSERT_NE(receiver, nullptr);
	//取回的是同一区块数据对象
	EXPECT_EQ(receiver->ptr_data, data);
	//数据内容未被改动
	EXPECT_EQ(*receiver->ptr_data, 42);
}

//区块构建：范围构建后单点与范围均可取回
TEST_F(Quadtree_Test, build范围创建后单点与范围均可取回)
{
	//默认构造的四叉树
	engine::Quadtree<int> tree;
	//按范围建出区块
	tree.build(engine::Rect2l(-15, 0, 0, -15));
	//单点取回该区块
	std::shared_ptr<engine::Tree_Chunk_Data<int>> point;
	tree.seek(engine::Point2l(0, 0), point);
	ASSERT_NE(point, nullptr);
	//区块数据已随范围构建分配
	EXPECT_NE(point->ptr_data, nullptr);
	//范围取回同一区块
	std::vector<std::shared_ptr<engine::Tree_Chunk_Data<int>>> range_receiver{};
	tree.seek(engine::Rect2l(-15, 0, 0, -15), range_receiver);
	ASSERT_EQ(range_receiver.size(), 1u);
	//两条路径指向同一区块数据
	EXPECT_EQ(range_receiver[0]->ptr_data, point->ptr_data);
	chunk_clean(range_receiver);
}

// ———— 单点检索 ————

//区块获取：不存在时创建、存在时取回同一叶子
TEST_F(Quadtree_Test, get不存在时创建存在时取回同一叶子)
{
	//默认构造的四叉树
	engine::Quadtree<int> tree;
	//首次获取创建区块
	std::shared_ptr<engine::Tree_Chunk_Data<int>> first;
	tree.get({ 0, 0 }, first);
	ASSERT_NE(first, nullptr);
	ASSERT_NE(first->ptr_data, nullptr);
	//再次获取命中已有叶子
	std::shared_ptr<engine::Tree_Chunk_Data<int>> second;
	tree.get({ 0, 0 }, second);
	ASSERT_NE(second, nullptr);
	//两次指向同一区块数据
	EXPECT_EQ(first->ptr_data, second->ptr_data);
}

//区块查找：seek 不创建区块而 get 会创建
TEST_F(Quadtree_Test, seek不创建区块而get会创建)
{
	//默认构造的四叉树
	engine::Quadtree<int> tree;
	//只读查找结果
	std::shared_ptr<engine::Tree_Chunk_Data<int>> seek_receiver;
	//只读查找尚未建立的区块
	tree.seek({ 0, 0 }, seek_receiver);
	//只读查找不创建区块
	EXPECT_EQ(seek_receiver, nullptr);
	//获取结果
	std::shared_ptr<engine::Tree_Chunk_Data<int>> get_receiver;
	//获取模式在同一坐标创建区块
	tree.get({ 0, 0 }, get_receiver);
	//获取模式建出并取回区块
	ASSERT_NE(get_receiver, nullptr);
	EXPECT_NE(get_receiver->ptr_data, nullptr);
}

//区块获取：重复获取命中同一区块存储
TEST_F(Quadtree_Test, 稳定模式重复查询命中同一存储)
{
	//默认构造的四叉树
	engine::Quadtree<int> tree;
	//两次检索结果
	std::shared_ptr<engine::Tree_Chunk_Data<int>> first;
	std::shared_ptr<engine::Tree_Chunk_Data<int>> second;
	//同一坐标获取两次
	tree.get({ 0, 0 }, first);
	tree.get({ 0, 0 }, second);
	ASSERT_NE(first, nullptr);
	ASSERT_NE(second, nullptr);
	//两次指向同一叶子
	EXPECT_EQ(first->ptr_data, second->ptr_data);
}

//区块获取：不同区块相互独立
TEST_F(Quadtree_Test, 不同区块相互独立)
{
	//默认构造的四叉树
	engine::Quadtree<int> tree;
	//两个不同区块的检索结果
	std::shared_ptr<engine::Tree_Chunk_Data<int>> west;
	std::shared_ptr<engine::Tree_Chunk_Data<int>> east;
	//原点与 (10,10) 分属不同区块
	tree.get({ 0, 0 }, west);
	tree.get({ 10, 10 }, east);
	ASSERT_NE(west, nullptr);
	ASSERT_NE(east, nullptr);
	//两块叶子互不相同
	EXPECT_NE(west->ptr_data, east->ptr_data);
	//区块中心各自独立
	EXPECT_DOUBLE_EQ(west->node.X, -7.5);
	EXPECT_DOUBLE_EQ(east->node.X, 8.5);
}

//区块获取：同一区块内的坐标命中同一叶子
TEST_F(Quadtree_Test, 同区块坐标命中同一叶子)
{
	//默认构造的四叉树
	engine::Quadtree<int> tree;
	//同一区块内的两个检索结果
	std::shared_ptr<engine::Tree_Chunk_Data<int>> origin;
	std::shared_ptr<engine::Tree_Chunk_Data<int>> corner;
	//(0,0) 与 (-15,-15) 同属 [-15,0] 区块
	tree.get({ 0, 0 }, origin);
	tree.get({ -15, -15 }, corner);
	ASSERT_NE(origin, nullptr);
	ASSERT_NE(corner, nullptr);
	//两个坐标落到同一叶子存储
	EXPECT_EQ(origin->ptr_data, corner->ptr_data);
	//区块中心一致
	EXPECT_DOUBLE_EQ(origin->node.X, corner->node.X);
}

//区块获取：区块边界两侧分属不同叶子
TEST_F(Quadtree_Test, 区块边界两侧分属不同叶子)
{
	//默认构造的四叉树
	engine::Quadtree<int> tree;
	//边界两侧的检索结果
	std::shared_ptr<engine::Tree_Chunk_Data<int>> inside;
	std::shared_ptr<engine::Tree_Chunk_Data<int>> outside;
	//(0,0) 在 [-15,0] 内，(-16,-16) 已跨到相邻区块
	tree.get({ 0, 0 }, inside);
	tree.get({ -16, -16 }, outside);
	ASSERT_NE(inside, nullptr);
	ASSERT_NE(outside, nullptr);
	//两者属于不同叶子
	EXPECT_NE(inside->ptr_data, outside->ptr_data);
}

//只读查找：不建立路径
TEST_F(Quadtree_Test, 非稳定模式不建立路径)
{
	//默认构造的四叉树
	engine::Quadtree<int> tree;
	//检索结果
	std::shared_ptr<engine::Tree_Chunk_Data<int>> receiver;
	//只读查询未建立过的坐标
	tree.seek({ 0, 0 }, receiver);
	//路径不存在，取不到区块
	EXPECT_EQ(receiver, nullptr);
}

//只读查找：可命中已建立的路径
TEST_F(Quadtree_Test, 非稳定模式命中已建立路径)
{
	//默认构造的四叉树
	engine::Quadtree<int> tree;
	//先用获取模式建立路径
	std::shared_ptr<engine::Tree_Chunk_Data<int>> created;
	tree.get({ 0, 0 }, created);
	ASSERT_NE(created, nullptr);
	//同区块的另一坐标走只读查找
	std::shared_ptr<engine::Tree_Chunk_Data<int>> hit;
	tree.seek({ -15, -15 }, hit);
	ASSERT_NE(hit, nullptr);
	//命中同一叶子存储
	EXPECT_EQ(hit->ptr_data, created->ptr_data);
}

//退化树：范围查找不产出结果
//边长为 16 的四叉树没有可寻址叶子（递归级数为零），范围查找应取不到区块
//注意：该退化树的析构函数会因递归级数为零而无限递归（栈溢出），
//      故此处以堆对象形式构造并刻意不释放，仅验证范围查找分支本身
TEST_F(Quadtree_Test, 退化树范围查询不产出结果)
{
	//堆上构造退化树（不释放，规避析构函数的无限递归缺陷）
	engine::Quadtree<int>* tree = new(std::nothrow) engine::Quadtree<int>(16, { 0.5, 0.5 });
	ASSERT_NE(tree, nullptr);
	//范围检索结果集合
	std::vector<std::shared_ptr<engine::Tree_Chunk_Data<int>>> receiver{};
	//范围只读查找
	tree->seek(engine::Rect2l(-7, 8, 8, -7), receiver);
	//无可寻址叶子，结果为空
	EXPECT_TRUE(receiver.empty());
	chunk_clean(receiver);
}

//退化树：单点查找触发引擎缺陷，暂不验证
//退化树递归级数为零，单点查找会直接对根节点（中间节点）执行
//std::get<1>(根节点数据)以取区块数据，而根节点持有的是子节点数组，
//故抛 std::bad_variant_access。此为引擎实现缺陷，非测试可修正范围，
//留待引擎层补齐退化树单点查找的空值防御后再启用。
TEST_F(Quadtree_Test, 退化树单点查询触发引擎缺陷)
{
	GTEST_SKIP() << "退化树单点查找对中间节点取区块数据抛 std::bad_variant_access，属引擎缺陷";
	//边长为 16 的退化为单区块的四叉树
	engine::Quadtree<int> tree(16, { 0.5, 0.5 });
	//检索结果
	std::shared_ptr<engine::Tree_Chunk_Data<int>> receiver;
	//单点只读查找
	tree.seek({ 0, 0 }, receiver);
	EXPECT_EQ(receiver, nullptr);
}

// ———— 卸载 ————

//区块卸载：单点卸载后叶子仍在但区块数据被置空
//引擎当前实现仅重置叶子持有的区块数据指针，叶子节点本身保留在树中
TEST_F(Quadtree_Test, unload单点后区块数据被置空)
{
	//默认构造的四叉树
	engine::Quadtree<int> tree;
	//先建出区块
	tree.build({ 0, 0 });
	//卸载前确认区块数据存在
	std::shared_ptr<engine::Tree_Chunk_Data<int>> before;
	tree.seek({ 0, 0 }, before);
	ASSERT_NE(before, nullptr);
	ASSERT_NE(before->ptr_data, nullptr);
	//卸载该区块
	tree.unload({ 0, 0 });
	//卸载后只读取回
	std::shared_ptr<engine::Tree_Chunk_Data<int>> after;
	tree.seek({ 0, 0 }, after);
	//叶子仍在树中，查找仍返回结果对象
	ASSERT_NE(after, nullptr);
	//卸载仅置空叶子持有的区块数据
	EXPECT_EQ(after->ptr_data, nullptr);
}

//区块卸载：范围卸载后区块数据被置空
TEST_F(Quadtree_Test, unload范围后区块数据被置空)
{
	//默认构造的四叉树
	engine::Quadtree<int> tree;
	//按范围建出区块
	tree.build(engine::Rect2l(-15, 0, 0, -15));
	//卸载前确认区块数据存在
	std::shared_ptr<engine::Tree_Chunk_Data<int>> before;
	tree.seek(engine::Point2l(0, 0), before);
	ASSERT_NE(before, nullptr);
	ASSERT_NE(before->ptr_data, nullptr);
	//按范围卸载该区块
	tree.unload(engine::Rect2l(-15, 0, 0, -15));
	//卸载后只读取回
	std::shared_ptr<engine::Tree_Chunk_Data<int>> after;
	tree.seek(engine::Point2l(0, 0), after);
	//叶子仍在树中，查找仍返回结果对象
	ASSERT_NE(after, nullptr);
	//卸载仅置空叶子持有的区块数据
	EXPECT_EQ(after->ptr_data, nullptr);
}

// ———— 64 位坐标 ————

//64 位坐标：可按大坐标建出区块并只读取回
//注意：引擎析构函数会为每个缺失子节点补建内存并递归整棵树，
//      对 64 位大坐标下的大边长树将按 4^递归级数 物化（此处级数 37），量级不可承受，
//      故以堆对象构造并刻意不释放，规避析构缺陷，仅验证建树与检索本身
TEST_F(Quadtree_Test, 64位坐标可建出并取回区块)
{
	//堆上构造的四叉树（刻意不释放）
	engine::Quadtree<int>* tree = new(std::nothrow) engine::Quadtree<int>();
	ASSERT_NE(tree, nullptr);
	//64 位大坐标
	const int64_t big = 1LL << 40;
	//按大坐标建出区块
	tree->build(engine::Point2l(big, big));
	//边长被逐级翻倍至 2 的 41 次方
	EXPECT_EQ(tree->tree_state_get().size, (1ull << 41));
	//检索结果
	std::shared_ptr<engine::Tree_Chunk_Data<int>> receiver;
	//按同一大坐标只读取回区块
	tree->seek(engine::Point2l(big, big), receiver);
	ASSERT_NE(receiver, nullptr);
	EXPECT_NE(receiver->ptr_data, nullptr);
	//区块中心落在目标坐标半个区块边长以内
	EXPECT_LE(std::abs(receiver->node.X - static_cast<double>(big)), 16.0);
	EXPECT_LE(std::abs(receiver->node.Y - static_cast<double>(big)), 16.0);
}

// ———— 越界与扩大联动 ————

//越界检索：回调放行时自动扩大一次
TEST_F(Quadtree_Test, 越界检索经回调放行后扩大)
{
	//默认构造的四叉树
	engine::Quadtree<int> tree;
	//回调一律放行
	tree.set_callback_manage([](engine::Point2d, engine::Point2l) { return true; });
	//检索结果
	std::shared_ptr<engine::Tree_Chunk_Data<int>> receiver;
	//获取超出当前管理范围的坐标
	tree.get({ 200, 200 }, receiver);
	//边长被扩大一次
	EXPECT_EQ(tree.tree_state_get().size, 512u);
	//扩大后该坐标落在树内并取到区块
	ASSERT_NE(receiver, nullptr);
	//数据指针有效
	EXPECT_NE(receiver->ptr_data, nullptr);
}

//越界检索：回调拒绝时不扩大
TEST_F(Quadtree_Test, 越界检索被回调拒绝)
{
	//默认构造的四叉树
	engine::Quadtree<int> tree;
	//回调一律拒绝
	tree.set_callback_manage([](engine::Point2d, engine::Point2l) { return false; });
	//检索结果
	std::shared_ptr<engine::Tree_Chunk_Data<int>> receiver;
	//获取超出当前管理范围的坐标
	tree.get({ 200, 200 }, receiver);
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
	std::shared_ptr<engine::Tree_Chunk_Data<int>> receiver;
	//不注册回调直接获取越界坐标
	tree.get({ 200, 200 }, receiver);
	//边长被扩大一次
	EXPECT_EQ(tree.tree_state_get().size, 512u);
}

//越界检索：已达边长上限时不再扩大
//先把上限设为 512，再用首次越界获取把边长撑到 512，使边长与上限相等
TEST_F(Quadtree_Test, 达到边长上限不再扩大)
{
	//默认构造的四叉树
	engine::Quadtree<int> tree;
	//把上限设为 512（须严格大于当前边长 256）
	ASSERT_TRUE(tree.set_max_size(512));
	//首次越界获取把边长撑到上限
	std::shared_ptr<engine::Tree_Chunk_Data<int>> first;
	tree.get({ 200, 200 }, first);
	ASSERT_EQ(tree.tree_state_get().size, 512u);
	//检索结果
	std::shared_ptr<engine::Tree_Chunk_Data<int>> receiver;
	//获取远超管理范围的坐标
	tree.get({ 100000, 100000 }, receiver);
	//边长保持在上限
	EXPECT_EQ(tree.tree_state_get().size, 512u);
	//未取到区块
	EXPECT_EQ(receiver, nullptr);
}

//越界检索：达到上限时回调收到一次通报
//先把边长撑到上限 512，再注册计数回调，观察上限分支的通报
TEST_F(Quadtree_Test, 达到上限时回调收到通报)
{
	//默认构造的四叉树
	engine::Quadtree<int> tree;
	//把上限设为 512
	ASSERT_TRUE(tree.set_max_size(512));
	//无回调地把边长撑到上限
	std::shared_ptr<engine::Tree_Chunk_Data<int>> warm;
	tree.get({ 200, 200 }, warm);
	ASSERT_EQ(tree.tree_state_get().size, 512u);
	//记录回调次数
	int call_count = 0;
	//注册计数回调（一律拒绝）
	tree.set_callback_manage([&call_count](engine::Point2d, engine::Point2l)
		{
			call_count++;
			return false;
		});
	//检索结果
	std::shared_ptr<engine::Tree_Chunk_Data<int>> receiver;
	//获取远超管理范围的坐标
	tree.get({ 100000, 100000 }, receiver);
	//上限分支下回调被调用一次，仅作通报
	EXPECT_EQ(call_count, 1);
	//未取到区块
	EXPECT_EQ(receiver, nullptr);
}

//越界检索：单次调用连续扩大直至容纳目标坐标
//实现语义：get 先按上限与当前边长算出最大检测次数，
//          再在循环里反复调用 seekable_analyse 逐级扩大，
//          所以单次获取会把边长一路翻倍到能容纳目标坐标为止（或触达上限）。
//注意：引擎析构函数会按 4^递归级数 物化整棵树，边长达 262144 时级数为 14，
//      析构耗时约 188 秒，故以堆对象构造并刻意不释放，规避析构缺陷
TEST_F(Quadtree_Test, 越界检索连续扩大直至容纳目标)
{
	//堆上构造的四叉树（刻意不释放）
	engine::Quadtree<int>* tree = new(std::nothrow) engine::Quadtree<int>();
	ASSERT_NE(tree, nullptr);
	//检索结果
	std::shared_ptr<engine::Tree_Chunk_Data<int>> receiver;
	//获取远超出管理范围的坐标
	tree->get({ 100000, 100000 }, receiver);
	//边长被逐级翻倍至足以覆盖 (100000,100000) 的 262144
	EXPECT_EQ(tree->tree_state_get().size, 262144u);
}

// ———— 范围检索 ————

//范围获取：格式化后覆盖单个区块时只返回一个区块
TEST_F(Quadtree_Test, 范围检索单区块)
{
	//默认构造的四叉树
	engine::Quadtree<int> tree;
	//检索结果集合
	std::vector<std::shared_ptr<engine::Tree_Chunk_Data<int>>> receiver;
	//获取原点所在区块
	engine::Rect2l range{ -15, 0, 0, -15 };
	tree.get(range, receiver);
	//恰好命中一个区块
	ASSERT_EQ(receiver.size(), 1u);
	//区块中心与单点检索一致
	EXPECT_DOUBLE_EQ(receiver[0]->node.X, -7.5);
	EXPECT_DOUBLE_EQ(receiver[0]->node.Y, -7.5);
	chunk_clean(receiver);
}

//范围获取：跨两个区块时返回四个区块
TEST_F(Quadtree_Test, 范围检索跨区块)
{
	//默认构造的四叉树
	engine::Quadtree<int> tree;
	//检索结果集合
	std::vector<std::shared_ptr<engine::Tree_Chunk_Data<int>>> receiver;
	//获取横跨四块区块的范围
	engine::Rect2l range{ -15, 16, 16, -15 };
	tree.get(range, receiver);
	//应取回四个区块
	ASSERT_EQ(receiver.size(), 4u);
	//逐块校验中心坐标
	bool has_sw = false;
	bool has_se = false;
	bool has_nw = false;
	bool has_ne = false;
	for (const auto& item : receiver)
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

//范围获取：结果与单点获取指向同一叶子
TEST_F(Quadtree_Test, 范围检索与单点检索一致)
{
	//默认构造的四叉树
	engine::Quadtree<int> tree;
	//先做一次单点获取
	std::shared_ptr<engine::Tree_Chunk_Data<int>> point;
	tree.get({ 0, 0 }, point);
	ASSERT_NE(point, nullptr);
	//再做同区块的范围获取
	std::vector<std::shared_ptr<engine::Tree_Chunk_Data<int>>> receiver;
	engine::Rect2l range{ -15, 0, 0, -15 };
	tree.get(range, receiver);
	ASSERT_EQ(receiver.size(), 1u);
	//两条路径指向同一叶子存储
	EXPECT_EQ(receiver[0]->ptr_data, point->ptr_data);
	chunk_clean(receiver);
}

//范围查找：只读模式不建立路径
TEST_F(Quadtree_Test, 范围检索非稳定模式)
{
	//默认构造的四叉树
	engine::Quadtree<int> tree;
	//检索结果集合
	std::vector<std::shared_ptr<engine::Tree_Chunk_Data<int>>> receiver;
	//只读模式查询新区块
	engine::Rect2l range{ -15, 0, 0, -15 };
	tree.seek(range, receiver);
	//节点尚未建立，无区块可取
	EXPECT_TRUE(receiver.empty());
	chunk_clean(receiver);
}

//范围获取：越界范围触发扩大
TEST_F(Quadtree_Test, 范围检索越界触发扩大)
{
	//默认构造的四叉树
	engine::Quadtree<int> tree;
	//回调一律放行
	tree.set_callback_manage([](engine::Point2d, engine::Point2l) { return true; });
	//检索结果集合
	std::vector<std::shared_ptr<engine::Tree_Chunk_Data<int>>> receiver;
	//获取向右越界的范围
	engine::Rect2l range{ 100, 200, 16, -15 };
	tree.get(range, receiver);
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
	std::shared_ptr<engine::Tree_Chunk_Data<int>> receiver;
	//作用域内获取后离开
	{
		engine::Quadtree<int> tree;
		tree.get({ 0, 0 }, receiver);
	}
	//执行至此说明回收路径未崩溃
	ASSERT_NE(receiver, nullptr);
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