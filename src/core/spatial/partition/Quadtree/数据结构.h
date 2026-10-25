#pragma once
//预编译头
#include "Engine/EngineCore/common/前置头文件包含.h"
//获取预定义坐标类型
#include "../../common/core/坐标类型.h"

//游戏引擎命名空间
namespace engine
{
	//四叉树管理器前向声明
	template <typename T>
	class Quadtree_Manager;
	//四叉树前向声明
	template <typename T>
	class Quadtree;

	//四叉树状态结构体
	struct Tree_State
	{
	public:
		//为避免根节点中心偏移现象
		//故采用小数坐标
		//根节点坐标
		Point2d root = { 0.5,0.5 };
		//四叉树大小
		uint64_t size = 0;
		//四叉树大小上限
		uint64_t max_size = 9223372036854775808;//初始化2的63次方
		//最小区块单元大小
		uint64_t block_size = 0;
	};

	//区块信息结构体
	template<typename T>
	struct Tree_Chunk_Data
	{
		//默认构造函数
		Tree_Chunk_Data() = default;
		//列表构造函数：直接接收坐标和数据
		Tree_Chunk_Data(double x, double y, std::shared_ptr<T> ptr)
		{
			center.X = x;
			center.Y = y;
			data = std::move(ptr);
		}
		//列表构造函数：直接接收中心坐标和数据
		Tree_Chunk_Data(const Point2d& point, std::shared_ptr<T> ptr)
		{
			center = point;
			data = std::move(ptr);
		}
		//默认析构函数
		~Tree_Chunk_Data() = default;

		//区块中心坐标
		Point2d center = { 0.5,0.5 };
		//区块挂载数据
		std::shared_ptr<T> data = nullptr;
	};
}
