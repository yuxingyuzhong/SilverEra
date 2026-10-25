#pragma once
#include "../函数预声明.h"

//引擎命名空间
namespace engine
{
    //回调管理四叉树查找
    template<typename T>
    Tree_Record<T>* Quadtree_Manager<T>::callback_tree_seek(const Point2d& root) const
    {
        //寻找回调管理四叉树
        for (Tree_Record<T>* record : X_sequence)
        {
            //若根节点坐标一致则为目标树
            if (root == record->root)
                return record;
        }
        //若查找失败则返回空指针
        return nullptr;
    }

    //新四叉树管理范围计算
    template<typename T>
    void Quadtree_Manager<T>::new_tree_range_calcu(const Tree_Record<T>* primary_tree,
        const Point2l& target, Rect2l& new_tree) const
    {
        //树大小按 64 位有符号整数参与边界运算
        int64_t min_size = static_cast<int64_t>(settings.min_tree_size);
        const Point2d& root = primary_tree->root;

        //计算新四叉树起始范围
        new_tree = primary_tree->tree->rect_range_calcu(root, settings.min_tree_size);

        //若基准四叉树大小不等于下限则先做半格校准
        if (primary_tree->size != settings.min_tree_size)
        {
            //X轴偏移量
            int64_t offset_x = min_size / 2;
            //Y轴偏移量
            int64_t offset_y = min_size / 2;

            //若目标X轴坐标小于根节点坐标
            if (target.X < root.X)
                offset_x = -offset_x;
            //若目标Y轴坐标小于根节点坐标
            if (target.Y < root.Y)
                offset_y = -offset_y;

            //校准范围
            new_tree.left += offset_x;
            new_tree.right += offset_x;
            new_tree.up += offset_y;
            new_tree.down += offset_y;
        }

        //滑动新四叉树范围直至包含目标点
        for (;;)
        {
            //若新四叉树左边界在目标右侧
            if (new_tree.left > target.X)
            {
                new_tree.left -= min_size;
                new_tree.right -= min_size;
            }
            //若新四叉树右边界在目标左侧
            else if (new_tree.right < target.X)
            {
                new_tree.left += min_size;
                new_tree.right += min_size;
            }
            //若新四叉树上边界在目标下侧
            if (new_tree.up < target.Y)
            {
                new_tree.up += min_size;
                new_tree.down += min_size;
            }
            //若新四叉树下边界在目标上侧
            else if (new_tree.down > target.Y)
            {
                new_tree.up -= min_size;
                new_tree.down -= min_size;
            }

            //若已包含目标则结束
            if (target.X >= new_tree.left && target.X <= new_tree.right &&
                target.Y >= new_tree.down && target.Y <= new_tree.up)
                break;
        }
    }

    //扩大管理方法
    template<typename T>
    bool Quadtree_Manager<T>::tree_expand_approve(const Point2d& root, const Point2l& target, bool internal)
    {
        //获取回调管理四叉树
        Tree_Record<T>* now_tree = callback_tree_seek(root);
        if (now_tree == nullptr)
            return false;

        //若目标点已被其他四叉树覆盖则直接拒绝
        if (tree_inclusion_seek(target) != nullptr)
            return false;

        //若当前四叉树未达上限则尝试原地扩大
        if (now_tree->size < settings.max_tree_size)
        {
            //扩大后的管理范围
            Rect2l expanded_range = now_tree->tree->rect_range_calcu(now_tree->root, now_tree->size * 2);
            //检查扩大范围内是否已有其他四叉树根节点
            std::vector<Tree_Record<T>*> inner_trees{};
            rect_filter(inner_trees, now_tree, expanded_range);

            //默认扩大可行
            bool expandable = inner_trees.empty();

            //若扩大范围内无其他树根则继续检查管理范围重叠
            if (expandable)
            {
                //记录原大小
                uint64_t old_size = now_tree->size;
                //临时按扩大后大小检查
                now_tree->size *= 2;

                //查找相邻四叉树
                std::vector<Tree_Record<T>*> neighbors{};
                neighbor_seek(neighbors, now_tree);

                //若存在相邻四叉树则确认是否重叠
                if (!neighbors.empty())
                {
                    std::vector<Tree_Record<T>*> overlap{};
                    auto screen_method = [](double coord_1, double coord_2, double coord_3, double coord_4) -> bool
                        {
                            return coord_1 > coord_2 - 1 && coord_3 > coord_4 - 1;
                        };
                    neighbor_verify(overlap, now_tree, neighbors, screen_method);
                    expandable = overlap.empty();
                }

                //若不可扩大则恢复原大小
                if (!expandable)
                    now_tree->size = old_size;
            }

            //若扩大可行则执行
            if (expandable)
            {
                //若为内部调用则由管理器完成实体扩大
                if (internal && !now_tree->tree->tree_expand())
                {
                    now_tree->size /= 2;
                    return false;
                }

                //维护最大四叉树边长
                if (now_tree->size > largest_tree_size)
                    largest_tree_size = now_tree->size;

                //更新高速缓存范围
                for (int exam_time = 0; exam_time < tree_cache.records.size(); exam_time++)
                {
                    if (now_tree == tree_cache.records[exam_time])
                        tree_cache.ranges[exam_time] =
                        now_tree->tree->rect_range_calcu(now_tree->root, now_tree->size);
                }
                return true;
            }
        }

        //原地扩大不可行，改为在目标附近新建一棵最小尺寸四叉树
        Rect2l new_tree_range{};
        new_tree_range_calcu(now_tree, target, new_tree_range);

        //检查新树范围是否与其他四叉树重叠
        for (Tree_Record<T>* record : X_sequence)
        {
            if (record == now_tree)
                continue;
            Rect2l record_range = record->tree->rect_range_calcu(record->root, record->size);
            Rect2l overlap{};
            if (range_clip(new_tree_range, record_range, overlap))
                return false;
        }

        //计算新四叉树根节点
        Point2d new_root{ 0.5,0.5 };
        new_root.X = (new_tree_range.left + new_tree_range.right) / 2.0;
        new_root.Y = (new_tree_range.up + new_tree_range.down) / 2.0;

        //创建新四叉树
        tree_build(new_root, settings.min_tree_size);
        return false;
    }
}
