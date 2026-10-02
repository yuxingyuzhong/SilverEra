#pragma once
//预编译头
#include "common/前置头文件包含.h"

namespace engine
{
    //日志系统
    class Log
    {
    public:
        //输出流别名
        using Stream = std::shared_ptr<std::ostream>;
        //日志格式串（携带调用处信息，并承担占位符与实参的编译期校验）
        template<typename... Args>
        class Format
        {
        public:
            //从字面量构造（默认参数在调用点求值，从而自动捕获调用处文件路径）
            template<typename Text>
            consteval Format(const Text& text, std::source_location place = std::source_location::current())
                : pattern(text), site(place)
            {
            }

            //格式串本体（编译期校验占位符与实参匹配）
            std::format_string<Args...> pattern;
            //调用处信息
            std::source_location site;
        };
    public:
        //信息输出
        template<typename... Args>
        static void info(Format<std::type_identity_t<Args>...> fmt, Args&&... args)
        {
            //格式化正文并交由链表树路由输出（方法不感知任何流设置）
            output(fmt.site.file_name(), std::format(fmt.pattern, std::forward<Args>(args)...), "INFO");
        }
        //警告输出
        template<typename... Args>
        static void warn(Format<std::type_identity_t<Args>...> fmt, Args&&... args)
        {
            //格式化正文并交由链表树路由输出
            output(fmt.site.file_name(), std::format(fmt.pattern, std::forward<Args>(args)...), "WARN");
        }
        //错误输出
        template<typename... Args>
        static void error(Format<std::type_identity_t<Args>...> fmt, Args&&... args)
        {
            //格式化正文并交由链表树路由输出
            output(fmt.site.file_name(), std::format(fmt.pattern, std::forward<Args>(args)...), "ERROR");
        }
        //调试输出
        template<typename... Args>
        static void debug(Format<std::type_identity_t<Args>...> fmt, Args&&... args)
        {
            //格式化正文并交由链表树路由输出
            output(fmt.site.file_name(), std::format(fmt.pattern, std::forward<Args>(args)...), "DEBUG");
        }
        //输出流绑定（路径节点绑定文件输出流，目录节点绑定后其后代共享该流）
        static bool stream_bind(const std::string& path, const std::string& file_name)
        {
            //整树加锁（多线程保护）
            std::lock_guard<std::mutex> lock(tree_mtx);
            //定位或创建路径节点
            Stream_Node& node = node_ensure(path_normalize(path));
            //若文件名为空则改为绑定控制台输出流
            if (file_name.empty())
                return group_append(node, stream_console());
            //分配文件输出流内存（禁用异常的分配形式）
            Stream stream(new(std::nothrow) std::ofstream(file_name));
            //若内存分配失败
            if (!stream)
            {
                //提示并回落控制台输出
                *stream_console() << "[FATAL]" << "内存不足\n已切换至控制台输出\n" << std::endl;
                return false;
            }
            //构造文件流指针
            std::shared_ptr<std::ofstream> file_stream = std::static_pointer_cast<std::ofstream>(stream);
            //若文件打开失败
            if (!file_stream->is_open())
            {
                //提示并回落控制台输出
                *stream_console() << "[ERROR]" << "文件打开失败\n已切换至控制台输出\n" << std::endl;
                return false;
            }
            //把文件流追加进节点自有容器
            return group_append(node, stream);
        }
        //输出流绑定（路径节点绑定外部输出流）
        static bool stream_bind(const std::string& path, const Stream& stream)
        {
            //若输出流为空则绑定失败
            if (!stream)
                return false;
            //整树加锁（多线程保护）
            std::lock_guard<std::mutex> lock(tree_mtx);
            //定位或创建路径节点并追加输出流
            return group_append(node_ensure(path_normalize(path)), stream);
        }
        //输出流清空（清除路径节点自有输出流，使该节点及其后代回落共享父节点输出流）
        static bool stream_clear(const std::string& path)
        {
            //整树加锁（多线程保护）
            std::lock_guard<std::mutex> lock(tree_mtx);
            //按规范化键查找节点（不新建）
            Stream_Node* node = node_find(path_normalize(path));
            //若节点不存在则清空失败
            if (!node)
                return false;
            //清除节点自有流容器，使该节点转为共享父节点输出流
            node->own_group.reset();
            //清除节点共享流容器缓存，下次输出时按父链重新解析
            node->shared_group.reset();
            return true;
        }
    private:
        //输出流容器别名（容器可存储多个输出流，打印日志时全部输出）
        using Stream_Group = std::shared_ptr<std::vector<Stream>>;
        //输出流容器弱引用别名（共享父节点输出流时使用，不持有容器）
        using Stream_Group_Weak = std::weak_ptr<std::vector<Stream>>;

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
            Stream_Group own_group{};
            //共享输出流容器（节点未设置单独输出流时激活，弱引用最近祖先的自有容器）
            Stream_Group_Weak shared_group{};
        };

        //调用路径规范化（分隔符统一 + 剥离到项目根相对路径 + 末尾补 '/'）
        static std::string path_normalize(const std::string& raw)
        {
            //统一路径分隔符
            std::string path = raw;
            for (char& ch : path)
            {
                //反斜杠统一为正斜杠
                if (ch == '\\')
                    ch = '/';
            }
            //项目根目录名锚点（以其为界剥离出项目内相对路径）
            static const std::string anchor = "白银纪元/";
            //取锚点最后一次出现的位置
            const size_t anchor_pos = path.rfind(anchor);
            //若锚点存在则剥离锚点及其之前的部分
            if (anchor_pos != std::string::npos)
                path.erase(0, anchor_pos + anchor.size());
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
            //末尾补 '/'，使目录节点与文件节点共用同一套路径段前缀比较
            if (!path.empty() && path.back() != '/')
                path.push_back('/');
            //返回规范化路径键
            return path;
        }
        //路径段对齐的公共前缀长度（公共部分不足一个路径段时回退到最近的 '/' 边界）
        static size_t prefix_common(const std::string& left, const std::string& right)
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
        static Stream_Node& node_ensure(const std::string& key)
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
                    //在子链表末尾新建节点（list 追加不影响既有元素地址）
                    node->children.push_back(Stream_Node{});
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
                node->children.push_back(Stream_Node{});
                //取中间节点
                Stream_Node& middle = node->children.back();
                //设置中间节点键（公共前缀）与父指针
                middle.key = match->key.substr(0, length);
                middle.parent = node;
                //把原子节点移入中间节点子链表（splice 不搬移元素，子节点地址与指针保持有效）
                middle.children.splice(middle.children.end(), node->children, match);
                //截短原子节点键（去掉公共前缀部分）并重设父指针
                middle.children.back().key.erase(0, length);
                middle.children.back().parent = &middle;
                //去掉已匹配的公共前缀
                rest.erase(0, length);
                //若剩余键已清空则中间节点即目标节点
                if (rest.empty())
                    return middle;
                //否则以剩余键为中间节点新建子节点
                middle.children.push_back(Stream_Node{});
                //取新建节点
                Stream_Node& created = middle.children.back();
                //设置路径键与父指针
                created.key = rest;
                created.parent = &middle;
                return created;
            }
        }
        //节点查找（按路径键自根向下定位，不新建节点）
        static Stream_Node* node_find(const std::string& key)
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
        //控制台输出流获取（不持有 std::cout 生命周期的非拥有指针）
        static Stream stream_console()
        {
            //控制台流仅需构造一次，以函数局部静态持有
            static Stream stream(&std::cout, [](std::ostream*) {});
            return stream;
        }
        //控制台兜底输出流容器获取（未设置任何文件流时的默认输出目标）
        static Stream_Group group_console()
        {
            //容器仅需构造一次，以函数局部静态持有
            static Stream_Group group = group_create_console();
            return group;
        }
        //控制台兜底输出流容器构建
        static Stream_Group group_create_console()
        {
            //以禁用异常的形式分配容器
            Stream_Group group(new(std::nothrow) std::vector<Stream>());
            //若容器分配失败则返回空容器
            if (!group)
                return group;
            //装入控制台输出流
            group->push_back(stream_console());
            return group;
        }
        //节点自有输出流容器追加（必要时创建容器；新建容器时刷新后代共享容器缓存）
        static bool group_append(Stream_Node& node, const Stream& stream)
        {
            //记录节点此前是否已有自有容器
            const bool owned = static_cast<bool>(node.own_group);
            //若节点自有容器尚未创建则创建
            if (!node.own_group)
                node.own_group = Stream_Group(new(std::nothrow) std::vector<Stream>());
            //若容器不存在则追加失败
            if (!node.own_group)
                return false;
            //把输出流追加进节点自有容器
            node.own_group->push_back(stream);
            //若节点此前无自有容器（本次由共享父流转为单独流输出）
            if (!owned)
            {
                //刷新后代共享容器缓存，使后代改取本节点容器
                group_shared_reset(node);
            }
            return true;
        }
        //后代共享容器缓存刷新（节点新设自有容器后，后代需按父链重新解析共享容器）
        static void group_shared_reset(Stream_Node& node)
        {
            //逐个处理子节点
            for (Stream_Node& child : node.children)
            {
                //清除子节点共享容器缓存
                child.shared_group.reset();
                //递归处理更下游的节点
                group_shared_reset(child);
            }
        }
        //节点生效输出流容器获取（自有优先，否则经 weak_ptr 共享父节点输出流）
        static Stream_Group group_resolve(Stream_Node& node)
        {
            //若节点已设置自有输出流则单独流输出
            if (node.own_group)
                return node.own_group;
            //若共享容器缓存有效则共享父节点输出流
            if (Stream_Group shared = node.shared_group.lock())
                return shared;
            //若存在父节点则向上取用并刷新共享容器缓存
            if (node.parent)
            {
                //递归向父节点取生效容器
                Stream_Group parent_group = group_resolve(*node.parent);
                //记录共享容器缓存
                node.shared_group = parent_group;
                return parent_group;
            }
            //根部兜底：未设置任何文件流时默认使用控制台
            return group_console();
        }
        //树路由输出（调用处路径规范化 → 定位节点 → 解析生效容器 → 逐流输出）
        static void output(const char* file_name, const std::string& text, const std::string& type)
        {
            //规范化调用处路径键
            const std::string key = path_normalize(file_name);
            //构造消息行
            const std::string msg = std::format("[{}]{}", type, text);
            //整树加锁：节点定位与多流写出串行化
            std::lock_guard<std::mutex> lock(tree_mtx);
            //定位或创建调用处节点
            Stream_Node& node = node_ensure(key);
            //取节点生效输出流容器
            Stream_Group group = group_resolve(node);
            //若容器不可用则直接写出到控制台
            if (!group || group->empty())
            {
                std::cout << msg << std::endl;
                return;
            }
            //逐个流输出（容器内所有流均输出日志信息）
            for (const Stream& stream : *group)
            {
                //跳过空流
                if (stream)
                    *stream << msg << std::endl;
            }
        }

        //流节点树根（路径键为空串）
        inline static Stream_Node node_root{};
        //整树互斥锁（建节点 / 设流 / 清流 / 输出共用，实现多线程保护）
        inline static std::mutex tree_mtx{};
    };

    //日志系统全局实例
    inline Log logger;
}

namespace std
{
    template <>
    struct std::formatter<std::error_code>
    {
        // 简单实现：忽略格式说明符，直接输出错误码值和消息
        constexpr auto parse(format_parse_context& ctx) 
        {
            return ctx.begin(); // 接受任何格式，不做特殊解析
        }

        auto format(const std::error_code& ec, format_context& ctx) const {
            // 注意：message() 可能抛出 bad_alloc，这里简单处理
            return std::format_to(ctx.out(), "[{}] {}", ec.value(), ec.message());
        }
    };
}