#pragma once
#include "../函数预声明.h"

//引擎命名空间
namespace engine
{
    //直属四叉树查找
    template<typename T>
    Tree_Record<T>* Quadtree_Manager<T>::tree_inclusion_seek(const Point2l& target)
    {
        //临时树边界存储
        Rect2l tree_range{};

        //简化表示路径
        auto& is_cache_enabled = settings.is_cache_enabled;
        auto& records = tree_cache.records;
        auto& ranges = tree_cache.ranges;

        //若高速缓存启用且存在高速缓存
        if (is_cache_enabled && records.size() > 0)
        {
            for (int seek_time = 0; seek_time < records.size(); seek_time++)
            {
                //当前候选树
                Tree_Record<T>* now_tree = records[seek_time];
                //读取当前候选四叉树边界
                tree_range = ranges[seek_time];

                //若坐标完全位于候选四叉树管辖范围则为直属四叉树
                if (target.X >= tree_range.left && target.X <= tree_range.right &&
                    target.Y >= tree_range.down && target.Y <= tree_range.up)
                {
                    //若当前条目非第一条目
                    if (seek_time > 0)
                    {
                        //将当前条目高速缓存向前进位
                        std::swap(records[seek_time], records[seek_time - 1]);
                        std::swap(ranges[seek_time], ranges[seek_time - 1]);
                        //递减索引保证返回条目正确
                        seek_time--;
                    }
                    //返回高速缓存条目
                    return records[seek_time];
                }
            }
        }

        //若不存在高速缓存或高速缓存查找失败
        //则扫描四叉树序列

        //简化表示路径
        auto& tree_group = X_sequence;
        auto& cache_active_threshold = settings.cache_active_threshold;
        auto& max_cache_records = settings.max_cache_records;

        //匹配直属四叉树
        for (int seek_time = 0; seek_time < tree_group.size(); seek_time++)
        {
            //当前候选树
            Tree_Record<T>* ptr_tree = tree_group[seek_time];
            //计算当前候选四叉树边界
            tree_range = ptr_tree->tree->rect_range_calcu(ptr_tree->root, ptr_tree->size);

            //若坐标完全位于候选四叉树管辖范围则为直属四叉树
            if (target.X >= tree_range.left && target.X <= tree_range.right &&
                target.Y >= tree_range.down && target.Y <= tree_range.up)
            {
                //若高速缓存已启用且四叉树数量到达阈值
                if (is_cache_enabled && tree_group.size() >= cache_active_threshold)
                {
                    //若缓存条目上限有效
                    if (max_cache_records > 0)
                    {
                        //若缓存条目已达上限则移除最旧条目
                        if (records.size() >= max_cache_records)
                        {
                            records.pop_back();
                            ranges.pop_back();
                        }
                        //记录缓存条目
                        records.push_back(ptr_tree);
                        ranges.push_back(tree_range);
                    }
                }
                //返回查找结果
                return ptr_tree;
            }
        }

        //若查找失败则返回空指针
        return nullptr;
    }

    //四叉树序列索引查找
    template<typename T>
    std::optional<uint64_t> Quadtree_Manager<T>::tree_index_seek(const Point2d& root) const
    {
        //简化表示路径
        auto& tree_group = X_sequence;
        //获取根节点X轴坐标与目标坐标相同的索引范围
        std::optional<std::pair<uint64_t, uint64_t>> range = detail::range_binary_search
        (tree_group.begin(), tree_group.end(), root.X,
            std::ranges::greater(), [](auto* p) { return p->root.X; });
        //若返回区间无效
        if (!range.has_value())
            return std::nullopt;

        //检测是否存在符合要求的四叉树
        for (uint64_t begin = range.value().first, end = range.value().second; begin <= end; begin++)
        {
            //若根节点坐标相同则返回当前索引
            if (root == tree_group[begin]->root)
                return begin;
        }

        //若查找失败则返回无效索引
        return std::nullopt;
    }

    //最近四叉树查找
    template<typename T>
    Tree_Record<T>* Quadtree_Manager<T>::nearest_tree(const Point2l& target) const
    {
        //最近四叉树存储
        Tree_Record<T>* nearest = nullptr;
        //最近距离平方存储
        double nearest_distance = 0;

        //扫描四叉树序列
        for (Tree_Record<T>* record : X_sequence)
        {
            //计算根节点到目标点的距离平方
            double diff_x = static_cast<double>(target.X) - record->root.X;
            double diff_y = static_cast<double>(target.Y) - record->root.Y;
            double distance = diff_x * diff_x + diff_y * diff_y;

            //若为更近的四叉树则记录
            if (nearest == nullptr || distance < nearest_distance)
            {
                nearest = record;
                nearest_distance = distance;
            }
        }

        return nearest;
    }

    //确保目标坐标被某棵四叉树覆盖
    template<typename T>
    Tree_Record<T>* Quadtree_Manager<T>::ensure_tree(const Point2l& target)
    {
        //先尝试直接查找
        Tree_Record<T>* record = tree_inclusion_seek(target);
        if (record != nullptr)
            return record;

        //若序列为空则执行首轮智能创建
        if (X_sequence.empty())
            tree_build_smart_first({ target });
        //若序列非空则反复申请扩大或新建
        else
        {
            for (int attempt = 0; attempt < 64; attempt++)
            {
                //获取最近四叉树
                Tree_Record<T>* nearest = nearest_tree(target);
                if (nearest == nullptr)
                    break;

                //记录操作前状态
                size_t before_size = X_sequence.size();
                uint64_t before_largest = largest_tree_size;

                //申请扩大或新建
                tree_expand_approve(nearest->root, target, true);

                //重新查找
                record = tree_inclusion_seek(target);
                if (record != nullptr)
                    return record;

                //若无任何进展则结束
                if (X_sequence.size() == before_size && largest_tree_size == before_largest)
                    break;
            }
        }

        return tree_inclusion_seek(target);
    }

    //四叉树创建
    template<typename T>
    bool Quadtree_Manager<T>::tree_build(Point2d root, uint64_t tree_size)
    {
        //树边长须在上下限之间
        if (tree_size < settings.min_tree_size || tree_size > settings.max_tree_size)
            return false;
        //不允许重复根坐标
        if (tree_index_seek(root).has_value())
            return false;

        //分配新四叉树节点内存
        Tree_Record<T>* new_tree = new(std::nothrow) Tree_Record<T>;
        //若内存分配失败则直接返回
        if (new_tree == nullptr)
            return false;
        //分配新四叉树内存
        new_tree->tree = new(std::nothrow) Quadtree<T>(tree_size, root);
        //若内存分配失败则释放档案
        if (new_tree->tree == nullptr)
        {
            delete new_tree;
            return false;
        }

        //记录新四叉树大小
        new_tree->size = tree_size;
        //记录新四叉树根节点坐标
        new_tree->root = root;

        //设置新四叉树大小上限与最小区块
        if (!new_tree->tree->set_max_size(settings.max_tree_size) ||
            !new_tree->tree->set_block_size(settings.block_size))
        {
            delete new_tree->tree;
            delete new_tree;
            return false;
        }

        //设置新四叉树回调管理函数
        auto manage = [this](const Point2d& root, const Point2l& target)->bool
            { return this->tree_expand_approve(root, target); };
        new_tree->tree->set_callback_manage(manage);

        //将新四叉树放入四叉树序列
        X_sequence.push_back(new_tree);

        //维护当前最大四叉树边长
        if (tree_size > largest_tree_size)
            largest_tree_size = tree_size;

        //按根节点X坐标降序排序
        std::ranges::sort(X_sequence.begin(), X_sequence.end(),
            std::ranges::greater(), [](const Tree_Record<T>* node) { return node->root.X; });

        return true;
    }

    //卸载——序列索引重载
    template<typename T>
    void Quadtree_Manager<T>::unload_index(std::vector<uint64_t>& index_set)
    {
        //简化表示路径
        auto& records = tree_cache.records;
        auto& ranges = tree_cache.ranges;

        //降序排列防止索引失效
        std::ranges::sort(index_set, std::ranges::greater());

        //上一个已卸载索引
        uint64_t last_index = (std::numeric_limits<uint64_t>::max)();

        //卸载四叉树
        for (uint64_t index : index_set)
        {
            //越界索引略过
            if (index >= X_sequence.size())
                continue;
            //重复索引略过
            if (index == last_index)
                continue;
            last_index = index;

            //获取四叉树记录
            Tree_Record<T>* record = X_sequence[index];

            //若存在高速缓存则检查卸载对象是否位于高速缓存
            for (int seek_time = 0; seek_time < records.size(); seek_time++)
            {
                //若指针地址相同则成功匹配
                if (records[seek_time] == record)
                {
                    //卸载高速缓存
                    records.erase(records.begin() + seek_time);
                    ranges.erase(ranges.begin() + seek_time);
                    break;
                }
            }

            //释放四叉树与档案
            delete record->tree;
            delete record;
            //从四叉树序列中移除
            X_sequence.erase(X_sequence.begin() + static_cast<std::ptrdiff_t>(index));
        }

        //按剩余四叉树重算最大边长
        largest_tree_size = 0;
        for (Tree_Record<T>* record : X_sequence)
            if (record->size > largest_tree_size)
                largest_tree_size = record->size;
    }

    //卸载——根节点重载
    template<typename T>
    void Quadtree_Manager<T>::tree_unload(const std::vector<Point2d>& root_set)
    {
        //四叉树索引集合
        std::vector<uint64_t> index_set{};
        //逐个查找目标根坐标对应索引
        for (const Point2d& root : root_set)
        {
            std::optional<uint64_t> index = tree_index_seek(root);
            if (index.has_value())
                index_set.push_back(index.value());
        }
        //统一卸载
        unload_index(index_set);
    }
}
