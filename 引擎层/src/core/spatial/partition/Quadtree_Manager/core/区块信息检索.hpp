#pragma once
#include "../函数预声明.h"

//引擎命名空间
namespace engine
{
    //查询范围列表修改
    template<typename T>
    void Quadtree_Manager<T>::target_range_amend(const Rect2l& excel_range, bool* ptr_excel,
        const Rect2l& target_range)
    {
        //空指针防御
        if (ptr_excel == nullptr)
            return;
        //区块大小非正防御
        if (settings.block_size <= 0)
            return;

        //区块尺寸按 64 位整数参与计算
        //简化表示路径
        int64_t block_size = static_cast<int64_t>(settings.block_size);
        //行宽度（每行列数）
        int64_t width = (excel_range.right - excel_range.left + 1) / block_size;
        //总行数
        int64_t height = (excel_range.up - excel_range.down + 1) / block_size;

        //表尺寸非法防御
        if (width <= 0 || height <= 0)
            return;
        //完全无交集防御
        if (target_range.right < excel_range.left || target_range.left > excel_range.right ||
            target_range.down > excel_range.up || target_range.up < excel_range.down)
            return;

        //X轴起点列索引（包含）
        int64_t X_start = (target_range.left - excel_range.left) / block_size;
        //X轴终点列索引（包含），+1用于循环 < 结束
        int64_t X_end = (target_range.right - excel_range.left) / block_size + 1;
        //Y轴起始行索引（包含），从excel_range.up向下，行号递增
        int64_t Y_start = (excel_range.up - target_range.up) / block_size; // 注意：up在坐标中较大，向下减小
        //Y轴终点行索引（包含）
        int64_t Y_end = (excel_range.up - target_range.down) / block_size + 1;

        //裁剪到表内合法范围，越界部分丢弃
        if (X_start < 0)
            X_start = 0;
        if (Y_start < 0)
            Y_start = 0;
        if (X_end > width)
            X_end = width;
        if (Y_end > height)
            Y_end = height;

        //裁剪后区间为空则无需写入
        if (X_start >= X_end || Y_start >= Y_end)
            return;

        //开始修改元素
        for (int64_t row = Y_start; row < Y_end; row++)
        {
            //当前行首地址
            bool* p = ptr_excel + row * width;
            for (int64_t col = X_start; col < X_end; col++)
            {
                p[col] = true;
            }
        }
    }   
    
    //查询结果列表元素坐标化
    template<typename T>
    void Quadtree_Manager<T>::excel_element_to_coord(const Rect2l& excel_range, const int64_t& element_ID,
        Point2l& receiver)
    {
        //区块大小非正防御
        //区块尺寸为零时下方取模会抛整数除零异常
        if (settings.block_size == 0)
        {
            receiver = Point2l{};
            return;
        }

        //区块尺寸按 64 位整数参与计算
        int64_t block_size = static_cast<int64_t>(settings.block_size);
        //表格宽度（列数）
        int64_t width = (excel_range.right - excel_range.left + 1) / block_size;
        //表宽度非法防御
        //宽度不足一个区块时无法定位行列，退回原点
        if (width <= 0)
        {
            receiver = Point2l{};
            return;
        }

        //计算当前元素所在行列
        int64_t row = element_ID / width;
        int64_t col = element_ID % width;
        //将行列转换为坐标（区块左边界和上边界）
        receiver.X = excel_range.left + col * block_size;
        receiver.Y = excel_range.up - row * block_size;
    }

    //单区块信息查询
    template<typename T>
    void Quadtree_Manager<T>::seek(tree_chunk_data<T>*& receiver, const Point2i& target, bool stable)
    {
        //待查坐标提升为 64 位整数精度
        Point2l seek_coord{ target.X, target.Y };
        //查询四叉树记录
        tree_record<T>* tree_record = nullptr;

        for (;;)
        {
            //查找直属四叉树
            tree_record = quadtree_inclusion_seek(seek_coord);

            //若直属四叉树查找成功
            //则查找申请访问区块信息
            if (tree_record != nullptr)
                tree_record->tree->block_seek(receiver, seek_coord, stable);

            //若查找成功则结束
            if (receiver != nullptr || stable == false)
                break;
            //若查找失败则创建新树
            //重新查找
            else
                //调用四叉树智能创建
                qurdtree_build_smart({ target });
        }
    }

    //范围区块信息查询
    template<typename T>
    void Quadtree_Manager<T>::seek(std::vector<tree_chunk_data<T>*>& receiver,
        const Rect2i& target_range, bool stable)    {
        //获取四叉树序列
        auto& tree_group = X_sequence;
        //若四叉树序列为空
        if (tree_group.empty())
        {
            //若为非稳定模式则直接返回
            if (!stable)
                return;    
            else
            {
                //创建基准四叉树
                qurdtree_build_smart({ {target_range.left, target_range.down} });
                //若创建失败
                if (tree_group.empty())  
                    return;
            }
        }
        
        //获取基准树
        auto& baseline_tree = tree_group.front()->tree;
        //获取基准树根坐标
        auto& root = tree_group.front()->root;
        //获取区块单元大小
        auto& block_size = settings.block_size;

        //可查询范围存储（提升为 64 位整数精度）
        Rect2l seekable_range{ target_range.left, target_range.right, target_range.up, target_range.down };
        //可查询范围格式化
        baseline_tree->target_range_format(seekable_range, root, block_size);

        //区块尺寸按 64 位整数参与计算
        int64_t size_unit = static_cast<int64_t>(block_size);
        //区块尺寸非正防御
        if (size_unit <= 0)
            return;
        //待查询区块列数与行数
        int64_t want_col = (seekable_range.right - seekable_range.left + 1) / size_unit;
        int64_t want_row = (seekable_range.up - seekable_range.down + 1) / size_unit;
        //可查询范围非法防御
        if (want_col <= 0 || want_row <= 0)
            return;
        //待查询区块数计算（乘法前做溢出拦截）
        if (want_row > INT64_MAX / want_col)
            return;
        int64_t total_num = want_col * want_row;

        //分配足量内存存储查询结果列表
        bool* target_excel = new(std::nothrow) bool[static_cast<size_t>(total_num)]();
        //若分配失败则直接返回
        if (target_excel == nullptr)
            return;

        //查找坐标存储
        Point2l target{};
        //四叉树管理范围存储
        Rect2l tree_range{};
        //四叉树返回结果存储
        std::vector<tree_chunk_data<T>*> buffer{};

        //内层循环查找结果
        for (int64_t seek_time = 0; seek_time < total_num; seek_time++)
        {
            //若检测到未查找区块
            if (target_excel[seek_time] == false)
            {
                //索引格式化坐标
                excel_element_to_coord(seekable_range, seek_time, target);
                //查找直属四叉树
                tree_record<T>* ptr_tree = quadtree_inclusion_seek(target);
                //若未查询到直属四叉树且为稳定查询模式
                if (ptr_tree == nullptr && stable == true)
                {
                    //建档坐标需落回 32 位整数范围
                    //超出范围说明该坐标不宜作为管理层建档依据，跳过该区块
                    if (target.X < INT32_MIN || target.X > INT32_MAX ||
                        target.Y < INT32_MIN || target.Y > INT32_MAX)
                        continue;
                    //智能创建合适四叉树
                    qurdtree_build_smart({ Point2i{ static_cast<int>(target.X), static_cast<int>(target.Y) } });
                    //提取新创建四叉树
                    ptr_tree = quadtree_inclusion_seek(target);
                }

                //若四叉树创建失败或者为不稳定查询模式
                if (ptr_tree == nullptr)
                    continue;

                //查询待查询区块
                ptr_tree->tree->range_seek(buffer, seekable_range, stable);
                //记录查询结果
                receiver.insert(receiver.end(), buffer.begin(), buffer.end());
                //重置查询结果存储器
                buffer.clear();
                //获取四叉树管理范围
                ptr_tree->tree->manage_range_calcu(tree_range, ptr_tree->root, ptr_tree->size);
                //裁剪实际查找范围
                ptr_tree->tree->seekable_range_calcu(seekable_range, tree_range);
                //修改查询结果列表
                target_range_amend(tree_range, target_excel, seekable_range);
            }
        }

        //释放指针内存
        delete[] target_excel;
    }

}

