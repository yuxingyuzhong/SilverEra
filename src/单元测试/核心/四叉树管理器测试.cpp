//四叉树管理器测试：覆盖设置读写、智能创建、单点查询、越界联动、范围查询、合并与卸载
#include <gtest/gtest.h>

//获取四叉树管理器
#include "src/core/spatial/partition/Quadtree_Manager/四叉树管理器.h"

//四叉树管理器测试夹具
class Quadtree_Manager_Test : public ::testing::Test
{
public:
	//把边长上限压到 256，再按目标坐标建一棵四叉树
	//PS:tree_record 的大小字段是 uint16_t，而设置里的边长上限缺省为 65536，
	//   即"默认上限下建出的树，其记录大小必然溢出"。相关缺陷见本文件末尾的禁用用例，
	//   因此凡涉及查询的用例都先把上限压到 uint16_t 装得下的 256。
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

// ———— 缺陷固化（修复前禁用） ————
//以下用例记录当前实现中的缺陷，默认不执行。
//每一项都按"修复后即可去掉下划线前缀转为回归用例"的方式编写。

//缺陷位置：四叉树管理器通信结构体.h 的 tree_record::size
//成因：size 声明为 uint16_t，而 tree_manager_settings::max_tree_size 的缺省值是 65536
//     （且可配置到更大）。quadtree_build 里 new_tree->size = tree_size 会把 65536
//     截断成 0，于是记录里的大小与四叉树自身状态不一致。
//修复方向：把 tree_record::size 的类型改回 uint64_t，与 Quadtree::state.size 保持一致。
TEST_F(Quadtree_Manager_Test, DISABLED_默认边长上限下建成的树记录大小正确)
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

//缺陷位置：四叉树管理器通信结构体.h 的 tree_record::size（承接上一项）
//成因：记录大小被截断成 0 后，quadtree_inclusion_seek 调用 manage_range_calcu
//     会算出 left > right 的空范围，于是任何坐标都找不到直属四叉树；
//     单点查询的 for(;;) 便会每轮重新走一次智能创建与扩大审批，
//     而记录大小始终为 0，循环永不收敛（每轮还会真的扩大一次树，内存持续增长）。
//修复方向：同上一项。修复前本用例会挂死，故只能禁用。
TEST_F(Quadtree_Manager_Test, DISABLED_默认设置下单点查询能够收敛)
{
	//启用前请先修复上述缺陷，否则本用例会陷入死循环并持续占用内存。
	//engine::Quadtree_Manager<int> manager;
	//engine::tree_chunk_data<int>* receiver = nullptr;
	//manager.seek(receiver, make_coord(0, 0), true);
	//EXPECT_NE(receiver, nullptr);
	//delete receiver;
}

//缺陷位置：Quadtree_Manager/core/四叉树智能创建.hpp 的 prepare_smart_create_params
//成因：函数首行直接取 coord_set.front() 与 .back() 一侧的边界，没有任何空集合校验，
//     空坐标集合会触发空容器取首元素（未定义行为）。
//修复方向：在 qurdtree_build_smart 入口对 coord_set 做 empty() 判断并直接返回，
//     或在 prepare_smart_create_params 内先行处理空集合并把最大区块数置零。
//修复前本用例会崩溃，故只能禁用。
TEST_F(Quadtree_Manager_Test, DISABLED_智能创建空坐标集合不建树)
{
	//启用前请先修复上述缺陷，否则本用例会访问空容器。
	//engine::Quadtree_Manager<int> manager;
	//manager.set_max_size(256);
	//manager.qurdtree_build_smart({});
	//EXPECT_TRUE(manager.records_get().empty());
}

//缺陷位置：Quadtree_Manager/core/区块信息检索.hpp 的范围查询重载
//成因：函数开篇即取 tree_group.front() 以获取基准树，未校验序列是否为空；
//     在没有任何四叉树时调用范围查询会取空容器首元素（未定义行为）。
//修复方向：进入查询前判断 X_sequence 为空则直接返回，或先按范围调用智能创建。
//修复前本用例会崩溃，故只能禁用。
TEST_F(Quadtree_Manager_Test, DISABLED_范围查询空序列返回空结果)
{
	//启用前请先修复上述缺陷，否则本用例会访问空容器。
	//engine::Quadtree_Manager<int> manager;
	//std::vector<engine::tree_chunk_data<int>*> receiver{};
	//manager.seek(receiver, make_range(0, 255, 255, 0), false);
	//EXPECT_TRUE(receiver.empty());
}

//缺陷位置：Quadtree_Manager/core/基础操作.hpp 的 quadtree_index_seek 与 quadtree_unload
//成因：quadtree_index_seek 在查不到时返回 -1，而两个 quadtree_unload
//     都只判断 index < 0 就 continue（按根坐标的重载有一处判断，按索引的重载同样）
//     ——但 quadtree_index_seek 内部先调用 range_binary_search，
//     该函数未命中时返回 {-1,-1}，于是 for 循环仍会执行一次并访问 tree_group[-1]。
//修复方向：在 quadtree_index_seek 调用 range_binary_search 后先判断 range.first < 0 再进入循环。
//修复前本用例会越界访问，故只能禁用。
TEST_F(Quadtree_Manager_Test, DISABLED_卸载不存在的根坐标不越界)
{
	//启用前请先修复上述缺陷，否则本用例会越界访问序列。
	//engine::Quadtree_Manager<int> manager;
	//build_one_tree(manager, make_coord(0, 0));
	//manager.quadtree_unload({ engine::Point2d(9999.5, 9999.5) });
	//EXPECT_EQ(manager.records_get().size(), 1u);
}

//缺陷位置：Quadtree_Manager/core/区块信息检索.hpp 的 target_range_amaed
//成因：该函数签名的形参顺序是 (excel_range, ptr_excel, target_range)，
//     而范围查询的调用处传入的是 (seekable_range, target_excel, tree_range)，
//     两者语义被颠倒：本应作为"整表范围"的树管理范围被当成目标范围，
//     于是 X_start/Y_start 会算出负数、X_end 会超出列宽，ptr_excel[row * width + col]
//     直接写到数组边界之外。只有"查询范围恰好覆盖整棵树"时索引才恰好落回界内。
//修复方向：调用处改为 target_range_amaed(tree_range, target_excel, seekable_range)，
//     并在函数内对行列索引做 [0, width) / [0, 总行数) 的裁剪。
//修复前本用例会破坏堆内存，故只能禁用。
TEST_F(Quadtree_Manager_Test, DISABLED_范围查询部分覆盖不越界)
{
	//启用前请先修复上述缺陷，否则本用例会写出结果表边界。
	//engine::Quadtree_Manager<int> manager;
	//build_one_tree(manager, make_coord(0, 0));
	//std::vector<engine::tree_chunk_data<int>*> receiver{};
	//manager.seek(receiver, make_range(20, 40, 40, 20), true);
	//EXPECT_GT(receiver.size(), 0u);
}

//缺陷位置：Quadtree_Manager/core/设置与交互.hpp 的 set_min_size
//成因：函数把下限记录进 settings.min_tree_size 之后，遍历现有四叉树时调用的却是
//     tree->set_max_size(settings.min_tree_size) —— 下限被写成了四叉树的上限。
//     一旦下限大于上限，四叉树的边长上限会被反向放大；
//     反之则被反向压缩，两种情况下四叉树自身的扩大判定都不再可信。
//修复方向：此处应调用 tree->set_block_size 之外的对应接口；Quadtree 目前没有
//     set_min_size，需要补一个并在此处改调它。
//说明：该副作用无法从管理器的公开接口直接观测（tree 是 tree_record 的私有成员），
//     故本用例仅作记录，修复后应改为通过四叉树状态读取接口断言。
TEST_F(Quadtree_Manager_Test, DISABLED_设置边长下限写入的是四叉树上限)
{
	//启用前请先为 Quadtree 补上 set_min_size 接口并修正调用点。
	//engine::Quadtree_Manager<int> manager;
	//manager.set_max_size(1024);
	//manager.set_min_size(256);
	//EXPECT_EQ(manager.settings_get().min_tree_size, 256u);
}

//缺陷位置：Quadtree_Manager/core/基础操作.hpp 的 quadtree_inclusion_seek（缓存写入段）
//成因：缓存条目已满时直接 records.pop_back() / ranges.pop_back()，
//     判满条件写作 records.size() == max_cache_records。
//     当外界把条目上限设为 0 时，条件对空容器成立，pop_back 会作用在空 vector 上。
//修复方向：改为 records.size() >= max_cache_records 且写入前判断 max_cache_records > 0。
//修复前本用例会触发未定义行为，故只能禁用。
TEST_F(Quadtree_Manager_Test, DISABLED_缓存条目上限为零时查找不崩溃)
{
	//启用前请先修复上述缺陷，否则本用例会对空缓存容器执行弹出。
	//engine::Quadtree_Manager<int> manager;
	//manager.set_max_size(256);
	//manager.set_cache_active_threshold(1);
	//manager.set_max_cach_records(0);
	//build_one_tree(manager, make_coord(0, 0));
	//engine::tree_chunk_data<int>* receiver = nullptr;
	//manager.seek(receiver, make_coord(10, 10), true);
	//EXPECT_NE(receiver, nullptr);
	//delete receiver;
}

//缺陷位置：Quadtree / Quadtree_Manager 的 range_seek 路径（Quadtree 的区块信息检索.hpp）
//成因：range_seek 在中间层调用 child_node_recur 时，若子节点为空且 stable == false，
//     该函数会把传入的 parent_node 置为 nullptr 并返回 false；
//     但调用处只据此设置 is_pop_back，当同层存在多于一个相交子块（recursive_num > 1）时
//     会把 is_pop_back 改回 false 并直接进入下一轮循环，
//     下一轮再对已为空的 parent_node 递归，形成空指针解引用。
//     因此范围查询的不稳定模式在首次遍历时就会崩溃。
//修复方向：child_node_recur 失败时不要把引用参数写成空指针（改为返回子节点指针），
//     或在调用处失败即弹栈/跳出，不再使用 parent_node。
//修复前本用例会崩溃，故只能禁用。
TEST_F(Quadtree_Manager_Test, DISABLED_范围查询不稳定模式不崩溃)
{
	//启用前请先修复上述缺陷，否则本用例会解引用空节点。
	//engine::Quadtree_Manager<int> manager;
	//build_one_tree(manager, make_coord(0, 0));
	//std::vector<engine::tree_chunk_data<int>*> receiver{};
	//manager.seek(receiver, make_range(0, 255, 255, 0), false);
	//EXPECT_TRUE(receiver.empty());
}

//缺陷位置：Quadtree 的区块信息检索.hpp 的 block_seek（检测次数计算段）
//成因：函数把 uint64_t 的 state.max_size 直接赋给 int 类型的 max_size。
//     当边长上限配置到超过 INT_MAX 时赋值发生截断，负值参与无符号除法后
//     会得到一个极大的 exam_time_max，区块检索会进行长时间的无效循环。
//     同理，Quadtree_Manager 的 quedtree_merge_collect 里
//     int max_expandable_size = largest_tree_size 也存在相同的截断风险。
//修复方向：两处局部变量均改为 uint64_t。
//说明：触发需要把边长上限配置到 INT_MAX 以上，本用例仅作记录。
TEST_F(Quadtree_Manager_Test, DISABLED_超大边长上限下区块检索次数正常)
{
	//启用前请先修复上述缺陷。
	//engine::Quadtree_Manager<int> manager;
	//manager.set_max_size(1ull << 33);
	//manager.qurdtree_build_smart({ make_coord(0, 0) });
	//EXPECT_EQ(manager.records_get().size(), 1u);
}

//缺陷位置：Quadtree_Manager/core/四叉树合并.hpp 的 qurdtree_merge（数据拷贝段）
//成因：稳定查询 new_tree->tree->block_seek(ptr_data, ...) 之后直接
//     copy(*ptr_data, *buffer[copy_time])，没有判断 ptr_data 是否为空。
//     新区块分配失败或新树拒绝该坐标时 ptr_data 会保持 nullptr，
//     此时对空指针解引用。
//修复方向：拷贝前判断 ptr_data != nullptr，为空则跳过并计入失败统计。
//修复前本用例会崩溃（需要构造分配失败场景），故只能禁用。
TEST_F(Quadtree_Manager_Test, DISABLED_合并拷贝前检查区块指针)
{
	//启用前请先修复上述缺陷；构造四棵同级相邻四叉树即可进入拷贝段。
	//engine::Quadtree_Manager<int> manager;
	//manager.set_max_size(256);
	//manager.callback_register([](engine::tree_chunk_data<int>& receiver,
	//	engine::tree_chunk_data<int>& transmiter) {});
	//manager.qurdtree_merge();
	//SUCCEED();
}
