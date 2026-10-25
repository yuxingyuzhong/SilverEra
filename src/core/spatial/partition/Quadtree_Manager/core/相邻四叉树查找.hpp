#pragma once
#include "../函数预声明.h"

//引擎命名空间
namespace engine
{
    //矩形筛选
    template<typename T>
    void Quadtree_Manager<T>::rect_filter(std::vector<Tree_Record<T>*>& receiver, const Tree_Record<T>* tree,
        const Rect2l& range, const std::vector<Tree_Record<T>*>* tree_group) const
    {
        //临时筛选结果接收
        std::vector<Tree_Record<T>*> candidate{};

        //若外界未提供四叉树集合则使用全局序列
        if (tree_group == nullptr)
            tree_group = &X_sequence;

        //检测X轴坐标
        for (Tree_Record<T>* now_tree : *tree_group)
        {
            //若候选四叉树根节点落入矩形且不是主体树
            if (now_tree->root.X >= range.left && now_tree->root.X <= range.right &&
                now_tree->root != tree->root)
                candidate.push_back(now_tree);
        }

        //若未筛选出候选四叉树则直接返回
        if (candidate.empty())
            return;

        //检测Y轴坐标
        for (Tree_Record<T>* now_tree : candidate)
        {
            //若候选四叉树根节点落入矩形且不是主体树
            if (now_tree->root.Y >= range.down && now_tree->root.Y <= range.up &&
                now_tree->root != tree->root)
                receiver.push_back(now_tree);
        }
    }

    //分类筛选
    template<typename T>
    std::vector<Tree_Record<T>*> Quadtree_Manager<T>::neighbor_classify(
        std::vector<Tree_Record<T>*>& receiver, const Tree_Record<T>* tree,
        const std::vector<Tree_Record<T>*>& candidate) const
    {
        //确认筛选候选者四叉树存储
        std::vector<Tree_Record<T>*> verify_candidate{};
        //筛选条件命中次数
        int filter_hits = 0;

        for (Tree_Record<T>* now_tree : candidate)
        {
            //重置筛选条件命中次数
            filter_hits = 0;
            //计算根节点坐标差值
            int64_t diff_x = static_cast<int64_t>(std::abs(tree->root.X - now_tree->root.X));
            int64_t diff_y = static_cast<int64_t>(std::abs(tree->root.Y - now_tree->root.Y));
            //两树边长之和的一半
            int64_t half_sum = static_cast<int64_t>((tree->size + now_tree->size) / 2);

            //若X轴坐标差值不大于两树边长之和的一半则命中
            if (diff_x <= half_sum)
                filter_hits++;
            //若Y轴坐标差值不大于两树边长之和的一半则命中
            if (diff_y <= half_sum)
                filter_hits++;
            //若命中两次则为对角相邻树
            if (filter_hits == 2)
                receiver.push_back(now_tree);
            //若命中一次则进入确认筛选
            else if (filter_hits == 1)
                verify_candidate.push_back(now_tree);
        }

        return verify_candidate;
    }

    //验证确认
    template<typename T>
    template<typename Screen>
    void Quadtree_Manager<T>::neighbor_verify(std::vector<Tree_Record<T>*>& receiver,
        const Tree_Record<T>* tree, const std::vector<Tree_Record<T>*>& candidate, Screen way) const
    {
        //中心四叉树边界
        Rect2l center_range = tree->tree->rect_range_calcu(tree->root, tree->size);

        //遍历候选四叉树
        for (Tree_Record<T>* ptr_tree : candidate)
        {
            //候选四叉树边界
            Rect2l candidate_range = ptr_tree->tree->rect_range_calcu(ptr_tree->root, ptr_tree->size);

            //X轴交集判定
            double left1 = static_cast<double>(center_range.left);
            double right1 = static_cast<double>(center_range.right);
            double left2 = static_cast<double>(candidate_range.left);
            double right2 = static_cast<double>(candidate_range.right);
            bool is_x_next = way(right1, left2, right2, left1);

            //Y轴交集判定
            double down1 = static_cast<double>(center_range.down);
            double up1 = static_cast<double>(center_range.up);
            double down2 = static_cast<double>(candidate_range.down);
            double up2 = static_cast<double>(candidate_range.up);
            bool is_y_next = way(up1, down2, up2, down1);

            //任一侧相交即为相邻树
            if (is_x_next || is_y_next)
                receiver.push_back(ptr_tree);
        }
    }

    //候选树筛选
    template<typename T>
    void Quadtree_Manager<T>::candidate_filter(std::vector<Tree_Record<T>*>& receiver,
        const Tree_Record<T>* tree, const std::vector<Tree_Record<T>*>& candidate) const
    {
        //分类筛选
        std::vector<Tree_Record<T>*> verify_candidate = neighbor_classify(receiver, tree, candidate);

        //重叠筛选方式
        auto screen_method = [](double coord_1, double coord_2, double coord_3, double coord_4) -> bool
            {
                return coord_1 > coord_2 - 1 && coord_3 > coord_4 - 1;
            };

        //确认筛选
        neighbor_verify(receiver, tree, verify_candidate, screen_method);
    }

    //相邻四叉树查找总函数
    template<typename T>
    void Quadtree_Manager<T>::neighbor_seek(std::vector<Tree_Record<T>*>& receiver,
        const Tree_Record<T>* tree, const std::vector<Tree_Record<T>*>* tree_group) const
    {
        //矩形筛选范围
        Rect2l tree_range{};
        //计算矩形筛选范围
        tree_range.left = static_cast<int64_t>(tree->root.X - (tree->size + largest_tree_size) / 2);
        tree_range.right = static_cast<int64_t>(tree->root.X + (tree->size + largest_tree_size) / 2);
        tree_range.up = static_cast<int64_t>(tree->root.Y + (tree->size + largest_tree_size) / 2);
        tree_range.down = static_cast<int64_t>(tree->root.Y - (tree->size + largest_tree_size) / 2);

        //矩形筛选
        std::vector<Tree_Record<T>*> rectan_trees{};
        rect_filter(rectan_trees, tree, tree_range, tree_group);
        //若未筛选出候选则直接返回
        if (rectan_trees.empty())
            return;

        //候选树筛选
        candidate_filter(receiver, tree, rectan_trees);
    }
}
