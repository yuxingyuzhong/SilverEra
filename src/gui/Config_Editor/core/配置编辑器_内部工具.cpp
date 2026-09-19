//============================================================================
// 配置编辑器 —— 内部工具（共享实现）
// 由 配置编辑器.cpp 的匿名命名空间工具拆分而来（架构改革 阶段 1）
// 保留原缩进，仅去掉匿名命名空间外壳，行为与拆分前完全一致。
//============================================================================
#include "gui/Config_Editor/配置编辑器_内部工具.h"

namespace engine
{
    bool 本帧有编辑失焦 = false;

        //解析资源文件的绝对路径：
        //  1. 优先尝试当前工作目录（CWD = exe 所在目录时的常见情况）
        //  2. 失败则用 exe 所在目录拼接（防止 CWD 被外部改变）
        std::string 解析资源路径(const char* 相对路径)
        {
            {
                std::ifstream 探测(相对路径);
                if (探测.good())
                {
                    探测.close();
                    return 相对路径;
                }
            }
#ifdef _WIN32
            char 缓冲[MAX_PATH] = {};
            if (GetModuleFileNameA(nullptr, 缓冲, MAX_PATH) > 0)
            {
                std::filesystem::path exe路径(缓冲);
                std::filesystem::path 候选 = exe路径.parent_path() / 相对路径;
                std::ifstream 探测(候选);
                if (探测.good())
                {
                    探测.close();
                    return 候选.string();
                }
            }
#endif
            return 相对路径;   //都失败就按原路径交给 stb 再试一次
        }

        //std::string 输入框（原理同 imgui_stdlib：直接绑定 std::string 内部缓冲）
        //返回是否发生修改
        bool 输入文本(const char* label, std::string& value)
        {
            //确保缓冲区可写且以 \0 结尾
            if (value.capacity() < value.size() + 1)
                value.reserve(value.size() + 16);
            char* buffer = value.data();   // C++17 起 data() 可写
            buffer[value.size()] = '\0';

            bool changed = ImGui::InputText(label, buffer, value.capacity(), 0);
            if (changed)
                value.resize(std::strlen(buffer));
            //输入框刚渲染完，立即检查「编辑后失焦」并置位帧级脏标记
            //（等帧末再查就查不到这个 item 了，所以必须在渲染点就近检查）
            if (ImGui::IsItemDeactivatedAfterEdit())
                本帧有编辑失焦 = true;
            return changed;
        }

        //带提示文本的输入框（空值时显示灰色提示）
        //计入未保存=false 时该输入框不触发「未保存修改」（如搜索过滤框）
        bool 输入文本提示(const char* label, std::string& value, const char* hint,
            bool 计入未保存)
        {
            if (value.capacity() < value.size() + 1)
                value.reserve(value.size() + 16);
            char* buffer = value.data();
            buffer[value.size()] = '\0';

            bool changed = ImGui::InputTextWithHint(label, hint, buffer, value.capacity(), 0);
            if (changed)
                value.resize(std::strlen(buffer));
            if (计入未保存 && ImGui::IsItemDeactivatedAfterEdit())
                本帧有编辑失焦 = true;
            return changed;
        }

        //生成字段默认值（按字段类型，用于通用配置字段缺失时补默认）
        nlohmann::json 生成默认值(const 配置字段定义& f)
        {
            switch (f.类型)
            {
            case 配置字段类型::文本:
            case 配置字段类型::脚本路径:
                return "";
            case 配置字段类型::文本列表:
            case 配置字段类型::事件对列表:
                return nlohmann::json::array();
            case 配置字段类型::整数:
                return 0;
            case 配置字段类型::浮点数:
                return 0.0;
            case 配置字段类型::布尔:
                return false;
            }
            return "";
        }

}
