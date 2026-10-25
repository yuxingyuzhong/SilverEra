#pragma once
#include "../函数预声明.h"

//引擎命名空间
namespace engine
{
    //范围裁剪
    template<typename T>
    bool Quadtree_Manager<T>::range_clip(const Rect2l& lhs, const Rect2l& rhs, Rect2l& receiver) const
    {
        //四边界取交集
        receiver.left = (std::max)(lhs.left, rhs.left);
        receiver.right = (std::min)(lhs.right, rhs.right);
        receiver.down = (std::max)(lhs.down, rhs.down);
        receiver.up = (std::min)(lhs.up, rhs.up);

        //返回交集是否有效
        return receiver.left <= receiver.right && receiver.down <= receiver.up;
    }

    //确保范围内每个区块都有四叉树覆盖
    template<typename T>
    void Quadtree_Manager<T>::coverage_ensure(const Rect2l& target_range)
    {
        //若序列为空则先建立基准树
        if (X_sequence.empty())
        {
            tree_build_smart_first({ { target_range.left, target_range.down } });
            if (X_sequence.empty())
                return;
        }

        //以基准树网格对齐查询范围
        Rect2l aligned = target_range;
        Tree_Record<T>* base = X_sequence.front();
        base->tree->range_align(aligned, base->root, settings.block_size);

        //区块尺寸非正防御
        int64_t block = static_cast<int64_t>(settings.block_size);
        if (block <= 0)
            return;

        //网格列数与行数
        int64_t width = (aligned.right - aligned.left + 1) / block;
        int64_t height = (aligned.up - aligned.down + 1) / block;
        if (width <= 0 || height <= 0)
            return;

        //逐格检查四个角点是否被覆盖
        for (int64_t row = 0; row < height; row++)
        {
            for (int64_t col = 0; col < width; col++)
            {
                int64_t left = aligned.left + col * block;
                int64_t right = left + block - 1;
                int64_t up = aligned.up - row * block;
                int64_t down = up - block + 1;
                ensure_tree({ left, down });
                ensure_tree({ right, down });
                ensure_tree({ left, up });
                ensure_tree({ right, up });
            }
        }
    }

    //区块查询/操作分派
    template<typename T>
    void Quadtree_Manager<T>::chunk_dispatch(const Rect2l& target_range, const Chunk_Mode& mode,
        std::vector<Tree_Chunk_Data<T>>& receiver)
    {
        //只读模式不建树也不扩树
        bool read_only = (mode == Chunk_Mode::Seek || mode == Chunk_Mode::Unload);

        //若序列为空
        if (X_sequence.empty())
        {
            //只读模式直接返回
            if (read_only)
                return;
            //非只读模式先建立基准树
            tree_build_smart_first({ { target_range.left, target_range.down } });
            if (X_sequence.empty())
                return;
        }

        //构建/获取模式先补齐范围内覆盖
        if (mode == Chunk_Mode::Build || mode == Chunk_Mode::Get)
            coverage_ensure(target_range);

        //以基准树网格对齐查询范围
        Rect2l aligned = target_range;
        Tree_Record<T>* base = X_sequence.front();
        base->tree->range_align(aligned, base->root, settings.block_size);

        //快照四叉树序列，避免操作期间序列变化影响遍历
        std::vector<Tree_Record<T>*> snapshot = X_sequence;

        //逐树求交并执行区块操作
        for (Tree_Record<T>* record : snapshot)
        {
            //计算四叉树管理范围
            Rect2l tree_range = record->tree->rect_range_calcu(record->root, record->size);
            //裁剪出本次操作范围
            Rect2l region{};
            if (!range_clip(aligned, tree_range, region))
                continue;

            //四叉树返回结果缓冲
            std::vector<Tree_Chunk_Data<T>> buffer{};
            //按模式执行区块操作
            switch (mode)
            {
            case Chunk_Mode::Build:
                record->tree->build(region);
                break;
            case Chunk_Mode::Seek:
                record->tree->seek(region, buffer);
                break;
            case Chunk_Mode::Get:
                record->tree->get(region, buffer);
                break;
            case Chunk_Mode::Unload:
                record->tree->unload(region);
                break;
            }

            //查找/获取模式收集结果
            if (mode == Chunk_Mode::Seek || mode == Chunk_Mode::Get)
            {
                receiver.insert(receiver.end(),
                    std::make_move_iterator(buffer.begin()),
                    std::make_move_iterator(buffer.end()));
            }
        }
    }

    //区块构建 —— 单区块重载
    template<typename T>
    void Quadtree_Manager<T>::build(const Point2l& target, const std::shared_ptr<T>& data)
    {
        //确保目标坐标被四叉树覆盖
        Tree_Record<T>* record = ensure_tree(target);
        if (record == nullptr)
            return;
        //在归属树中构建区块
        record->tree->build(target, data);
    }

    //区块构建 —— 范围重载
    template<typename T>
    void Quadtree_Manager<T>::build(const Rect2l& target_range)
    {
        //参数填充临时对象
        std::vector<Tree_Chunk_Data<T>> temp{};
        //启动区块构建模式
        chunk_dispatch(target_range, Chunk_Mode::Build, temp);
    }

    //区块查找 —— 单区块重载
    template<typename T>
    bool Quadtree_Manager<T>::seek(const Point2l& target, Tree_Chunk_Data<T>& receiver)
    {
        //查找直属四叉树
        Tree_Record<T>* record = tree_inclusion_seek(target);
        if (record == nullptr)
            return false;
        //在归属树中查找区块
        return record->tree->seek(target, receiver);
    }

    //区块查找 —— 范围重载
    template<typename T>
    void Quadtree_Manager<T>::seek(const Rect2l& target_range,
        std::vector<Tree_Chunk_Data<T>>& receiver)
    {
        //启动区块查找模式
        chunk_dispatch(target_range, Chunk_Mode::Seek, receiver);
    }

    //区块获取 —— 单区块重载
    template<typename T>
    bool Quadtree_Manager<T>::get(const Point2l& target, Tree_Chunk_Data<T>& receiver)
    {
        //确保目标坐标被四叉树覆盖
        Tree_Record<T>* record = ensure_tree(target);
        if (record == nullptr)
            return false;
        //在归属树中获取区块
        return record->tree->get(target, receiver);
    }

    //区块获取 —— 范围重载
    template<typename T>
    void Quadtree_Manager<T>::get(const Rect2l& target_range,
        std::vector<Tree_Chunk_Data<T>>& receiver)
    {
        //启动区块获取模式
        chunk_dispatch(target_range, Chunk_Mode::Get, receiver);
    }

    //区块卸载 —— 单区块重载
    template<typename T>
    void Quadtree_Manager<T>::unload(const Point2l& target)
    {
        //查找直属四叉树
        Tree_Record<T>* record = tree_inclusion_seek(target);
        if (record == nullptr)
            return;
        //在归属树中卸载区块
        record->tree->unload(target);
    }

    //区块卸载 —— 范围重载
    template<typename T>
    void Quadtree_Manager<T>::unload(const Rect2l& target_range)
    {
        //参数填充临时对象
        std::vector<Tree_Chunk_Data<T>> temp{};
        //启动区块卸载模式
        chunk_dispatch(target_range, Chunk_Mode::Unload, temp);
    }
}
