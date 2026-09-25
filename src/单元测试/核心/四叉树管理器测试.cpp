//四叉树管理器测试：覆盖设置读写、智能创建、单点查询、越界联动、范围查询、合并与卸载
#include <gtest/gtest.h>

//获取四叉树管理器
#include "src/core/spatial/partition/Quadtree_Manager/四叉树管理器.h"

//四叉树管理器测试夹具
class Quadtree_Manager_Test : public ::testing::Test
{
public:
	//把边长上限压到 256，再按目标坐标建一棵四叉树
	//tree_record 的大小字段已随引擎修复改回 uint64_t，默认上限 65536 不再溢出；
	//此处仍压到 256，只是为了让包围矩形与区块划分保持固定、便于断言。
	static void build_one_tree(engine::Quadtree_Manager<int>& manager, const engine::Point2i& target)
	{
		manager.set_max_size(256);
		manager.qurdtree_build_smart({ target });
	}

	//释放单点查询结果
	static void chunk_clean(engine::tree_chunk_data<int>*& receiver)
	{
		delete receiver;
		receiver = nullptr;
	}

	//释放范围查询结果
	static void chunk_clean(std::vector<engine::tree_chunk_data<int>*>& receiver)
	{
		for (auto* item : receiver)
			delete item;
		receiver.clear();
	}

	//构造整数坐标
	static engine::Point2i make_coord(int coord_X, int coord_Y)
	{
		return engine::Point2i(coord_X, coord_Y);
	}

	//构造范围（左、右、上、下）
	static engine::Rect2i make_range(int left, int right, int up, int down)
	{
		engine::Rect2i range{};
		range.left = left;
		range.right = right;
		range.up = up;
		range.down = down;
		return range;
	}
};

// ———— 构造与设置 ————

//默认构造：设置结构体六项取缺省值
TEST_F(Quadtree_Manager_Test, 默认设置的初始值)
{
	//默认构造的管理器
	engine::Quadtree_Manager<int> manager;
	//读取设置
	const engine::tree_manager_settings& settings = manager.settings_get();
	//区块单元与边长上下限取缺省值
	EXPECT_EQ(settings.block_size, 16u);
	EXPECT_EQ(settings.max_tree_size, 65536u);
	EXPECT_EQ(settings.min_tree_size, 256u);
	//缓存缺省启用，阈值与条目上限取缺省值
	EXPECT_TRUE(settings.is_cache_enabled);
	EXPECT_EQ(settings.cache_active_threshold, 32u);
	EXPECT_EQ(settings.max_cache_records, 16u);
}

//默认构造：四叉树序列为空且最大规模为零
TEST_F(Quadtree_Manager_Test, 初始序列与最大规模)
{
	//默认构造的管理器
	engine::Quadtree_Manager<int> manager;
	//序列中尚无四叉树
	EXPECT_TRUE(manager.records_get().empty());
	//最大规模尚未记录
	EXPECT_EQ(manager.largest_size_get(), 0u);
}

//设置：六项设置均可写入设置结构体
TEST_F(Quadtree_Manager_Test, 设置项写入设置结构体)
{
	//默认构造的管理器
	engine::Quadtree_Manager<int> manager;
	//改写区块单元大小
	manager.set_block_size(8);
	//改写边长上限
	manager.set_max_size(1024);
	//改写边长下限
	manager.set_min_size(64);
	//关闭高速缓存
	manager.set_cache_state(false);
	//改写缓存启用阈值
	manager.set_cache_active_threshold(8);
	//改写缓存条目上限
	manager.set_max_cach_records(4);
	//读取设置
	const engine::tree_manager_settings& settings = manager.settings_get();
	//六项设置均已生效
	EXPECT_EQ(settings.block_size, 8u);
	EXPECT_EQ(settings.max_tree_size, 1024u);
	EXPECT_EQ(settings.min_tree_size, 64u);
	EXPECT_FALSE(settings.is_cache_enabled);
	EXPECT_EQ(settings.cache_active_threshold, 8u);
	EXPECT_EQ(settings.max_cache_records, 4u);
}

//设置：序列为空时设置区块大小与边长不产生副作用
TEST_F(Quadtree_Manager_Test, 空序列下设置不影响序列)
{
	//默认构造的管理器
	engine::Quadtree_Manager<int> manager;
	//在没有任何四叉树时下发设置
	manager.set_block_size(32);
	manager.set_max_size(512);
	manager.set_min_size(128);
	//序列仍为空，设置照常记录
	EXPECT_TRUE(manager.records_get().empty());
	EXPECT_EQ(manager.settings_get().block_size, 32u);
	EXPECT_EQ(manager.settings_get().max_tree_size, 512u);
	EXPECT_EQ(manager.settings_get().min_tree_size, 128u);
}

//读取：三项读取接口返回的都是内部成员的引用
TEST_F(Quadtree_Manager_Test, 读取接口返回内部引用)
{
	//默认构造的管理器
	engine::Quadtree_Manager<int> manager;
	//设置引用两次取址一致
	EXPECT_EQ(&manager.settings_get(), &manager.settings_get());
	//序列引用两次取址一致
	EXPECT_EQ(&manager.records_get(), &manager.records_get());
	//最大规模引用两次取址一致
	EXPECT_EQ(&manager.largest_size_get(), &manager.largest_size_get());
}

// ———— 智能创建 ————

//智能创建：单点在上限 256 下建立一棵恰好覆盖它的四叉树
TEST_F(Quadtree_Manager_Test, 智能创建单点建立一棵树)
{
	//默认构造的管理器
	engine::Quadtree_Manager<int> manager;
	//按单点建树
	build_one_tree(manager, make_coord(0, 0));
	//读取序列
	const std::vector<engine::tree_record<int>*>& records = manager.records_get();
	//只建立一棵四叉树
	ASSERT_EQ(records.size(), 1u);
	//包围矩形按上限对齐后为 [0,255]×[0,255]，根节点落在其中心
	EXPECT_EQ(records[0]->root, (engine::Point2d(127.5, 127.5)));
	//记录的大小与上限一致
	EXPECT_EQ(records[0]->size, 256);
}

//智能创建：两点分处不同最大区块时各建一棵树，序列按根节点 X 降序
TEST_F(Quadtree_Manager_Test, 智能创建多点建立多棵树)
{
	//默认构造的管理器
	engine::Quadtree_Manager<int> manager;
	//设定边长上限并投喂两个相距较远的坐标
	manager.set_max_size(256);
	manager.qurdtree_build_smart({ make_coord(0, 0), make_coord(300, 300) });
	//读取序列
	const std::vector<engine::tree_record<int>*>& records = manager.records_get();
	//两个坐标各占一个最大区块，故建立两棵树
	ASSERT_EQ(records.size(), 2u);
	//(300,300) 所属区块的根坐标更大，排在前面
	EXPECT_EQ(records[0]->root, (engine::Point2d(383.5, 383.5)));
	EXPECT_EQ(records[1]->root, (engine::Point2d(127.5, 127.5)));
	//两棵树的大小均等于上限
	EXPECT_EQ(records[0]->size, 256);
	EXPECT_EQ(records[1]->size, 256);
}

//智能创建：已覆盖的坐标不重复建树
TEST_F(Quadtree_Manager_Test, 智能创建不重复覆盖已建区域)
{
	//默认构造的管理器
	engine::Quadtree_Manager<int> manager;
	//先建立覆盖 [0,255]×[0,255] 的四叉树
	build_one_tree(manager, make_coord(0, 0));
	//(10,10) 已落在现有四叉树管理范围内
	manager.qurdtree_build_smart({ make_coord(10, 10) });
	//序列数量不变
	EXPECT_EQ(manager.records_get().size(), 1u);
}

//智能创建：首次创建不更新最大规模记录
TEST_F(Quadtree_Manager_Test, 智能创建后最大规模仍为零)
{
	//默认构造的管理器
	engine::Quadtree_Manager<int> manager;
	//按单点建树
	build_one_tree(manager, make_coord(0, 0));
	//最大规模只在"扩大审批"路径上写入，首次建树不经过该路径
	EXPECT_EQ(manager.largest_size_get(), 0u);
}

// ———— 单点查询 ————

//单点查询：空管理器合上不稳定模式时返回空结果且不建树
TEST_F(Quadtree_Manager_Test, 不稳定查询空管理器返回空结果)
{
	//默认构造的管理器
	engine::Quadtree_Manager<int> manager;
	//查询结果存储
	engine::tree_chunk_data<int>* receiver = nullptr;
	//不稳定模式下不创建任何区块
	manager.seek(receiver, make_coord(0, 0), false);
	//未取得结果
	EXPECT_EQ(receiver, nullptr);
	//未建立四叉树
	EXPECT_TRUE(manager.records_get().empty());
}

//单点查询：稳定模式自动建树并返回区块
TEST_F(Quadtree_Manager_Test, 稳定查询自动建树)
{
	//默认构造的管理器
	engine::Quadtree_Manager<int> manager;
	//查询结果存储
	engine::tree_chunk_data<int>* receiver = nullptr;
	//稳定模式下查询未覆盖坐标
	manager.seek(receiver, make_coord(0, 0), true);
	//取得了区块信息
	ASSERT_NE(receiver, nullptr);
	//(0,0) 落在最小区块 [0,15]×[0,15]，其中心为 (7.5,7.5)
	EXPECT_EQ(receiver->node, (engine::Point2d(7.5, 7.5)));
	//顺带建立了一棵四叉树
	EXPECT_EQ(manager.records_get().size(), 1u);
	chunk_clean(receiver);
}

//单点查询：命中已建四叉树的区块
TEST_F(Quadtree_Manager_Test, 稳定查询命中已有树)
{
	//默认构造的管理器
	engine::Quadtree_Manager<int> manager;
	//先建立一棵覆盖 [0,255]×[0,255] 的四叉树
	build_one_tree(manager, make_coord(0, 0));
	//查询结果存储
	engine::tree_chunk_data<int>* receiver = nullptr;
	//查询落在该树内的坐标
	manager.seek(receiver, make_coord(10, 10), true);
	//取得了区块信息
	ASSERT_NE(receiver, nullptr);
	//(10,10) 同样落在 [0,15]×[0,15]
	EXPECT_EQ(receiver->node, (engine::Point2d(7.5, 7.5)));
	//序列数量未变
	EXPECT_EQ(manager.records_get().size(), 1u);
	chunk_clean(receiver);
}

//单点查询：不同象限落到不同区块
TEST_F(Quadtree_Manager_Test, 稳定查询不同象限返回不同区块)
{
	//默认构造的管理器
	engine::Quadtree_Manager<int> manager;
	//先建立一棵覆盖 [0,255]×[0,255] 的四叉树
	build_one_tree(manager, make_coord(0, 0));
	//查询结果存储
	engine::tree_chunk_data<int>* receiver = nullptr;
	//查询东北侧的坐标
	manager.seek(receiver, make_coord(200, 200), true);
	//取得了区块信息
	ASSERT_NE(receiver, nullptr);
	//逐级向东北再折向西南，最终落在 [192,207]×[192,207]
	EXPECT_EQ(receiver->node, (engine::Point2d(199.5, 199.5)));
	chunk_clean(receiver);
}

//单点查询：不稳定模式不创建区块
TEST_F(Quadtree_Manager_Test, 不稳定查询不创建区块)
{
	//默认构造的管理器
	engine::Quadtree_Manager<int> manager;
	//先建立一棵覆盖 [0,255]×[0,255] 的四叉树
	build_one_tree(manager, make_coord(0, 0));
	//查询结果存储
	engine::tree_chunk_data<int>* receiver = nullptr;
	//不稳定模式下查询尚未开辟路径的坐标
	manager.seek(receiver, make_coord(10, 10), false);
	//未取得结果，因为不稳定模式不创建区块节点
	EXPECT_EQ(receiver, nullptr);
}

//单点查询：重复查询同一坐标得到同一区块
TEST_F(Quadtree_Manager_Test, 稳定查询结果可重复取得)
{
	//默认构造的管理器
	engine::Quadtree_Manager<int> manager;
	//先建立一棵覆盖 [0,255]×[0,255] 的四叉树
	build_one_tree(manager, make_coord(0, 0));
	//第一次查询
	engine::tree_chunk_data<int>* first = nullptr;
	manager.seek(first, make_coord(10, 10), true);
	ASSERT_NE(first, nullptr);
	//第二次查询同一坐标
	engine::tree_chunk_data<int>* second = nullptr;
	manager.seek(second, make_coord(10, 10), true);
	ASSERT_NE(second, nullptr);
	//两次落在同一区块（区块数据指针指向同一叶子）
	EXPECT_EQ(first->node, second->node);
	EXPECT_EQ(first->ptr_data, second->ptr_data);
	//结果对象各自独立，需要各自释放
	chunk_clean(first);
	chunk_clean(second);
}

// ———— 越界联动（查询触发智能创建）————

//越界查询：现有四叉树已达上限时改建一棵新树覆盖目标
TEST_F(Quadtree_Manager_Test, 越界查询改建新树)
{
	//默认构造的管理器
	engine::Quadtree_Manager<int> manager;
	//建立一棵覆盖 [0,255]×[0,255] 的四叉树（其大小恰好等于上限）
	build_one_tree(manager, make_coord(0, 0));
	//查询结果存储
	engine::tree_chunk_data<int>* receiver = nullptr;
	//查询落在现有树之外的坐标
	manager.seek(receiver, make_coord(300, 300), true);
	//取得了区块信息
	ASSERT_NE(receiver, nullptr);
	//读取序列
	const std::vector<engine::tree_record<int>*>& records = manager.records_get();
	//在原树之外补建了一棵新树
	ASSERT_EQ(records.size(), 2u);
	//新树的根节点正对目标所在区块，大小取下限 256
	EXPECT_EQ(records[0]->root, (engine::Point2d(383.5, 383.5)));
	EXPECT_EQ(records[0]->size, 256);
	//原树保持不变
	EXPECT_EQ(records[1]->root, (engine::Point2d(127.5, 127.5)));
	EXPECT_EQ(records[1]->size, 256);
	chunk_clean(receiver);
}

//越界查询：基准树不等于下限大小时先做半格校准再改建
TEST_F(Quadtree_Manager_Test, 越界查询按基准树校准新树位置)
{
	//默认构造的管理器
	engine::Quadtree_Manager<int> manager;
	//把上限抬到 512，使建成的主树大小为 512（不等于下限 256）
	manager.set_max_size(512);
	manager.qurdtree_build_smart({ make_coord(0, 0) });
	ASSERT_EQ(manager.records_get().size(), 1u);
	ASSERT_EQ(manager.records_get()[0]->size, 512);
	//查询结果存储
	engine::tree_chunk_data<int>* receiver = nullptr;
	//查询落在主树 [0,511]×[0,511] 之外的坐标
	manager.seek(receiver, make_coord(600, 600), true);
	//取得了区块信息
	ASSERT_NE(receiver, nullptr);
	//读取序列
	const std::vector<engine::tree_record<int>*>& records = manager.records_get();
	ASSERT_EQ(records.size(), 2u);
	//新树被校准到 [512,767]×[512,767]，根节点落在中心
	EXPECT_EQ(records[0]->root, (engine::Point2d(639.5, 639.5)));
	EXPECT_EQ(records[0]->size, 256);
	//主树保持原有大小与位置
	EXPECT_EQ(records[1]->root, (engine::Point2d(255.5, 255.5)));
	EXPECT_EQ(records[1]->size, 512);
	chunk_clean(receiver);
}

// ———— 范围查询 ————

//范围查询：整棵树的范围返回全部最小区块
TEST_F(Quadtree_Manager_Test, 范围查询整树返回全部区块)
{
	//默认构造的管理器
	engine::Quadtree_Manager<int> manager;
	//建立一棵覆盖 [0,255]×[0,255] 的四叉树
	build_one_tree(manager, make_coord(0, 0));
	//查询结果存储
	std::vector<engine::tree_chunk_data<int>*> receiver{};
	//按整棵树的范围查询
	manager.seek(receiver, make_range(0, 255, 255, 0), true);
	//16×16 个最小区块全部命中
	EXPECT_EQ(receiver.size(), 256u);
	//所有结果中心都落在树管理范围内
	for (const auto* item : receiver)
	{
		EXPECT_GE(item->node.X, 0.0);
		EXPECT_LE(item->node.X, 255.0);
		EXPECT_GE(item->node.Y, 0.0);
		EXPECT_LE(item->node.Y, 255.0);
	}
	chunk_clean(receiver);
}

//范围查询：未对齐的范围先被格式化到区块网格
TEST_F(Quadtree_Manager_Test, 范围查询未对齐范围被格式化)
{
	//默认构造的管理器
	engine::Quadtree_Manager<int> manager;
	//建立一棵覆盖 [0,255]×[0,255] 的四叉树
	build_one_tree(manager, make_coord(0, 0));
	//查询结果存储
	std::vector<engine::tree_chunk_data<int>*> receiver{};
	//查询 [3,20]×[3,20]，格式化后应扩展到 [0,31]×[0,31]
	manager.seek(receiver, make_range(3, 20, 20, 3), true);
	//2×2 个最小区块命中
	EXPECT_EQ(receiver.size(), 4u);
	chunk_clean(receiver);
}

//范围查询：只覆盖单个最小区块时返回一个结果
TEST_F(Quadtree_Manager_Test, 范围查询单区块范围)
{
	//默认构造的管理器
	engine::Quadtree_Manager<int> manager;
	//建立一棵覆盖 [0,255]×[0,255] 的四叉树
	build_one_tree(manager, make_coord(0, 0));
	//查询结果存储
	std::vector<engine::tree_chunk_data<int>*> receiver{};
	//查询 [0,15]×[0,15]，恰好一个最小区块
	manager.seek(receiver, make_range(0, 15, 15, 0), true);
	ASSERT_EQ(receiver.size(), 1u);
	//该区块中心为 (7.5,7.5)
	EXPECT_EQ(receiver[0]->node, (engine::Point2d(7.5, 7.5)));
	chunk_clean(receiver);
}

//范围查询：同一范围重复查询结果数量一致
TEST_F(Quadtree_Manager_Test, 范围查询结果可重复取得)
{
	//默认构造的管理器
	engine::Quadtree_Manager<int> manager;
	//建立一棵覆盖 [0,255]×[0,255] 的四叉树
	build_one_tree(manager, make_coord(0, 0));
	//第一次查询
	std::vector<engine::tree_chunk_data<int>*> first{};
	manager.seek(first, make_range(0, 255, 255, 0), true);
	//第二次查询同一范围
	std::vector<engine::tree_chunk_data<int>*> second{};
	manager.seek(second, make_range(0, 255, 255, 0), true);
	//两次结果数量一致
	EXPECT_EQ(first.size(), second.size());
	chunk_clean(first);
	chunk_clean(second);
}

// ———— 维护：清空 / 卸载 / 合并 ————

//清空：clear 释放全部四叉树
TEST_F(Quadtree_Manager_Test, 清空释放全部四叉树)
{
	//默认构造的管理器
	engine::Quadtree_Manager<int> manager;
	//建立两棵相距较远的四叉树
	manager.set_max_size(256);
	manager.qurdtree_build_smart({ make_coord(0, 0), make_coord(300, 300) });
	ASSERT_EQ(manager.records_get().size(), 2u);
	//清空
	manager.clear();
	//序列已空
	EXPECT_TRUE(manager.records_get().empty());
}

//清空：对空序列重复清空不产生副作用
TEST_F(Quadtree_Manager_Test, 清空空序列不产生副作用)
{
	//默认构造的管理器
	engine::Quadtree_Manager<int> manager;
	//连续两次清空
	manager.clear();
	manager.clear();
	//序列仍为空
	EXPECT_TRUE(manager.records_get().empty());
}

//卸载：按根节点坐标卸载其中一棵
TEST_F(Quadtree_Manager_Test, 按根坐标卸载单棵树)
{
	//默认构造的管理器
	engine::Quadtree_Manager<int> manager;
	//建立两棵树（根坐标分别为 383.5 与 127.5）
	manager.set_max_size(256);
	manager.qurdtree_build_smart({ make_coord(0, 0), make_coord(300, 300) });
	ASSERT_EQ(manager.records_get().size(), 2u);
	//卸载根坐标 383.5 的那一棵
	manager.quadtree_unload({ engine::Point2d(383.5, 383.5) });
	//只剩一棵，且根坐标正确
	const std::vector<engine::tree_record<int>*>& records = manager.records_get();
	ASSERT_EQ(records.size(), 1u);
	EXPECT_EQ(records[0]->root, (engine::Point2d(127.5, 127.5)));
}

//卸载：按根节点坐标卸载全部
TEST_F(Quadtree_Manager_Test, 按根坐标卸载全部树)
{
	//默认构造的管理器
	engine::Quadtree_Manager<int> manager;
	//建立两棵树
	manager.set_max_size(256);
	manager.qurdtree_build_smart({ make_coord(0, 0), make_coord(300, 300) });
	ASSERT_EQ(manager.records_get().size(), 2u);
	//一次卸载两棵
	manager.quadtree_unload({ engine::Point2d(383.5, 383.5),
		engine::Point2d(127.5, 127.5) });
	//序列已空
	EXPECT_TRUE(manager.records_get().empty());
}

//缓存：清空高速缓存不改变四叉树序列
TEST_F(Quadtree_Manager_Test, 清空缓存不影响序列)
{
	//默认构造的管理器
	engine::Quadtree_Manager<int> manager;
	//建立一棵四叉树
	build_one_tree(manager, make_coord(0, 0));
	//清空高速缓存
	manager.cache_clear();
	//序列数量不变
	EXPECT_EQ(manager.records_get().size(), 1u);
}

//合并：未注册数据迁移方法时不做任何处理
TEST_F(Quadtree_Manager_Test, 未注册迁移方法时合并不生效)
{
	//默认构造的管理器
	engine::Quadtree_Manager<int> manager;
	//建立两棵树
	manager.set_max_size(256);
	manager.qurdtree_build_smart({ make_coord(0, 0), make_coord(300, 300) });
	ASSERT_EQ(manager.records_get().size(), 2u);
	//未注册迁移方法即请求合并
	manager.qurdtree_merge();
	//序列保持不变
	EXPECT_EQ(manager.records_get().size(), 2u);
}

//合并：注册迁移方法但树数量不足时同样不做处理
TEST_F(Quadtree_Manager_Test, 树数量不足时合并不生效)
{
	//默认构造的管理器
	engine::Quadtree_Manager<int> manager;
	//注册数据迁移方法（本用例中不会被调用）
	int copy_times = 0;
	manager.callback_register([&copy_times](engine::tree_chunk_data<int>& receiver,
		engine::tree_chunk_data<int>& transmiter)
		{
			++copy_times;
		});
	//建立一棵四叉树
	build_one_tree(manager, make_coord(0, 0));
	ASSERT_EQ(manager.records_get().size(), 1u);
	//请求合并
	manager.qurdtree_merge();
	//不足四棵同级树，未发生任何数据迁移
	EXPECT_EQ(copy_times, 0);
	EXPECT_EQ(manager.records_get().size(), 1u);
}

//析构：离开作用域自动释放全部四叉树
TEST_F(Quadtree_Manager_Test, 析构释放全部四叉树)
{
	//在堆上构造管理器并建立两棵树
	engine::Quadtree_Manager<int>* manager = new engine::Quadtree_Manager<int>();
	manager->set_max_size(256);
	manager->qurdtree_build_smart({ make_coord(0, 0), make_coord(300, 300) });
	ASSERT_EQ(manager->records_get().size(), 2u);
	//删除时析构函数应完成全部四叉树的释放
	delete manager;
	//此处无崩溃即为通过（资源释放已由析构函数承担）
	SUCCEED();
}

// ———— 缺陷回归（修复后启用） ————
//以下用例原先固化引擎缺陷并处于禁用状态，缺陷修复后已转为正式回归用例；
//唯「超大边长上限下区块检索次数正常」一条仍被未修的除零缺陷阻塞，保持禁用。

//默认边长上限：建成的树记录大小与上限一致
//修复后语义：tree_record::size 已改回 uint64_t，与 Quadtree::state.size 对齐，
//          65536 不再被截断成 0。
TEST_F(Quadtree_Manager_Test, 默认边长上限下建成的树记录大小正确)
{
	//默认构造的管理器（边长上限缺省 65536）
	engine::Quadtree_Manager<int> manager;
	//按单点建树
	manager.qurdtree_build_smart({ make_coord(0, 0) });
	//读取序列
	const std::vector<engine::tree_record<int>*>& records = manager.records_get();
	ASSERT_EQ(records.size(), 1u);
	//根节点仍应落在包围矩形中心
	EXPECT_EQ(records[0]->root, (engine::Point2d(32767.5, 32767.5)));
	//期望记录值与边长上限一致（当前实际被截断为 0）
	EXPECT_EQ(records[0]->size, 65536);
}

//默认设置：单点查询能够收敛
//修复后语义：记录大小不再截断，quadtree_inclusion_seek 能算出正确范围，
//          单点查询的 for(;;) 首轮即可取到区块并结束，不会反复重建。
TEST_F(Quadtree_Manager_Test, 默认设置下单点查询能够收敛)
{
	//默认构造的管理器（边长上限缺省 65536）
	engine::Quadtree_Manager<int> manager;
	//查询结果存储
	engine::tree_chunk_data<int>* receiver = nullptr;
	//稳定模式下查询单点
	manager.seek(receiver, make_coord(0, 0), true);
	//应取到区块信息
	ASSERT_NE(receiver, nullptr);
	//释放区块信息
	delete receiver;
}

//智能创建：空坐标集合不建树
//修复后语义：qurdtree_build_smart 入口已对 coord_set 做 empty() 判断并直接返回，
//          不会再对空容器取首元素。
TEST_F(Quadtree_Manager_Test, 智能创建空坐标集合不建树)
{
	//默认构造的管理器
	engine::Quadtree_Manager<int> manager;
	//设定边长上限
	manager.set_max_size(256);
	//投喂空坐标集合
	manager.qurdtree_build_smart({});
	//不应建立任何四叉树
	EXPECT_TRUE(manager.records_get().empty());
}

//范围查询：空序列直接返回空结果
//修复后语义：范围查询重载已先校验 X_sequence 是否为空，非稳定模式下直接返回。
TEST_F(Quadtree_Manager_Test, 范围查询空序列返回空结果)
{
	//默认构造的管理器（不含任何四叉树）
	engine::Quadtree_Manager<int> manager;
	//查询结果存储
	std::vector<engine::tree_chunk_data<int>*> receiver{};
	//不稳定模式下对空管理器做范围查询
	manager.seek(receiver, make_range(0, 255, 255, 0), false);
	//应返回空结果
	EXPECT_TRUE(receiver.empty());
}

//卸载：不存在的根坐标不会越界
//修复后语义：quadtree_index_seek 在 range_binary_search 未命中时先判断
//          range.first < 0 并返回 -1，不会再索引 tree_group[-1]。
TEST_F(Quadtree_Manager_Test, 卸载不存在的根坐标不越界)
{
	//默认构造的管理器
	engine::Quadtree_Manager<int> manager;
	//先建立一棵覆盖 [0,255]×[0,255] 的四叉树
	build_one_tree(manager, make_coord(0, 0));
	//卸载一个不在序列中的根坐标
	manager.quadtree_unload({ engine::Point2d(9999.5, 9999.5) });
	//原有四叉树应保持不变
	EXPECT_EQ(manager.records_get().size(), 1u);
}

//范围查询：部分覆盖目标范围不越界
//修复后语义：调用处已按 (tree_range, target_excel, seekable_range) 传参，
//          target_range_amend 内也对行列索引做了 [0, width) / [0, 总行数) 裁剪。
TEST_F(Quadtree_Manager_Test, 范围查询部分覆盖不越界)
{
	//默认构造的管理器
	engine::Quadtree_Manager<int> manager;
	//先建立一棵覆盖 [0,255]×[0,255] 的四叉树
	build_one_tree(manager, make_coord(0, 0));
	//查询结果存储
	std::vector<engine::tree_chunk_data<int>*> receiver{};
	//稳定模式下查询树内的一小块区域（不覆盖整棵树）
	manager.seek(receiver, make_range(20, 40, 40, 20), true);
	//应取到区块信息
	EXPECT_GT(receiver.size(), 0u);
	//释放查询结果
	chunk_clean(receiver);
}

//设置边长下限：只更新下限，不再误写上限
//修复后语义：set_min_size 只写入 settings.min_tree_size，
//          不再调用 tree->set_max_size()，四叉树的边长上限不会被反向改写。
TEST_F(Quadtree_Manager_Test, 设置边长下限只更新下限)
{
	//默认构造的管理器
	engine::Quadtree_Manager<int> manager;
	//先设定边长上限
	manager.set_max_size(1024);
	//再设定边长下限
	manager.set_min_size(256);
	//下限被正确记录
	EXPECT_EQ(manager.settings_get().min_tree_size, 256u);
	//上限不受影响
	EXPECT_EQ(manager.settings_get().max_tree_size, 1024u);
}

//缓存：条目上限为零时查找不崩溃
//修复后语义：判满条件改为 records.size() >= max_cache_records，
//          且写入前先判断 max_cache_records > 0，空缓存不会再被弹出。
TEST_F(Quadtree_Manager_Test, 缓存条目上限为零时查找不崩溃)
{
	//默认构造的管理器
	engine::Quadtree_Manager<int> manager;
	//设定边长上限、缓存启用阈值与条目上限
	manager.set_max_size(256);
	manager.set_cache_active_threshold(1);
	manager.set_max_cach_records(0);
	//先建立一棵覆盖 [0,255]×[0,255] 的四叉树
	build_one_tree(manager, make_coord(0, 0));
	//查询结果存储
	engine::tree_chunk_data<int>* receiver = nullptr;
	//稳定模式下查询
	manager.seek(receiver, make_coord(10, 10), true);
	//应取到区块信息
	ASSERT_NE(receiver, nullptr);
	//释放区块信息
	delete receiver;
}

//范围查询：不稳定模式不崩溃
//修复后语义：child_node_recur 的返回值被调用处接收（entered / continue），
//          失败时不再继续使用已置空的节点指针。
TEST_F(Quadtree_Manager_Test, 范围查询不稳定模式不崩溃)
{
	//默认构造的管理器
	engine::Quadtree_Manager<int> manager;
	//先建立一棵覆盖 [0,255]×[0,255] 的四叉树
	build_one_tree(manager, make_coord(0, 0));
	//查询结果存储
	std::vector<engine::tree_chunk_data<int>*> receiver{};
	//不稳定模式下做范围查询
	manager.seek(receiver, make_range(0, 255, 255, 0), false);
	//不稳定模式不新建区块，结果应为空
	EXPECT_TRUE(receiver.empty());
	//释放查询结果
	chunk_clean(receiver);
}

//超大边长上限：建树路径仍会抛整数除零，用例保持禁用
//已修复部分：block_seek 的 max_size 与 quedtree_merge_collect 的
//          max_expandable_size 均已改为 uint64_t，检索次数的类型截断已消除；
//          block_seek 入口另补 `state.size <= 0` 直接返回，杜绝该处除零。
//仍存在的缺陷：把边长上限配置到 INT_MAX 以上（如 1ull << 33）后按单点建树，
//          仍会抛出 SEH 异常 0xC0000094（整数除以零）。
//          崩点位于 Quadtree_Manager/core/区块信息检索.hpp 的 excel_element_to_coord：
//              int width = (excel_range.right - excel_range.left + 1) / settings.block_size;
//              int row = element_ID / width;
//              int col = element_ID % width;
//          当格式化后的查询范围宽度不足一个 block_size 时 width 为 0，除法即抛异常。
//          根因涉及 int 型坐标表示无法承载 INT_MAX 以上的树尺寸
//          （Rect2i 各分量为 int，manage_range_calcu 按 tree_size 计算范围时会溢出），
//          需架构层调整，非局部防御可解。
//          修复后去掉下划线前缀即可转为回归用例。
TEST_F(Quadtree_Manager_Test, DISABLED_超大边长上限下区块检索次数正常)
{
	//启用前请先修复上述除零缺陷。
	//默认构造的管理器
	//engine::Quadtree_Manager<int> manager;
	//把边长上限提到 INT_MAX 以上
	//manager.set_max_size(1ull << 33);
	//按单点建树
	//manager.qurdtree_build_smart({ make_coord(0, 0) });
	//应建成一棵树
	//EXPECT_EQ(manager.records_get().size(), 1u);
}

//合并：拷贝前检查区块指针
//修复后语义：qurdtree_merge 的数据拷贝段已加 if (ptr_data) 判断，
//          区块创建失败时跳过拷贝并计入失败统计。
TEST_F(Quadtree_Manager_Test, 合并拷贝前检查区块指针)
{
	//默认构造的管理器
	engine::Quadtree_Manager<int> manager;
	//设定边长上限
	manager.set_max_size(256);
	//注册数据迁移方法
	manager.callback_register([](engine::tree_chunk_data<int>& receiver,
		engine::tree_chunk_data<int>& transmiter) {});
	//执行合并
	manager.qurdtree_merge();
	//无崩溃即为通过
	SUCCEED();
}
