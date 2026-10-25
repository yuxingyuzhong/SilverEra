#pragma once
//预编译头
#include "Engine/EngineCore/common/前置头文件包含.h"
//获取预定义坐标类型
#include "../../common/core/坐标类型.h"
//获取预定义数据结构(用于函数返回值)
#include "数据结构.h"

namespace engine
{
	template<typename T>
	//四叉树模板
	class Quadtree
	{
	private:
		//节点种类枚举
		enum class Node_Kind
		{
			//中间节点
			Middle,
			//叶子节点
			Leaf
		};
		//节点结构体
		struct Node
		{
			//节点数据
			//中间节点存放四个子节点指针，叶子节点存放区块数据
			std::variant<std::array<std::shared_ptr<Node>, 4>, std::shared_ptr<T>> data;

			//按节点种类构造(默认为中间节点)
			explicit Node(Node_Kind kind = Node_Kind::Middle, std::shared_ptr<T> external_data = nullptr)
			{
				//若为中间节点
				if (kind == Node_Kind::Middle)
					//子节点指针一律置空
					data.template emplace<0>(std::array<std::shared_ptr<Node>, 4>{ nullptr, nullptr, nullptr, nullptr });
				//若为叶子节点且外界传入数据
				else if (external_data)
					//挂载外界数据
					data.template emplace<1>(std::move(external_data));
				//若为叶子节点且外界未传入数据
				else
					//分配默认区块数据
					data.template emplace<1>(std::shared_ptr<T>(new(std::nothrow) T()));
			}
		};
		//区块操作模式枚举
		enum class Chunk_Mode
		{
			//构建
			Build,
			//查找
			Seek,
			//获取
			Get,
			//卸载
			Unload
		};
		//节点递归方向枚举
		enum Quadrant { NW, NE, SW, SE };

		//四叉树根节点
		Node root;
		//四叉树状态记录
		Tree_State state;
		//外界上级管理对象回调管理方法----四叉树扩大行为权限申请
		//目标坐标取自范围推导结果，同样按 64 位整数通报
		std::function<bool(const Point2d& root, const Point2l& target)> callback;

	public:
		//构造函数
		explicit Quadtree(const uint64_t& size = 256, const Point2d& root = { 0.5,0.5 });
		//析构函数
		~Quadtree(void) = default;

		// ---- 设置 ----

		//最小区块单元大小设置
		bool set_block_size(const uint64_t& size);
		//四叉树边长上限设置
		bool set_max_size(const uint64_t& size);
		//四叉树回调管理方法设置
		void set_callback_manage
		(const std::function<bool(Point2d root, Point2l target)>& cb);

		// ---- 区块操作 ----

		//区块构建 —— 单区块重载(可直接挂载数据)
		void build(const Point2l& target, const std::shared_ptr<T>& data = nullptr);
		//区块构建 —— 范围重载(不可直接挂载数据)
		void build(const Rect2l& target_range);

		//区块查找 —— 单区块重载(命中返回 true)
		bool seek(const Point2l& target, Tree_Chunk_Data<T>& receiver);
		//区块查找 —— 范围重载
		void seek(const Rect2l& target_range, std::vector<Tree_Chunk_Data<T>>& receiver);

		//区块获取 —— 单区块重载(命中或新建返回 true)
		bool get(const Point2l& target, Tree_Chunk_Data<T>& receiver);
		//区块获取 —— 范围重载
		void get(const Rect2l& target_range, std::vector<Tree_Chunk_Data<T>>& receiver);

		//区块卸载 —— 单区块重载
		void unload(const Point2l& target);
		//区块卸载 —— 范围重载
		void unload(const Rect2l& target_range);

		//四叉树状态获取
		const Tree_State& tree_state_get(void) const;

		//四叉树扩大
		bool tree_expand(void);

		// ———— 几何计算工具（管理器复用） ————
	public:
		//点属区块范围计算
		Rect2l chunk_range_calcu(const Point2l& target) const;

		//子节点范围计算
		Rect2l child_range_calcu(const int& quadrant, const Rect2l& parent_range) const;

		//范围中心计算
		Point2d center_calcu(const Rect2l& range) const;

		//矩形范围计算
		Rect2l rect_range_calcu(const Point2d& root, const uint64_t& size) const;

		//待查询范围格式化(对齐到区块网格)
		void range_align(Rect2l& target_range, const Point2d& root, const uint64_t& block_size) const;

		//可查询范围求交(返回越界标记)
		Point2d range_intersect(const Rect2l& target_range, Rect2l& reachable_range) const;

	private:
		//递归级数计算
		int depth_calcu(void) const;

		//范围相交判定
		bool range_overlap_check(const Rect2l& target_range, const Rect2l& node_range) const;

		// ———— 结构维护 ————
	private:
		//子节点推进
		bool child_step(Node*& this_node, const int& quadrant, const Node_Kind& kind,
			bool read_only, const std::shared_ptr<T>& external_data = nullptr);

		//区块递归
		void chunk_recur(Node* node, const Rect2l& node_range, const Rect2l& target_range,
			int now_level, const int& max_level, bool read_only,
			const Chunk_Mode& mode, std::vector<Tree_Chunk_Data<T>>& receiver,
			const std::shared_ptr<T>& data);

		//叶子槽操作
		void leaf_slot_apply(Node* node, const Rect2l& node_range, const Rect2l& target_range,
			const Chunk_Mode& mode, bool read_only,
			std::vector<Tree_Chunk_Data<T>>& receiver,
			const std::shared_ptr<T>& data);

		//区块操作 —— 单区块重载
		void chunk_dispatch(const Point2l& target, const Chunk_Mode& mode,
			std::vector<Tree_Chunk_Data<T>>& receiver,
			const std::shared_ptr<T>& data = nullptr);

		//区块操作 —— 范围重载
		void chunk_dispatch(const Rect2l& target_range, const Chunk_Mode& mode,
			std::vector<Tree_Chunk_Data<T>>& receiver,
			const std::shared_ptr<T>& data = nullptr);

		// ———— 查询前置支撑 ————
	private:
		//扩大可行性分析
		void expand_analyse(const Rect2l& target_range, Rect2l& reachable_range, bool read_only);
	};
}
