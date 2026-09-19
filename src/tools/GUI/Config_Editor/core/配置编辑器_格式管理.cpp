//============================================================================
// 配置编辑器 —— 配置格式管理（格式管理窗口/保存格式编辑）
// 由 配置编辑器.cpp 拆分而来（架构改革 阶段 1），行为与拆分前完全一致
//============================================================================
#include "src/tools/GUI/Config_Editor/配置编辑器.h"
#include "src/tools/GUI/Config_Editor/配置编辑器_内部工具.h"

namespace engine
{
    //渲染配置格式管理窗口
    void 配置编辑器::渲染格式管理窗口()
    {
        if (!显示格式管理)
            return;

        //首次打开时若格式集合为空，尝试加载
        if (仓库.获取格式全部().empty())
        {
            仓库.加载格式();
            if (!仓库.获取格式全部().empty())
            {
                格式编辑 = 仓库.获取格式全部().front();
                格式编辑有效 = true;
            }
        }

        //窗口尺寸
        const ImVec2 屏幕 = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowSize(ImVec2(屏幕.x * 0.72f, 屏幕.y * 0.78f), ImGuiCond_Appearing);
        ImGui::SetNextWindowPos(ImVec2(屏幕.x * 0.5f, 屏幕.y * 0.5f),
            ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        //粉色不透明背景（与帮助窗口一致，避免透字）
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.50f, 0.30f, 0.54f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.62f, 0.40f, 0.66f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(0.72f, 0.38f, 0.68f, 1.00f));

        if (ImGui::Begin("配置格式管理", &显示格式管理, ImGuiWindowFlags_NoCollapse))
        {
            float 左栏宽 = ImGui::GetContentRegionAvail().x * 0.30f;

            // —— 左栏：格式列表 ——
            ImGui::BeginChild("##格式列表", ImVec2(左栏宽, 0), true);
            {
                const auto& 格式集合 = 仓库.获取格式全部();
                for (int i = 0; i < (int)格式集合.size(); ++i)
                {
                    const auto& fmt = 格式集合[i];
                    std::string 标签 = fmt.模块名 + (fmt.内置 ? "（内置）" : "（自定义）");
                    bool selected = 格式编辑有效 && (格式编辑.模块名 == fmt.模块名);
                    if (ImGui::Selectable(标签.c_str(), selected))
                    {
                        格式编辑 = fmt;
                        格式编辑有效 = true;
                    }
                }

                ImGui::Separator();
                ImGui::TextDisabled("内置格式仅可查看，不可编辑/删除");

                // —— 新建模块 ——
                ImGui::Spacing();
                ImGui::Text("新建模块：");
                输入文本提示("##新模块名", 新模块名输入, "模块名（如 Effect_Manager）");
                if (ImGui::Button("创建并添加", ImVec2(-1, 0)))
                {
                    std::string module = 新模块名输入;
                    while (!module.empty() && module.front() == ' ') module.erase(module.begin());
                    while (!module.empty() && module.back() == ' ') module.pop_back();

                    if (module.empty())
                    {
                        状态消息 = "模块名不能为空";
                    }
                    else if (仓库.查找格式(module) != nullptr)
                    {
                        状态消息 = "模块已存在：" + module;
                    }
                    else
                    {
                        //创建新格式：默认目录 custom/<模块名>，路由 custom_<模块名>.json
                        配置格式 fmt;
                        fmt.模块名 = module;
                        fmt.配置目录 = "custom/" + module;
                        fmt.路由文件名 = "custom_" + module + ".json";
                        fmt.内置 = false;
                        //默认字段：type（文本，必填）
                        配置字段定义 type_field;
                        type_field.字段名 = "type";
                        type_field.显示名 = "type（条目标识）";
                        type_field.类型 = 配置字段类型::文本;
                        type_field.必填 = true;
                        type_field.说明 = "条目标识，保存后用于文件名";
                        fmt.字段.push_back(std::move(type_field));

                        std::string error;
                        if (仓库.保存格式(fmt, error))
                        {
                            状态消息 = "已创建模块格式：" + module;
                            格式编辑 = fmt;
                            格式编辑有效 = true;
                            新模块名输入.clear();
                        }
                        else
                            状态消息 = "创建失败：" + error;
                    }
                }
            }
            ImGui::EndChild();

            ImGui::SameLine();

            // —— 右栏：字段编辑 ——
            ImGui::BeginChild("##格式编辑", ImVec2(0, 0), true);
            {
                if (!格式编辑有效)
                {
                    ImGui::TextWrapped("请从左侧选择一个配置格式，或创建新模块。");
                }
                else
                {
                    //模块基本信息
                    ImGui::Text("模块：%s", 格式编辑.模块名.c_str());
                    ImGui::TextDisabled("配置目录：config/%s | 路由：route/%s%s",
                        格式编辑.配置目录.c_str(), 格式编辑.路由文件名.c_str(),
                        格式编辑.内置 ? " | 内置格式" : "");

                    if (格式编辑.内置)
                    {
                        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.5f, 1.0f),
                            "内置格式不可编辑，仅供查看字段定义。");
                        ImGui::Separator();
                    }

                    // —— 字段列表 ——
                    ImGui::Text("字段定义：");
                    int 删除字段索引 = -1;

                    for (int i = 0; i < (int)格式编辑.字段.size(); ++i)
                    {
                        auto& f = 格式编辑.字段[i];
                        std::string prefix = "##fmt_" + std::to_string(i) + "_";

                        //字段名
                        ImGui::Text("字段 %d：", i + 1);
                        if (!格式编辑.内置)
                        {
                            ImGui::SetNextItemWidth(180.0f);
                            输入文本((prefix + "name").c_str(), f.字段名);
                            ImGui::SameLine();
                        }
                        else
                        {
                            ImGui::TextDisabled("%s", f.字段名.c_str());
                            ImGui::SameLine();
                        }

                        //类型下拉
                        if (!格式编辑.内置)
                        {
                            if (ImGui::BeginCombo((prefix + "type").c_str(),
                                实体配置仓库::字段类型名称(f.类型), 0))
                            {
                                for (int t = 0; t <= (int)配置字段类型::布尔; ++t)
                                {
                                    auto type = (配置字段类型)t;
                                    bool selected = (f.类型 == type);
                                    if (ImGui::Selectable(实体配置仓库::字段类型名称(type), selected))
                                        f.类型 = type;
                                }
                                ImGui::EndCombo();
                            }
                            ImGui::SameLine();
                        }
                        else
                        {
                            ImGui::TextDisabled("类型：%s", 实体配置仓库::字段类型名称(f.类型));
                        }

                        //必填勾选
                        if (!格式编辑.内置)
                        {
                            ImGui::Checkbox((prefix + "req").c_str(), &f.必填);
                            ImGui::SameLine();
                            ImGui::TextDisabled("必填");
                        }

                        //说明输入（内置格式仅显示）
                        if (!格式编辑.内置)
                        {
                            输入文本((prefix + "desc").c_str(), f.说明);
                        }
                        else
                        {
                            ImGui::TextDisabled("说明：%s", f.说明.c_str());
                        }

                        //删除字段（内置格式不可删）
                        if (!格式编辑.内置)
                        {
                            if (ImGui::SmallButton((std::string("删除字段##") + std::to_string(i)).c_str()))
                                删除字段索引 = i;
                        }

                        ImGui::Separator();
                    }

                    if (删除字段索引 >= 0)
                        格式编辑.字段.erase(格式编辑.字段.begin() + 删除字段索引);

                    // —— 添加字段区（仅自定义格式）——
                    if (!格式编辑.内置)
                    {
                        ImGui::Spacing();
                        ImGui::Text("添加字段：");
                        ImGui::SetNextItemWidth(160.0f);
                        输入文本提示("##新字段名", 新字段名输入, "字段名");
                        ImGui::SameLine();
                        if (ImGui::BeginCombo("##新字段类型", 实体配置仓库::字段类型名称(新字段类型), 0))
                        {
                            for (int t = 0; t <= (int)配置字段类型::布尔; ++t)
                            {
                                auto type = (配置字段类型)t;
                                bool selected = (新字段类型 == type);
                                if (ImGui::Selectable(实体配置仓库::字段类型名称(type), selected))
                                    新字段类型 = type;
                            }
                            ImGui::EndCombo();
                        }
                        ImGui::SameLine();
                        ImGui::Checkbox("##新字段必填", &新字段必填);
                        ImGui::SameLine();
                        ImGui::TextDisabled("必填");
                        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                        输入文本提示("##新字段说明", 新字段说明输入, "字段说明（可选）");
                        if (ImGui::Button("+ 添加字段", ImVec2(160, 0)))
                        {
                            std::string name = 新字段名输入;
                            while (!name.empty() && name.front() == ' ') name.erase(name.begin());
                            while (!name.empty() && name.back() == ' ') name.pop_back();
                            if (name.empty())
                            {
                                状态消息 = "字段名不能为空";
                            }
                            else
                            {
                                //字段名查重
                                bool 重复 = false;
                                for (const auto& existing : 格式编辑.字段)
                                    if (existing.字段名 == name)
                                    {
                                        重复 = true;
                                        break;
                                    }
                                if (重复)
                                {
                                    状态消息 = "字段已存在：" + name;
                                }
                                else
                                {
                                    配置字段定义 field;
                                    field.字段名 = name;
                                    field.显示名 = name;
                                    field.类型 = 新字段类型;
                                    field.必填 = 新字段必填;
                                    field.说明 = 新字段说明输入;
                                    格式编辑.字段.push_back(std::move(field));
                                    新字段名输入.clear();
                                    新字段说明输入.clear();
                                    状态消息 = "已添加字段：" + name;
                                }
                            }
                        }
                    }

                    // —— 底部操作 ——
                    ImGui::Spacing();
                    ImGui::Separator();
                    if (!格式编辑.内置)
                    {
                        if (ImGui::Button("保存格式", ImVec2(140, 0)))
                            保存格式编辑();

                        ImGui::SameLine();
                        //删除格式（二次确认）
                        bool 正在确认 = (待确认删除类型 == std::string("格式:") + 格式编辑.模块名);
                        if (ImGui::Button(正在确认 ? "再次点击确认删除" : "删除格式", ImVec2(160, 0)))
                        {
                            if (!正在确认)
                                待确认删除类型 = std::string("格式:") + 格式编辑.模块名;
                            else
                            {
                                std::string error;
                                if (仓库.删除格式(格式编辑.模块名, error))
                                {
                                    状态消息 = "已删除格式：" + 格式编辑.模块名;
                                    格式编辑有效 = false;
                                    //若当前浏览/新建模块就是被删模块，回退到第一个可用模块
                                    if (当前模块 == 格式编辑.模块名)
                                        当前模块 = "Entity_Manager";
                                    if (新建模块 == 格式编辑.模块名)
                                        新建模块 = "Entity_Manager";
                                }
                                else
                                    状态消息 = "删除失败：" + error;
                                待确认删除类型.clear();
                            }
                        }
                        if (正在确认)
                        {
                            ImGui::SameLine();
                            if (ImGui::SmallButton("取消"))
                                待确认删除类型.clear();
                            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.3f, 1.0f),
                                "注意：删除格式不会删除已生成的配置文件");
                        }
                    }

                    // —— 字段类型说明 ——
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.94f, 1.0f), "字段类型说明：");
                    ImGui::TextWrapped("• 文本：普通字符串输入框");
                    ImGui::TextWrapped("• 脚本路径：文本输入 + assets/scripts 下 Lua 脚本下拉选择，保存时校验脚本存在性");
                    ImGui::TextWrapped("• 文本列表：字符串数组（如 acls 权限列表）");
                    ImGui::TextWrapped("• 事件对列表：二元组数组（如 needed_events，每项 = [分类, 标签]）");
                    ImGui::TextWrapped("• 整数 / 浮点数 / 布尔：数值与开关控件");
                }
            }
            ImGui::EndChild();
        }
        ImGui::End();
        ImGui::PopStyleColor(3);
    }

    //把当前格式编辑副本写回仓库（保存到磁盘 + 更新内存）
    void 配置编辑器::保存格式编辑()
    {
        if (!格式编辑有效)
            return;

        //字段名校验：字段名不得为空
        for (const auto& f : 格式编辑.字段)
        {
            if (f.字段名.empty())
            {
                状态消息 = "字段名不能为空，请检查字段定义";
                return;
            }
        }

        std::string error;
        if (仓库.保存格式(格式编辑, error))
            状态消息 = "格式已保存：" + 格式编辑.模块名;
        else
            状态消息 = "保存失败：" + error;
    }

}
