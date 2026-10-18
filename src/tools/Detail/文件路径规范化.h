#pragma once
//预编译头
#include "Engine/EngineCore/common/前置头文件包含.h"
//获取路径字符串转换工具
#include "路径字符串转换.h"

//通用算法模块
namespace engine
{
    //辅助工具命名空间
    namespace detail
    {
        //路径规范化（统一目录分隔符并消除尾部分隔符，供路径比较与哈希使用）
        inline std::filesystem::path path_normalize(const std::filesystem::path& path)
        {
            //词法规范化（消解"."与".."）
            const std::filesystem::path normal = path.lexically_normal();
            //取出相对路径成分
            const std::filesystem::path relative = normal.relative_path();
            //以根路径为起点逐节重组（operator/= 统一使用平台首选分隔符，并自动消除尾部分隔符）
            std::filesystem::path result = normal.root_path();
            for (const auto& element : relative)
            {
                //跳过空节（尾部分隔符产生的空节）
                if (element.empty())
                    continue;
                //拼接当前节
                result /= element;
            }
            //返回规范化结果
            return result;
        }

        //路径键规范化（在路径规范化基础上剥离到锚点相对路径，统一 '/' 分隔符并末尾补 '/'，供路径前缀比较与哈希使用）
        //root_anchor：锚点目录（以 '/' 结尾）；为空则不做剥离，仅做基础规范化（统一分隔符、剥前导、补尾）
        inline std::string path_key_normalize(const std::string& raw, const std::string& root_anchor)
        {
            //复用通用路径规范化（统一分隔符并消解"."与".."）后转回 UTF-8 文本
            std::string path = path_to_string(path_normalize(string_to_path(raw)));
            //统一路径分隔符为正斜杠
            for (char& ch : path)
            {
                //反斜杠统一为正斜杠
                if (ch == '\\')
                    ch = '/';
            }
            //末尾先补 '/'，使「路径本身即锚点」时锚点（含尾 '/'）也能命中
            if (!path.empty() && path.back() != '/')
                path.push_back('/');
            //若锚点非空则在路径中定位其最后一次出现并剥离（锚点为空＝只做基础规范化）
            if (!root_anchor.empty())
            {
                //取锚点最后一次出现的位置
                const size_t anchor_pos = path.rfind(root_anchor);
                //若锚点存在则剥离锚点及其之前的部分
                if (anchor_pos != std::string::npos)
                    path.erase(0, anchor_pos + root_anchor.size());
            }
            //剥离开头的前导分隔符与 './' 片段
            for (;;)
            {
                //去掉前导 '/'
                if (!path.empty() && path.front() == '/')
                {
                    path.erase(0, 1);
                    continue;
                }
                //去掉前导 './'
                if (path.compare(0, 2, "./") == 0)
                {
                    path.erase(0, 2);
                    continue;
                }
                break;
            }
            //返回规范化路径键
            return path;
        }
    }
}
