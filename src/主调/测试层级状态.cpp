#include "src/主调/测试层级状态.h"

//获取引擎环境（可执行文件目录）
#include "Engine/EngineFoundation/src/detail/methods/文件路径获取.h"
//获取路径字符串转换（中文路径段 → filesystem::path）
#include "Engine/EngineFoundation/src/detail/methods/路径字符串转换.h"

#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace engine
{
    //文件内部工具
    namespace
    {
        //层级与子工程构建目录的相对位置
        struct 层级目录
        {
            //层级
            Test_Layer layer;
            //子工程相对项目根的目录（本层自测即 Test 层根）
            const char* 相对目录;
            //该层级链接的下层（判定范围）
            std::vector<const char*> 依赖层;
        };

        //四个层级的目录与依赖关系
        const std::array<层级目录, test_layer_count>& 层级表(void)
        {
            static const std::array<层级目录, test_layer_count> table = { {
                { Test_Layer::Self, "Application/Test",
                    { "EngineFoundation", "EngineSystem", "EngineRenderer" } },
                { Test_Layer::Foundation, "Application/Test/tests/Foundation",
                    { "EngineFoundation" } },
                { Test_Layer::System, "Application/Test/tests/System",
                    { "EngineFoundation", "EngineSystem" } },
                { Test_Layer::Renderer, "Application/Test/tests/Renderer",
                    { "EngineFoundation", "EngineSystem", "EngineRenderer" } }
            } };
            return table;
        }

        //整份文本读取（失败返回 false）
        bool 文本读取(const std::filesystem::path& path, std::string& text)
        {
            std::ifstream file(path, std::ios::in | std::ios::binary);
            //若文件打不开
            if (!file)
                return false;
            std::ostringstream buffer;
            buffer << file.rdbuf();
            text = buffer.str();
            return true;
        }

        //从 CMake 文本里取 set(<键> "<值>") 的值
        bool 键值读取(const std::string& text, const std::string& key, std::string& value)
        {
            const std::string pattern = "set(" + key + " ";
            const std::size_t pos = text.find(pattern);
            //若键不存在
            if (pos == std::string::npos)
                return false;
            const std::size_t left = text.find('"', pos + pattern.size());
            //若缺少引号包裹
            if (left == std::string::npos)
                return false;
            const std::size_t right = text.find('"', left + 1);
            if (right == std::string::npos)
                return false;
            value = text.substr(left + 1, right - left - 1);
            return !value.empty();
        }

        //从版本锚点头里取 12 位哈希（锚点头注释行形如「哈希：<64 位十六进制>」）
        bool 锚点哈希读取(const std::string& text, std::string& hash)
        {
            static const std::string marker = "哈希：";
            const std::size_t pos = text.find(marker);
            //若未标注哈希
            if (pos == std::string::npos)
                return false;
            std::size_t begin = pos + marker.size();
            //跳过空白
            while (begin < text.size() && (text[begin] == ' ' || text[begin] == '\t'))
                ++begin;
            //取前 12 位十六进制
            std::size_t end = begin;
            while (end < text.size() && std::isxdigit(static_cast<unsigned char>(text[end])) &&
                end - begin < 12)
                ++end;
            if (end - begin < 12)
                return false;
            hash = text.substr(begin, 12);
            return true;
        }

        //子工程构建目录（<项目根>/<相对目录>/out/build/x64-<配置>）
        std::filesystem::path 构建目录获取(const std::filesystem::path& root,
            const char* 相对目录, const std::string& config)
        {
            return root / detail::string_to_path(相对目录) / "out/build" / ("x64-" + config);
        }

        //某层当前可用版本：版本锁优先，锁空则读该层构建树的版本锚点头
        std::string 当前版本获取(const std::filesystem::path& root, const char* 层,
            const std::string& config, const std::string& lock_text)
        {
            //版本锁显式指定时即为重建该层会解析到的版本
            std::string locked;
            if (键值读取(lock_text, std::string("BYJY_LOCK_") + 层 + "_" + config, locked))
                return locked;

            //锁空：读该层构建树导出的版本锚点头
            const std::filesystem::path anchor =
                root / detail::string_to_path("Engine") / detail::string_to_path(层) / "out/build"
                / ("x64-" + config) / detail::string_to_path("include/Engine") / detail::string_to_path(层)
                / detail::string_to_path("版本锚点.h");
            std::string text;
            std::string hash;
            if (文本读取(anchor, text) && 锚点哈希读取(text, hash))
                return hash;
            return std::string();
        }
    }

    //层级显示名
    const char* test_layer_name(Test_Layer layer)
    {
        switch (layer)
        {
        case Test_Layer::Self:
            return "本层自测";
        case Test_Layer::Foundation:
            return "EngineFoundation 子工程";
        case Test_Layer::System:
            return "EngineSystem 子工程";
        case Test_Layer::Renderer:
            return "EngineRenderer 子工程";
        default:
            return "未识别层级";
        }
    }

    //当前配置名
    const char* current_config_name(void)
    {
#if defined(_DEBUG)
        return "Debug";
#else
        return "Release";
#endif
    }

    //项目根目录
    std::string project_root_path(void)
    {
        //可执行文件目录：<项目根>/Application/Test
        const std::filesystem::path root =
            detail::exe_dir_get().parent_path().parent_path();
        return detail::path_to_string(root);
    }

    //版本比对（纯逻辑）
    bool version_mismatch(const std::map<std::string, std::string>& recorded,
        const std::map<std::string, std::string>& current,
        std::vector<std::string>& reasons)
    {
        reasons.clear();
        //逐项比对本轮判定范围内的下层
        for (const auto& [层名, 当前版本] : current)
        {
            //当前版本未知时无从判定，跳过
            if (当前版本.empty())
                continue;
            const auto it = recorded.find(层名);
            //记录里缺少该下层
            if (it == recorded.end() || it->second.empty())
            {
                reasons.push_back(层名 + "：构建记录缺少该层版本");
                continue;
            }
            //记录与当前一致则无问题
            if (it->second == 当前版本)
                continue;
            reasons.push_back(层名 + "：测试库用 " + it->second + "，当前可用 " + 当前版本);
        }
        return !reasons.empty();
    }

    //读取并判定四个层级的状态
    std::vector<Layer_Version_State> layer_version_states_get(void)
    {
        const std::filesystem::path root =
            detail::exe_dir_get().parent_path().parent_path();
        const std::string config = current_config_name();

        //版本锁文本（锁空或文件缺失时不影响后续锚点判定）
        std::string lock_text;
        文本读取(root / detail::string_to_path("Engine/版本锁.cmake"), lock_text);

        std::vector<Layer_Version_State> states;
        for (const 层级目录& entry : 层级表())
        {
            Layer_Version_State state;
            state.layer = entry.layer;

            //读取该子工程构建期写出的依赖版本记录
            const std::filesystem::path record_path =
                构建目录获取(root, entry.相对目录, config) / detail::string_to_path("依赖版本.cmake");
            std::string record_text;
            //记录缺失：该层级未构建或构建树被清理，一律按待更新提示
            if (!文本读取(record_path, record_text))
            {
                state.outdated = true;
                state.record_missing = true;
                state.reasons.push_back(std::string("缺少构建记录：请先重建 ")
                    + entry.相对目录);
                states.push_back(state);
                continue;
            }

            //该层级构建时记录的版本
            std::map<std::string, std::string> recorded;
            for (const char* 层 : entry.依赖层)
            {
                std::string value;
                if (键值读取(record_text, std::string("BYJY_BUILD_AGAINST_") + 层 + "_" + config,
                    value))
                    recorded[层] = value;
            }

            //当前可用版本
            std::map<std::string, std::string> current;
            for (const char* 层 : entry.依赖层)
                current[层] = 当前版本获取(root, 层, config, lock_text);

            //比对并记录原因
            state.outdated = version_mismatch(recorded, current, state.reasons);
            states.push_back(state);
        }
        return states;
    }
}
