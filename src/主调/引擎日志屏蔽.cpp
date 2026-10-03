//引擎日志屏蔽 —— 实现（声明见 src/主调/引擎日志屏蔽.h）
#include "Application/Test/src/主调/引擎日志屏蔽.h"

//获取日志系统（含全局 logger 单例）
#include "Engine/EngineCore/src/tools/Logging/日志系统运行包.h"
//获取引擎环境（以可执行文件目录为基准定位日志文件）
#include "Engine/EngineCore/src/tools/Engine_Env/引擎环境.h"
//获取路径字符串转换工具（中文字节与 filesystem::path 互转）
#include "Engine/EngineCore/src/tools/Detail/路径字符串转换.h"

//引擎命名空间
namespace engine
{
    //屏蔽日志文件在可执行文件目录下的相对位置（落在构建产物区 out/，不污染层根）
    static const char* 屏蔽日志相对路径 = "out/引擎运行日志.txt";

    //屏蔽状态（开启幂等：已开启时不再重复绑定，避免节点容器堆积）
    static bool 屏蔽已开启 = false;

    //屏蔽日志路径获取（内部统一入口）
    //注意：相对路径含中文，必须经 string_to_path 构造 —— 直接以 const char* 拼接会让
    //      filesystem::path 按系统本地码页解读 UTF-8 字节，生成乱码文件名
    static std::filesystem::path 屏蔽日志路径获取(void)
    {
        //以可执行文件目录为基准拼接（中文部分按 UTF-8 转换后再拼接）
        return Engine_Env::exe_dir_get() / detail::string_to_path(屏蔽日志相对路径);
    }

    //屏蔽日志文件路径获取
    std::string 日志屏蔽_文件路径获取(void)
    {
        //转为 UTF-8 文本返回
        return detail::path_to_string(屏蔽日志路径获取());
    }

    //屏蔽开启
    void 日志屏蔽_开启(void)
    {
        //已开启则直接返回（幂等）
        if (屏蔽已开启)
            return;

        //日志文件绝对路径
        const std::filesystem::path 日志路径 = 屏蔽日志路径获取();
        //目录缺失则先创建，避免文件打不开而使屏蔽静默失效
        std::error_code 目录信息;
        std::filesystem::create_directories(日志路径.parent_path(), 目录信息);

        //以 filesystem::path 打开文件流（禁用异常的分配形式，每次运行覆盖旧日志）
        std::shared_ptr<std::ofstream> 日志流(
            new(std::nothrow) std::ofstream(日志路径, std::ios::out | std::ios::trunc));
        //分配失败或打开失败则放弃屏蔽（日志继续留在控制台，不阻断测试）
        if (!日志流 || !日志流->is_open())
            return;

        //把根节点绑到该外部输出流：全部调用处的日志沿父链继承此流，不再落控制台
        if (logger.stream_bind("", 日志流))
            屏蔽已开启 = true;
    }

    //屏蔽关闭
    void 日志屏蔽_关闭(void)
    {
        //清除根节点绑定：日志回落到控制台兜底输出目标
        logger.stream_clear("");
        //同步状态，使后续 屏蔽_开启() 能重新绑定
        屏蔽已开启 = false;
    }

    //屏蔽日志落盘
    void 日志屏蔽_落盘(void)
    {
        //把整棵树内所有输出目标的缓冲写出并刷新
        logger.stream_flush();
    }
}