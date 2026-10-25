#pragma once
#include "../函数预声明.h"

//引擎命名空间
namespace engine
{
    //计算初始包围矩形及最大区块划分参数
    template<typename T>
    void Quadtree_Manager<T>::smart_params_prepare(const std::vector<Point2l>& coord_set,
        Rect2l& rect_range, uint64_t& father_block_num_all, uint64_t& father_block_size) const
    {
        //初始化包围矩形
        rect_range.left = coord_set.front().X;
        rect_range.right = coord_set.front().X;
        rect_range.up = coord_set.front().Y;
        rect_range.down = coord_set.front().Y;

        //矩形边界确认
        for (const Point2l& coord : coord_set)
        {
            if (rect_range.left > coord.X)
                rect_range.left = coord.X;
            if (rect_range.right < coord.X)
                rect_range.right = coord.X;
            if (rect_range.up < coord.Y)
                rect_range.up = coord.Y;
            if (rect_range.down > coord.Y)
                rect_range.down = coord.Y;
        }

        //最大区块边长取上限
        father_block_size = settings.max_tree_size;
        //最大区块边长非正防御
        if (father_block_size == 0)
        {
            father_block_num_all = 0;
            return;
        }

        //计算矩形宽度并补齐边界
        uint64_t width = static_cast<uint64_t>(rect_range.right - rect_range.left + 1);
        if (width % father_block_size != 0)
        {
            rect_range.right += static_cast<int64_t>(father_block_size - (width % father_block_size));
            width += father_block_size - (width % father_block_size);
        }
        //计算X轴最大区块数目
        uint64_t father_block_num_X = width / father_block_size;

        //计算矩形高度并补齐边界
        uint64_t height = static_cast<uint64_t>(rect_range.up - rect_range.down + 1);
        if (height % father_block_size != 0)
        {
            rect_range.up += static_cast<int64_t>(father_block_size - (height % father_block_size));
            height += father_block_size - (height % father_block_size);
        }
        //计算Y轴最大区块数目
        uint64_t father_block_num_Y = height / father_block_size;

        //总区块数溢出拦截
        if (father_block_num_X != 0 &&
            father_block_num_Y > (std::numeric_limits<uint64_t>::max)() / father_block_num_X)
        {
            father_block_num_all = 0;
            return;
        }
        father_block_num_all = father_block_num_X * father_block_num_Y;
    }

    //单个最大区块的深度划分
    template<typename T>
    void Quadtree_Manager<T>::father_block_divide(const std::vector<Point2l>& coord_set,
        int64_t block_left, int64_t block_right, int64_t block_up, int64_t block_down,
        uint64_t block_size, std::vector<Point2d>& node_centers,
        std::vector<uint64_t>& tree_sizes) const
    {
        //判断点是否在当前区块内
        auto in_block = [&](const Point2l& point) -> bool
            {
                return point.X >= block_left && point.X <= block_right &&
                    point.Y >= block_down && point.Y <= block_up;
            };

        //收集当前区块内的点
        std::vector<Point2l> local_points{};
        for (const Point2l& point : coord_set)
            if (in_block(point))
                local_points.push_back(point);

        //无点则不建树
        if (local_points.empty())
            return;

        //计算当前区块中心
        double center_x = block_left + (block_right - block_left) / 2.0;
        double center_y = block_down + (block_up - block_down) / 2.0;

        //到达最小树尺寸则直接建树
        if (block_size <= settings.min_tree_size)
        {
            node_centers.push_back({ center_x, center_y });
            tree_sizes.push_back(block_size);
            return;
        }

        //判断点集是否中心聚集
        double limit = 0.375 * static_cast<double>(block_size);
        bool centered = true;
        for (const Point2l& point : local_points)
        {
            if (std::fabs(point.X - center_x) > limit || std::fabs(point.Y - center_y) > limit)
            {
                centered = false;
                break;
            }
        }

        //若不中心聚集则在当前区块建树
        if (!centered)
        {
            node_centers.push_back({ center_x, center_y });
            tree_sizes.push_back(block_size);
            return;
        }

        //中心聚集则尝试继续细分
        int64_t half = static_cast<int64_t>(block_size / 2);
        //四个子区块边界
        struct Sub_Rect { int64_t left, right, up, down; };
        Sub_Rect subs[4] =
        {
            { block_left, block_left + half - 1, block_up, block_down + half },
            { block_left + half, block_right, block_up, block_down + half },
            { block_left, block_left + half - 1, block_down + half - 1, block_down },
            { block_left + half, block_right, block_down + half - 1, block_down }
        };

        //将点分配到四个子区块
        std::vector<Point2l> sub_points[4];
        for (const Point2l& point : local_points)
        {
            if (point.X <= block_left + half - 1)
            {
                if (point.Y >= block_down + half)
                    sub_points[0].push_back(point);
                else
                    sub_points[2].push_back(point);
            }
            else
            {
                if (point.Y >= block_down + half)
                    sub_points[1].push_back(point);
                else
                    sub_points[3].push_back(point);
            }
        }

        //统计非空子区块数量
        int nonempty = 0;
        int target = -1;
        for (int index = 0; index < 4; index++)
        {
            if (!sub_points[index].empty())
            {
                nonempty++;
                target = index;
            }
        }

        //点全部落入同一子区块则递归细分该子区块
        if (nonempty == 1)
        {
            const Sub_Rect& sub = subs[target];
            father_block_divide(sub_points[target], sub.left, sub.right, sub.up, sub.down,
                half, node_centers, tree_sizes);
        }
        //点分散到多个子区块则在当前区块建树
        else
        {
            node_centers.push_back({ center_x, center_y });
            tree_sizes.push_back(block_size);
        }
    }

    //空序列下的首轮智能创建
    template<typename T>
    void Quadtree_Manager<T>::tree_build_smart_first(const std::vector<Point2l>& coord_set)
    {
        //准备包围矩形与最大区块参数
        Rect2l rect_range{};
        uint64_t father_block_num_all = 0;
        uint64_t father_block_size = 0;
        smart_params_prepare(coord_set, rect_range, father_block_num_all, father_block_size);

        //参数非法则直接结束
        if (father_block_num_all == 0 || father_block_size == 0)
            return;

        //各区块内坐标个数记录
        std::vector<uint64_t> father_block_coord_count(father_block_num_all, 0);

        //X轴最大区块数目
        uint64_t father_block_num_X =
            static_cast<uint64_t>(rect_range.right - rect_range.left + 1) / father_block_size;

        //统计各区块包含坐标数
        for (const Point2l& coord : coord_set)
        {
            //计算当前点所在列号与行号
            uint64_t col_index = static_cast<uint64_t>(coord.X - rect_range.left) / father_block_size;
            uint64_t row_index = static_cast<uint64_t>(rect_range.up - coord.Y) / father_block_size;
            //合成一维区块索引
            uint64_t index = row_index * father_block_num_X + col_index;
            father_block_coord_count[index]++;
        }

        //待创建四叉树根节点与大小记录
        std::vector<Point2d> node_centers{};
        std::vector<uint64_t> tree_sizes{};

        //遍历最大区块并划分
        for (uint64_t find_index = 0; find_index < father_block_num_all; find_index++)
        {
            //跳过空区块
            if (father_block_coord_count[find_index] == 0)
                continue;

            //计算区块左边界与右边界(闭区间)
            int64_t left = rect_range.left +
                static_cast<int64_t>((find_index % father_block_num_X) * father_block_size);
            int64_t right = left + static_cast<int64_t>(father_block_size) - 1;
            //计算区块上边界与下边界(闭区间)
            int64_t up = rect_range.up -
                static_cast<int64_t>((find_index / father_block_num_X) * father_block_size);
            int64_t down = up - static_cast<int64_t>(father_block_size) + 1;

            //划分单个最大区块
            father_block_divide(coord_set, left, right, up, down,
                father_block_size, node_centers, tree_sizes);
        }

        //统一创建四叉树
        for (size_t create_time = 0; create_time < node_centers.size(); create_time++)
            tree_build(node_centers[create_time], tree_sizes[create_time]);
    }

    //四叉树智能创建主函数
    template<typename T>
    void Quadtree_Manager<T>::tree_build_smart(const std::vector<Point2l>& coord_set)
    {
        //空坐标集直接返回
        if (coord_set.empty())
            return;

        //若序列为空则执行首轮智能创建
        if (X_sequence.empty())
        {
            tree_build_smart_first(coord_set);
            return;
        }

        //若序列非空则确保每个坐标均被覆盖
        for (const Point2l& coord : coord_set)
            ensure_tree(coord);
    }
}
