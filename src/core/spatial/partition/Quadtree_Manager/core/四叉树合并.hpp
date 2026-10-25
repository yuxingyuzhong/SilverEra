#pragma once
#include "../函数预声明.h"

//引擎命名空间
namespace engine
{
    //合并：收集候选组合
    template<typename T>
    void Quadtree_Manager<T>::merge_collect(std::vector<std::vector<Tree_Record<T>*>>& receiver) const
    {
        //简化表示路径
        uint64_t max_tree_size = settings.max_tree_size;
        uint64_t min_tree_size = settings.min_tree_size;

        //四叉树扩大级数存储
        int expand_level = 0;
        //四叉树当前最大可扩大大小存储
        uint64_t max_expandable_size = largest_tree_size;
        //若当前最大四叉树大小已达上限则取半
        if (max_expandable_size == max_tree_size)
            max_expandable_size /= 2;

        //计算最大可扩大四叉树扩大级数
        for (uint64_t temp = max_expandable_size; (temp /= 2) >= min_tree_size;)
            expand_level++;

        //不同扩大级数四叉树存储
        std::vector<std::vector<Tree_Record<T>*>> level_tree_group(expand_level + 1);

        //按扩大级数分组
        for (Tree_Record<T>* record : X_sequence)
        {
            //重置四叉树扩大级数
            expand_level = 0;
            //当前四叉树大小
            uint64_t tree_size = record->size;
            //若当前四叉树超过可扩大大小则略过
            if (tree_size > max_expandable_size)
                continue;
            //计算当前四叉树级数
            for (; (tree_size /= 2) >= min_tree_size;)
                expand_level++;
            //放入相应分组
            level_tree_group[expand_level].push_back(record);
        }

        //数量筛选通过的四叉树存储
        std::vector<std::vector<Tree_Record<T>*>> level_qualified_tree{};
        //筛选数量符合可合并最低条件的分组
        for (auto& group : level_tree_group)
            if (group.size() >= 4)
                level_qualified_tree.push_back(group);

        //相邻四叉树存储
        std::vector<Tree_Record<T>*> next_trees{};

        //筛选相邻四叉树数量符合要求的分组
        for (auto& candidate_trees : level_qualified_tree)
        {
            for (Tree_Record<T>* center_tree : candidate_trees)
            {
                //重置相邻四叉树集合
                next_trees.clear();
                //候选相邻树筛选
                neighbor_seek(next_trees, center_tree, &candidate_trees);

                //相邻树数量达到三棵即可组成四棵一组
                if (next_trees.size() >= 3)
                {
                    //添加主体四叉树
                    next_trees.push_back(center_tree);
                    //交换主体四叉树位置
                    std::swap(next_trees.front(), next_trees.back());
                    //记录筛选出的组合
                    receiver.push_back(next_trees);
                }
            }
        }
    }

    //合并：精确筛选组合
    template<typename T>
    void Quadtree_Manager<T>::merge_filter(
        const std::vector<std::vector<Tree_Record<T>*>>& candidate,
        std::vector<std::vector<Tree_Record<T>*>>& receiver) const
    {
        //新四叉树可能根节点坐标存储
        std::vector<Point2d> possible_root{};
        //主体四叉树坐标存储
        Point2d center_tree_root{ 0.5,0.5 };
        //主体四叉树大小存储
        uint64_t center_tree_size = 0;
        //根节点坐标偏移
        uint64_t offset = 0;

        //已使用四叉树集合
        std::unordered_set<Tree_Record<T>*> used_trees{};

        //遍历候选组合
        for (const auto& tree_group : candidate)
        {
            //若主体四叉树已被使用则跳过
            if (used_trees.count(tree_group.front()))
                continue;

            //重置主体四叉树坐标与大小
            center_tree_root = tree_group.front()->root;
            center_tree_size = tree_group.front()->size;
            offset = center_tree_size / 2;

            //计算新四叉树可能根节点坐标
            possible_root.clear();
            possible_root.push_back({ center_tree_root.X + offset, center_tree_root.Y + offset });
            possible_root.push_back({ center_tree_root.X + offset, center_tree_root.Y - offset });
            possible_root.push_back({ center_tree_root.X - offset, center_tree_root.Y + offset });
            possible_root.push_back({ center_tree_root.X - offset, center_tree_root.Y - offset });

            //四叉树索引集合
            int indices[3] = { 1, 2, 3 };
            //四叉树集合平均根节点坐标存储
            Point2d average_root{ 0.5,0.5 };
            //循环终止变量
            bool is_mergeable_group_found = false;

            //寻找可合并组合
            for (; indices[0] < static_cast<int>(tree_group.size()) - 2 && !is_mergeable_group_found;)
            {
                //计算四叉树集合平均根节点坐标
                average_root.X = (center_tree_root.X +
                    tree_group[indices[0]]->root.X +
                    tree_group[indices[1]]->root.X +
                    tree_group[indices[2]]->root.X) / 4.0;
                average_root.Y = (center_tree_root.Y +
                    tree_group[indices[0]]->root.Y +
                    tree_group[indices[1]]->root.Y +
                    tree_group[indices[2]]->root.Y) / 4.0;

                //筛选可合并组合
                for (const Point2d& root : possible_root)
                {
                    //若平均根节点坐标与可能根坐标相符
                    if (average_root == root)
                    {
                        //组合记录可行性标记
                        bool is_recordable = true;
                        //检查组合中是否有四叉树已被使用
                        for (int filter_time = 0; filter_time < 3; filter_time++)
                        {
                            if (used_trees.count(tree_group[indices[filter_time]]))
                            {
                                is_recordable = false;
                                break;
                            }
                        }

                        //若组合可记录则继续处理
                        if (is_recordable)
                        {
                            //四叉树拷贝缓冲区
                            std::vector<Tree_Record<T>*> buffer{};
                            buffer.push_back(tree_group.front());
                            buffer.push_back(tree_group[indices[0]]);
                            buffer.push_back(tree_group[indices[1]]);
                            buffer.push_back(tree_group[indices[2]]);
                            //记录可合并组合
                            receiver.push_back(buffer);
                            //记录已使用四叉树
                            for (Tree_Record<T>* record : buffer)
                                used_trees.insert(record);
                            //结束循环
                            is_mergeable_group_found = true;
                            break;
                        }
                    }
                }

                //索引自增
                indices[2]++;
                //校准进位
                if (indices[2] >= static_cast<int>(tree_group.size()))
                {
                    indices[1]++;
                    indices[2] = indices[1] + 1;
                    if (indices[1] >= static_cast<int>(tree_group.size()) - 1)
                    {
                        indices[0]++;
                        indices[1] = indices[0] + 1;
                        indices[2] = indices[1] + 1;
                    }
                }
            }
        }
    }

    //四叉树合并总函数
    template<typename T>
    void Quadtree_Manager<T>::tree_merge(void)
    {
        //高速缓存状态操作标志位
        bool is_cache_operate = false;
        //若高速缓存正在生效则临时关闭
        if (settings.is_cache_enabled)
        {
            settings.is_cache_enabled = false;
            is_cache_operate = true;
        }

        //分类筛选与确认筛选
        std::vector<std::vector<Tree_Record<T>*>> classified_tree{};
        std::vector<std::vector<Tree_Record<T>*>> verified_tree{};
        merge_collect(classified_tree);
        merge_filter(classified_tree, verified_tree);

        //合并四叉树
        for (const auto& tree_group : verified_tree)
        {
            //累加四棵旧树根节点坐标
            Point2d new_root{ 0.0,0.0 };
            for (Tree_Record<T>* record : tree_group)
            {
                new_root.X += record->root.X;
                new_root.Y += record->root.Y;
            }
            new_root.X /= 4.0;
            new_root.Y /= 4.0;

            //新四叉树边长
            uint64_t new_size = tree_group.front()->size * 2;
            //创建新四叉树
            if (!tree_build(new_root, new_size))
                continue;
            //获取新四叉树索引
            std::optional<uint64_t> new_tree_index = tree_index_seek(new_root);
            if (!new_tree_index.has_value())
                continue;
            Tree_Record<T>* new_tree = X_sequence[new_tree_index.value()];

            //待卸载四叉树索引集合
            std::vector<uint64_t> indexs{};
            //迁移区块数据
            for (Tree_Record<T>* record : tree_group)
            {
                //计算待合并四叉树管理范围
                Rect2l merged_range = record->tree->rect_range_calcu(record->root, record->size);
                //范围查询该范围内所有区块信息
                std::vector<Tree_Chunk_Data<T>> buffer{};
                record->tree->seek(merged_range, buffer);
                //迁移数据到新树
                for (Tree_Chunk_Data<T>& chunk : buffer)
                    new_tree->tree->build(point_to_l(chunk.center), chunk.data);
                //记录待卸载索引
                std::optional<uint64_t> index = tree_index_seek(record->root);
                if (index.has_value())
                    indexs.push_back(index.value());
            }

            //卸载已经被合并的四叉树
            unload_index(indexs);
        }

        //恢复高速缓存状态
        if (is_cache_operate)
            settings.is_cache_enabled = true;
    }
}
