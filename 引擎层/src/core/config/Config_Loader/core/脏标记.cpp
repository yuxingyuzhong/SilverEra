#include "../局部命名空间使用.h"
#include "src/tools/Logging/日志系统运行包.h"

//引擎命名空间
namespace engine
{
    //运行盐获取（进程内唯一且只生成一次；跨运行随机，使上一轮遗留脏标记自动失效）
    uint64_t Config_Loader::run_salt_get(void)
    {
        //函数局部静态保证进程内唯一
        static const uint64_t salt = []
            {
                //以随机数生成器产生种子
                size_t seed = static_cast<size_t>(Random_Generator<int64_t>()());
                //混入当前时间，保证跨运行随机
                detail::hash_combine(seed, static_cast<size_t>(
                    std::chrono::steady_clock::now().time_since_epoch().count()));
                //返回本次运行的盐
                return static_cast<uint64_t>(seed);
            }();
        //返回运行盐
        return salt;
    }

    //脏标记计算 —— 路由文件自身
    uint64_t Config_Loader::mark_make(const std::string& file_key)
    {
        //以运行盐为种
        size_t seed = static_cast<size_t>(run_salt_get());
        //混合路由文件路径哈希
        detail::hash_combine(seed, std::hash<std::string>{}(file_key));
        //返回标记
        return static_cast<uint64_t>(seed);
    }

    //脏标记计算 —— 对象×文件
    uint64_t Config_Loader::mark_make(const std::string& object, const std::string& config_path)
    {
        //以运行盐为种
        size_t seed = static_cast<size_t>(run_salt_get());
        //混合对象名哈希
        detail::hash_combine(seed, std::hash<std::string>{}(object));
        //混合配置文件路径哈希
        detail::hash_combine(seed, std::hash<std::string>{}(config_path));
        //返回标记
        return static_cast<uint64_t>(seed);
    }
}