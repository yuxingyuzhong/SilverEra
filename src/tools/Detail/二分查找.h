#pragma once
//预编译头
#include "common/前置头文件包含.h"

//通用算法模块
namespace engine
{
    //辅助工具命名空间
	namespace detail
	{
        //单点二分查找——迭代器重载
        template<typename RandomIt, typename T, typename Compare, typename Projection = std::identity>
        std::optional<uint64_t> binary_search(RandomIt first, RandomIt last, const T& target,
            const Compare& comp, Projection proj = {})
        {
            //保存原始起始位置
            RandomIt original_first = first;
            while (first < last)
            {
                //获取中间迭代器
                RandomIt mid = first + (last - first) / 2;
                //若目标小于中间元素
                if (comp(target, std::invoke(proj, *mid)))
                    last = mid;
                //若中间元素小于目标
                else if (comp(std::invoke(proj, *mid), target))
                    first = mid + 1;
                //若中间元素等于目标
                else
                    return static_cast<uint64_t>(mid - original_first);
            }
            //若查找失败则返回空值
            return std::nullopt;
        }

        //单点二分查找——容器重载
        template<typename Container, typename T, typename Compare, typename Projection = std::identity>
        std::optional<uint64_t> binary_search(const Container& container, const T& target,
            const Compare& comp, Projection proj = {})
        {
            //调用迭代器重载版本
            return binary_search(std::begin(container), std::end(container),
                target, comp, proj);
        }

        //范围二分查找——迭代器重载
        template<typename RandomIt, typename T, typename Compare,
            typename Projection = std::identity>
        std::optional<std::pair<uint64_t, uint64_t>> range_binary_search(RandomIt first, RandomIt last,
            const T& target, const Compare& comp, Projection proj = {})
        {
            //计算下界：第一个使得 comp(proj(*it), target) 为 false 的元素
            //即第一个不小于 target 的元素
            auto lower = first;
            //计算区间元素数量
            auto count = std::distance(first, last);
            while (count > 0)
            {
                //计算二分步长
                auto step = count / 2;
                //获取当前迭代器
                auto it = lower;
                //移动迭代器
                std::advance(it, step);
                //若当前元素小于目标
                if (comp(std::invoke(proj, *it), target))
                {
                    lower = ++it;
                    count -= step + 1;
                }
                else
                    count = step;
            }

            //计算上界：第一个使得 comp(target, proj(*it)) 为 true 的元素
            //即第一个大于 target 的元素
            auto upper = lower;
            //计算剩余区间元素数量
            count = std::distance(upper, last);
            while (count > 0)
            {
                //计算二分步长
                auto step = count / 2;
                //获取当前迭代器
                auto it = upper;
                //移动迭代器
                std::advance(it, step);
                //若目标大于等于当前元素
                if (!comp(target, std::invoke(proj, *it)))
                {
                    upper = ++it;
                    count -= step + 1;
                }
                else
                    count = step;
            }

            //检查下界是否有效且与目标等价
            if (lower == last || comp(std::invoke(proj, *lower), target) || comp(target, std::invoke(proj, *lower)))
                return std::nullopt;

            //计算左边界下标
            uint64_t left_index  = static_cast<uint64_t>(std::distance(first, lower));
            //计算右边界下标
            uint64_t right_index = static_cast<uint64_t>(std::distance(first, upper)) - 1;
            //返回闭区间下标
            return { {left_index, right_index} };
        }

        //范围二分查找——容器重载
        template<typename Container, typename T, typename Compare, typename Projection = std::identity>
        std::optional<std::pair<uint64_t, uint64_t>> range_binary_search(const Container& container, const T& target,
            const Compare& comp, Projection proj = {})
        {
            //调用迭代器重载版本
            return range_binary_search(std::begin(container), std::end(container),
                target, comp, proj);
        }
	}
}

