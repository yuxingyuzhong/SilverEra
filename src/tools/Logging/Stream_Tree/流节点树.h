#pragma once
//预编译头
#include "Engine/EngineCore/common/前置头文件包含.h"
//获取日志输出目标
#include "../Stream_Sink/输出目标.h"
//获取节点缓存
#include "../Node_Cache/节点缓存.h"
//获取路径操作工具（路径键规范化）
#include "Engine/EngineCore/src/tools/Detail/package/路径操作工具.h"

namespace engine
{
    //流节点链表树（按调用处路径的公共前缀建树，逐节点解析生效输出目标容器）
    class Stream_Tree
    {
    private:
        //线程安全输出目标别名（容器实际存储的输出目标）
        using Sink = std::shared_ptr<Stream_Sink>;
        //输出目标容器别名（容器可存储多个输出目标，打印日志时全部输出）
        using Sink_Group = std::shared_ptr<std::vector<Sink>>;
        //输出目标容器弱引用别名（共享父节点输出流时使用，不持有容器）
        using Sink_Group_Weak = std::weak_ptr<std::vector<Sink>>;

        //流节点（链表树节点）
        struct Stream_Node
        {
            //节点路径键（规范化路径，恒以 '/' 结尾，保证前缀比较落在路径段边界）
            std::string key{};
            //父节点（链表 splice 不搬移元素，故元素地址稳定、指针始终有效）
            Stream_Node* parent = nullptr;
            //子节点链表
            std::list<Stream_Node> children{};
            //自有输出流容器（节点被单独设置输出流时激活）
            Sink_Group own_group{};
            //共享输出流容器（节点未设置单独输出流时激活，弱引用最近祖先的自有容器）
            Sink_Group_Weak shared_group{};
            //共享容器的登记版本（与树版本号一致时该缓存才可信）
            uint64_t shared_epoch = 0;
            //节点互斥锁（保护本节点的容器、共享缓存与父指针）
            std::mutex node_mtx{};
        };

        //调用点节点缓存（调用处文件指针 → 节点指针）
        Node_Cache<Stream_Node> node_cache{};

        //内部文件输出目标的落盘阈值（字节；达此值才落盘，减少系统调用）
        static constexpr size_t file_flush_size = 4096;
    public:
        //输出流绑定（路径节点绑定文件输出流，目录节点绑定后其后代共享该流）
        bool stream_bind(const std::string& path, const std::string& file_name)
        {
            //整树结构锁（建节点）
            std::lock_guard<std::mutex> lock(tree_mtx);
            //定位或创建路径节点
            Stream_Node& node = node_ensure(path_normalize(path));
            //若文件名为空则改为绑定控制台输出流
            if (file_name.empty())
            {
                //提示并回落控制台输出
                if (Sink console = sink_console())
                    console->write("[FATAL]文件名为空\n已切换至控制台输出\n");
                return group_append(node, sink_console());
            }
            //分配文件输出流内存（禁用异常的分配形式）
            Log_Stream stream(new(std::nothrow) std::ofstream(file_name));
            //若内存分配失败
            if (!stream)
            {
                //提示并回落控制台输出
                if (Sink console = sink_console())
                    console->write("[FATAL]内存不足\n已切换至控制台输出\n");
                return false;
            }
            //构造文件流指针
            std::shared_ptr<std::ofstream> file_stream = std::static_pointer_cast<std::ofstream>(stream);
            //若文件打开失败
            if (!file_stream->is_open())
            {
                //提示并回落控制台输出
                if (Sink console = sink_console())
                    console->write("[ERROR]文件打开失败\n已切换至控制台输出\n");
                return false;
            }
            //以带缓冲的文件输出目标构造（禁用异常的分配形式）
            Sink sink(new(std::nothrow) Stream_Sink(stream, file_flush_size, true));
            //若输出目标分配失败
            if (!sink)
                return false;
            //把文件输出目标追加进节点自有容器
            return group_append(node, sink);
        }
        //输出流绑定（路径节点绑定外部输出流）
        bool stream_bind(const std::string& path, const Log_Stream& stream)
        {
            //若输出流为空则绑定失败
            if (!stream)
                return false;
            //整树结构锁（建节点）
            std::lock_guard<std::mutex> lock(tree_mtx);
            //以逐条落盘并刷新底层流的方式构造输出目标（保持外部流即时可读语义）
            Sink sink(new(std::nothrow) Stream_Sink(stream, 0, true));
            //若输出目标分配失败
            if (!sink)
                return false;
            //定位或创建路径节点并追加输出目标
            return group_append(node_ensure(path_normalize(path)), sink);
        }
        //输出流清空（清除路径节点自有输出流，使该节点及其后代回落共享父节点输出流）
        bool stream_clear(const std::string& path)
        {
            //整树结构锁
            std::lock_guard<std::mutex> lock(tree_mtx);
            //按规范化键查找节点（不新建）
            Stream_Node* node = node_find(path_normalize(path));
            //若节点不存在则清空失败
            if (!node)
                return false;
            //清除节点自有容器与共享缓存
            {
                //持节点锁改写容器
                std::lock_guard<std::mutex> node_lock(node->node_mtx);
                //清除自有容器（转为共享父节点输出流）
                node->own_group.reset();
                //清除共享缓存（下次输出按父链重新解析）
                node->shared_group.reset();
                //清除缓存登记版本
                node->shared_epoch = 0;
            }
            //流设置变更，递增树版本号使既有共享缓存全部失效
            epoch_bump();
            return true;
        }
        //输出流落盘（把整棵树内所有输出目标的缓冲内容写出并刷新）
        void stream_flush()
        {
            //先在结构锁内收集全部输出目标（不在锁内做 I/O）
            std::vector<Sink> sinks;
            {
                //整树结构锁
                std::lock_guard<std::mutex> lock(tree_mtx);
                //递归收集整棵树的自有输出目标
                group_collect(node_root, sinks);
                //并入控制台输出目标
                if (Sink console = sink_console())
                    sinks.emplace_back(console);
            }
            //逐个落盘（此时不持任何树锁，仅由各输出目标自锁）
            for (const Sink& sink : sinks)
            {
                //跳过空输出目标
                if (sink)
                    sink->flush();
            }
        }
        //日志行路由输出（按调用处路径定位节点 → 锁内解析容器 → 解锁后逐流输出）
        void stream_output(const char* file_name, const std::string& msg)
        {
            //定位调用处节点（缓存命中则不做任何路径规范化）
            Stream_Node* node = node_locate(file_name);
            //解析生效输出目标容器（锁内完成，取到容器后即已解锁）
            Sink_Group group = node ? group_resolve(*node) : group_console();
            //若容器不可用则直接写控制台
            if (!group || group->empty())
            {
                //取控制台输出目标
                if (Sink console = sink_console())
                    console->write(msg);
                return;
            }
            //逐个输出目标写出（此时不持任何树锁，仅由各输出目标自锁）
            for (const Sink& sink : *group)
            {
                //跳过空输出目标
                if (sink)
                    sink->write(msg);
            }
        }
    private:
        //调用路径规范化（复用通用路径规范化工具，剥离到项目根相对路径并补尾部分隔符）
        std::string path_normalize(const std::string& raw) const
        {
            //项目根目录名锚点（以其为界把绝对调用处路径剥离为项目内相对路径）
            static const std::string anchor = "白银纪元/";
            //交给通用路径操作工具完成词法规范化与路径键编排
            return detail::path_key_normalize(raw, anchor);
        }
        //路径段对齐的公共前缀长度（公共部分不足一个路径段时回退到最近的 '/' 边界）
        size_t prefix_common(const std::string& left, const std::string& right)
        {
            //求原始公共前缀长度
            const size_t limit = left.size() < right.size() ? left.size() : right.size();
            size_t length = 0;
            while (length < limit && left[length] == right[length])
                ++length;
            //回退到最近的 '/' 边界，保证公共部分以路径段为单位
            while (length > 0 && left[length - 1] != '/')
                --length;
            //返回对齐后的公共前缀长度
            return length;
        }
        //节点查找或创建（按路径键自根向下定位，公共前缀即父节点）
        Stream_Node& node_ensure(const std::string& key)
        {
            //空键即根节点
            if (key.empty())
                return node_root;
            //自根节点开始定位
            Stream_Node* node = &node_root;
            //待定位的剩余键
            std::string rest = key;
            //逐层向下定位
            for (;;)
            {
                //在当前子链表中寻找与剩余键存在公共前缀的子节点
                std::list<Stream_Node>::iterator match = node->children.end();
                for (std::list<Stream_Node>::iterator it = node->children.begin(); it != node->children.end(); ++it)
                {
                    //若存在公共前缀则命中该子节点
                    if (prefix_common(it->key, rest) > 0)
                    {
                        match = it;
                        break;
                    }
                }
                //若不存在公共前缀的子节点则以剩余键新建
                if (match == node->children.end())
                {
                    //在子链表末尾就地新建节点（list 追加不搬移既有元素，地址稳定）
                    node->children.emplace_back();
                    //取新建节点
                    Stream_Node& created = node->children.back();
                    //设置路径键与父指针
                    created.key = rest;
                    created.parent = node;
                    return created;
                }
                //取子节点键与剩余键的公共前缀长度
                const size_t length = prefix_common(match->key, rest);
                //若子节点键完整包含于剩余键则继续向下定位
                if (length == match->key.size())
                {
                    //去掉已匹配的公共前缀
                    rest.erase(0, length);
                    //若剩余键已清空则该子节点即目标节点
                    if (rest.empty())
                        return *match;
                    //下降到该子节点继续定位
                    node = &(*match);
                    continue;
                }
                //公共前缀短于子节点键：拆出中间节点承载公共前缀
                node->children.emplace_back();
                //取中间节点
                Stream_Node& middle = node->children.back();
                //设置中间节点键（公共前缀）与父指针
                middle.key = match->key.substr(0, length);
                middle.parent = node;
                //把原子节点移入中间节点子链表（splice 不搬移元素，子节点地址与指针保持有效）
                middle.children.splice(middle.children.end(), node->children, match);
                //取被搬移的原子节点
                Stream_Node& moved = middle.children.back();
                {
                    //持被搬移节点锁改写键与父指针（父指针会被输出路径并发读取）
                    std::lock_guard<std::mutex> moved_lock(moved.node_mtx);
                    //截短原子节点键（去掉公共前缀部分）并重设父指针
                    moved.key.erase(0, length);
                    moved.parent = &middle;
                }
                //去掉已匹配的公共前缀
                rest.erase(0, length);
                //若剩余键已清空则中间节点即目标节点
                if (rest.empty())
                    return middle;
                //否则以剩余键为中间节点新建子节点
                middle.children.emplace_back();
                //取新建节点
                Stream_Node& created = middle.children.back();
                //设置路径键与父指针
                created.key = rest;
                created.parent = &middle;
                return created;
            }
        }
        //节点查找（按路径键自根向下定位，不新建节点）
        Stream_Node* node_find(const std::string& key)
        {
            //空键即根节点
            if (key.empty())
                return &node_root;
            //自根节点开始定位
            Stream_Node* node = &node_root;
            //待定位的剩余键
            std::string rest = key;
            //逐层向下定位
            for (;;)
            {
                //在当前子链表中寻找匹配子节点
                Stream_Node* match = nullptr;
                for (Stream_Node& child : node->children)
                {
                    //若子节点键为剩余键的前缀则命中
                    if (prefix_common(child.key, rest) == child.key.size())
                    {
                        match = &child;
                        break;
                    }
                }
                //若未命中则节点不存在
                if (!match)
                    return nullptr;
                //去掉已匹配的公共前缀
                rest.erase(0, match->key.size());
                //若剩余键已清空则命中该节点
                if (rest.empty())
                    return match;
                //下降到该子节点继续定位
                node = match;
            }
        }
        //调用点节点定位（命中调用点缓存则直接返回，未命中才规范化路径、建节点并登记）
        Stream_Node* node_locate(const char* file_name)
        {
            //先查调用点缓存
            Stream_Node* cached = node_cache.cache_fetch(file_name);
            //命中则直接返回（节点地址稳定，缓存长期有效）
            if (cached)
                return cached;
            //未命中：持结构锁规范化路径并定位或创建节点（规范化仅此一次）
            std::lock_guard<std::mutex> lock(tree_mtx);
            //规范化调用处路径键
            const std::string key = path_normalize(file_name);
            //定位或创建节点
            Stream_Node* node = &node_ensure(key);
            //结构可能新增节点，递增树版本号使既有共享缓存失效
            epoch_bump();
            //把调用点登记进缓存，后续输出直接命中
            node_cache.cache_store(file_name, node);
            return node;
        }
        //当前树版本号
        uint64_t epoch_current() const
        {
            return tree_epoch.load();
        }
        //递增树版本号（使既有共享缓存全部失效）
        void epoch_bump()
        {
            tree_epoch.fetch_add(1);
        }
        //控制台输出目标构建（以非拥有指针包装 std::cout，逐条落盘且不强制刷新底层流）
        Sink sink_console_create()
        {
            //以不持有 std::cout 生命周期的共享指针包装标准输出
            Log_Stream console(&std::cout, [](std::ostream*) {});
            //以「每次写出即落盘、不刷新底层流」的方式构造控制台输出目标
            return Sink(new(std::nothrow) Stream_Sink(console, 0, false));
        }
        //控制台输出目标获取（仅构造一次）
        Sink sink_console()
        {
            //控制台目标仅需构造一次，以函数局部静态持有
            static Sink sink = sink_console_create();
            return sink;
        }
        //控制台兜底输出目标容器构建
        Sink_Group group_create_console()
        {
            //以禁用异常的形式分配容器
            Sink_Group group(new(std::nothrow) std::vector<Sink>());
            //若容器分配失败则返回空容器
            if (!group)
                return group;
            //装入控制台输出目标
            if (Sink console = sink_console())
                group->push_back(console);
            return group;
        }
        //控制台兜底输出目标容器获取（未设置任何文件流时的默认输出目标）
        Sink_Group group_console()
        {
            //容器仅需构造一次，以函数局部静态持有
            static Sink_Group group = group_create_console();
            return group;
        }
        //节点自有输出目标追加（必要时创建容器并递增树版本号）
        bool group_append(Stream_Node& node, const Sink& sink)
        {
            //记录本次是否新建了自有容器
            bool created = false;
            {
                //持节点锁改写容器
                std::lock_guard<std::mutex> lock(node.node_mtx);
                //若节点自有容器尚未创建则创建
                if (!node.own_group)
                {
                    //以禁用异常的形式分配容器
                    node.own_group = Sink_Group(new(std::nothrow) std::vector<Sink>());
                    //若容器分配失败则追加失败
                    if (!node.own_group)
                        return false;
                    //标记本次新建了自有容器
                    created = true;
                }
                //把输出目标追加进节点自有容器
                node.own_group->push_back(sink);
            }
            //若本次由共享父流转为单独流输出则递增树版本号，使既有共享缓存失效
            if (created)
                epoch_bump();
            return true;
        }
        //节点生效输出目标容器解析（自有优先，其次未过期的共享缓存，最后沿父链向上）
        Sink_Group group_resolve(Stream_Node& node)
        {
            //记录解析开始时的树版本号（用于判定登记结果是否过期）
            const uint64_t epoch_begin = epoch_current();
            //起点快速路径：自有容器优先，其次未过期的共享缓存
            Stream_Node* parent = nullptr;
            {
                //持起点节点锁读取容器与缓存
                std::lock_guard<std::mutex> lock(node.node_mtx);
                //节点已设置自有输出流则单独流输出
                if (node.own_group)
                    return node.own_group;
                //共享缓存未过期则直接复用父节点输出流
                if (node.shared_epoch == epoch_begin)
                {
                    //尝试锁定弱引用缓存
                    if (Sink_Group shared = node.shared_group.lock())
                        return shared;
                }
                //记录父节点以便继续向上解析
                parent = node.parent;
            }
            //沿父链向上解析（每级仅持一个节点锁，即取即放、不嵌套）
            Sink_Group found;
            Stream_Node* current = parent;
            while (current && !found)
            {
                //记录本级的父节点以便继续向上
                Stream_Node* next = nullptr;
                {
                    //持当前节点锁读取容器与缓存
                    std::lock_guard<std::mutex> lock(current->node_mtx);
                    //节点已设置自有输出流则命中
                    if (current->own_group)
                    {
                        found = current->own_group;
                    }
                    else
                    {
                        //共享缓存未过期时尝试复用
                        if (current->shared_epoch == epoch_begin)
                            found = current->shared_group.lock();
                        //仍未命中则记录父节点继续向上
                        if (!found)
                            next = current->parent;
                    }
                }
                //继续向父节点解析
                current = next;
            }
            //全部祖先均未设置输出流则回落控制台容器
            if (!found)
                found = group_console();
            //把解析结果登记到起点节点的共享缓存（以开始版本号登记，期间结构变更即自动判过期）
            {
                //持起点节点锁登记缓存
                std::lock_guard<std::mutex> lock(node.node_mtx);
                //登记共享容器弱引用
                node.shared_group = found;
                //登记缓存版本号
                node.shared_epoch = epoch_begin;
            }
            return found;
        }
        //自有输出目标收集（递归整棵树，供落盘使用）
        void group_collect(Stream_Node& node, std::vector<Sink>& sinks)
        {
            //持节点锁读取自有容器
            {
                //持节点锁
                std::lock_guard<std::mutex> lock(node.node_mtx);
                //把本节点自有容器内的输出目标并入结果
                if (node.own_group)
                {
                    for (const Sink& sink : *node.own_group)
                        sinks.emplace_back(sink);
                }
            }
            //递归处理子节点
            for (Stream_Node& child : node.children)
                group_collect(child, sinks);
        }

        //树版本号（节点结构或流设置变更时递增，用于判定共享缓存是否过期）
        std::atomic<uint64_t> tree_epoch{ 0 };
        //流节点树根（路径键为空串）
        Stream_Node node_root{};
        //整树结构互斥锁（建节点 / 设流 / 清流 / 落盘收集共用，不用于日志输出热路径）
        std::mutex tree_mtx{};
    };
}