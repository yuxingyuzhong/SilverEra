//============================================================================
// 配置编辑器 —— 主窗口（渲染/菜单栏/帮助窗口/关闭确认/孤儿清理/状态栏）
// 由 配置编辑器.cpp 拆分而来（架构改革 阶段 1），行为与拆分前完全一致
//============================================================================
#include "gui/Config_Editor/配置编辑器.h"
#include "gui/Config_Editor/配置编辑器_内部工具.h"

namespace engine
{
    //每帧渲染（架构改革 阶段 4：按当前页面分发）
    void 配置编辑器::渲染()
    {
        //页面导航分发：主界面 / 实体档案列表 / 实体档案详情 / 模块配置编辑
        switch (当前页面)
        {
        case 编辑页面::主界面:         渲染主界面(); break;
        case 编辑页面::实体档案列表:    渲染实体档案列表(); break;
        case 编辑页面::实体档案详情:    渲染实体档案详情(); break;
        case 编辑页面::模块配置编辑:    渲染模块配置编辑(); break;
        }

        //F1 快捷切换帮助窗口
        if (ImGui::IsKeyPressed(ImGuiKey_F1))
            显示帮助窗口 = !显示帮助窗口;

        //Ctrl+S：保存当前选中配置（各面板输入框激活时也生效）
        ImGuiIO& io = ImGui::GetIO();
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false))
            保存当前选中();

        //F5：重新加载全部配置（与菜单一致）
        if (ImGui::IsKeyPressed(ImGuiKey_F5))
            加载();

        //脏标记：任何输入控件被编辑并失焦（或点击类控件值变更）后视为有未保存修改
        //本帧内任意输入框编辑后失焦，由 输入文本/输入文本提示 就近置位帧级标志；
        //这里统一消费（新版 ImGui 无 IsAnyItemDeactivatedAfterEdit，只能逐 item 就近检查）
        if (本帧有编辑失焦)
        {
            有未保存修改 = true;
            本帧有编辑失焦 = false;
        }

        //关闭请求处理：无未保存修改则直接放行，有则弹确认窗
        if (关闭请求)
        {
            关闭请求 = false;
            if (有未保存修改)
                显示关闭确认 = true;
            else
                关闭已确认 = true;
        }
        if (显示关闭确认)
            渲染关闭确认窗口();

        //帮助窗口（独立小窗：可拖动/缩放，内容可滚动，右侧有笑脸）
        渲染帮助窗口();

        //配置格式管理窗口（工具菜单打开）
        渲染格式管理窗口();

        //孤儿配置文件清理窗口（工具菜单打开）
        渲染孤儿清理窗口();
    }

    //渲染模块配置编辑子页面（复用现有面板渲染函数，架构改革 阶段 4）
    void 配置编辑器::渲染模块配置编辑()
    {
        //主窗口（占满视口）
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("配置编辑器", nullptr,
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_MenuBar);

        //菜单栏
        渲染菜单栏();

        //返回导航：子页面左上角提供显式返回按钮，返回目标由进入来源决定（架构改革 阶段 7）
        //从档案详情进入时，返回后按记录的类型重新定位档案，保证「返回档案后状态一致」
        if (ImGui::Button("← 返回"))
        {
            if (返回目标页面 == 编辑页面::实体档案详情)
            {
                //若子页面中编辑过 type，返回时按最新 type 定位档案
                std::string 当前类型;
                if (当前模块 == "Entity_Manager" && 选中索引 >= 0 && 选中索引 < (int)仓库.获取全部().size())
                    当前类型 = 仓库.获取全部()[选中索引].type;
                else if (当前模块 == "Property_Manager" && 选中索引 >= 0 && 选中索引 < (int)仓库.获取属性槽全部().size())
                    当前类型 = 仓库.获取属性槽全部()[选中索引].type;
                if (!当前类型.empty())
                    子页面返回实体类型 = 当前类型;

                当前页面 = 编辑页面::实体档案详情;
                选中索引 = -1;
                选中通用配置索引 = -1;
                const auto& 实体集合 = 仓库.获取全部();
                for (int i = 0; i < (int)实体集合.size(); ++i)
                    if (实体集合[i].type == 子页面返回实体类型)
                    {
                        选中索引 = i;
                        break;
                    }
            }
            else
            {
                切换页面(返回目标页面);
            }
        }
        ImGui::SameLine();
        if (返回目标页面 == 编辑页面::实体档案详情)
            ImGui::TextDisabled("模块配置编辑（返回实体档案）");
        else
            ImGui::TextDisabled("模块配置编辑");

        //左右分栏（左栏宽度自适应：窗口宽度的 27%，限制在 300~520 之间，让列表和输入框更宽）
        float 状态栏高度 = 30.0f;
        float 分栏高度 = ImGui::GetContentRegionAvail().y - 状态栏高度;
        float 左栏宽度 = ImGui::GetIO().DisplaySize.x * 0.27f;
        if (左栏宽度 < 300.0f) 左栏宽度 = 300.0f;
        if (左栏宽度 > 520.0f) 左栏宽度 = 520.0f;
        ImGui::BeginChild("##左栏", ImVec2(左栏宽度, 分栏高度), true);
        //新建配置区（模块选择 + 条目名 + 创建按钮）
        渲染新建配置区();
        ImGui::Separator();
        //按 当前模块 显示配置列表
        渲染模块列表();
        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::BeginChild("##右栏", ImVec2(0, 分栏高度), true);
        //按 当前模块 渲染右侧面板
        渲染模块面板();
        ImGui::EndChild();

        //状态栏
        渲染状态栏();

        ImGui::End();
    }

    //渲染菜单栏
    void 配置编辑器::渲染菜单栏()
    {
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("文件"))
            {
                if (ImGui::MenuItem("重新加载", "F5"))
                {
                    加载();
                }
                ImGui::Separator();
                if (ImGui::MenuItem("保存当前配置", "Ctrl+S"))
                {
                    保存当前选中();
                }
                if (ImGui::MenuItem("保存全部"))
                {
                    保存全部();
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("帮助"))
            {
                if (ImGui::MenuItem("使用说明", "F1"))
                    显示帮助窗口 = true;
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("工具"))
            {
                if (ImGui::MenuItem("配置格式管理", nullptr, &显示格式管理))
                {
                    //打开时同步当前选中格式到编辑副本
                    if (显示格式管理)
                    {
                        if (仓库.获取格式全部().empty())
                            仓库.加载格式();
                        if (!仓库.获取格式全部().empty())
                        {
                            格式编辑 = 仓库.获取格式全部().front();
                            格式编辑有效 = true;
                        }
                        else
                            格式编辑有效 = false;
                    }
                }
                ImGui::Separator();
                //孤儿配置文件清理入口（扫描并删除未被任何路由表引用的配置文件）
                if (ImGui::MenuItem("清理孤儿配置文件"))
                {
                    孤儿清理列表.clear();
                    孤儿清理消息.clear();
                    显示孤儿清理 = true;
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }
    }

    //渲染帮助窗口（独立小窗：正文自动换行可滚动，右侧显示昔涟图片）
    void 配置编辑器::渲染帮助窗口()
    {
        if (!显示帮助窗口)
            return;

        //首次出现时居中显示，尺寸按屏幕比例放大（小窗太小的老问题：至少 860×560）
        const ImVec2 屏幕 = ImGui::GetIO().DisplaySize;
        float 帮助宽 = 屏幕.x * 0.55f;
        float 帮助高 = 屏幕.y * 0.72f;
        if (帮助宽 < 860.0f) 帮助宽 = 860.0f;
        if (帮助高 < 560.0f) 帮助高 = 560.0f;
        if (帮助宽 > 1200.0f) 帮助宽 = 1200.0f;
        if (帮助高 > 860.0f) 帮助高 = 860.0f;
        ImGui::SetNextWindowPos(
            ImVec2(屏幕.x * 0.5f, 屏幕.y * 0.5f),
            ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(帮助宽, 帮助高), ImGuiCond_Appearing);

        //标题栏右侧自带 × 关闭按钮，点击后 显示帮助窗口 自动置 false
        //—— 背景不透明处理（粉色系，与主界面统一；完全不透明避免透出主界面文字）——
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.55f, 0.32f, 0.58f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.72f, 0.48f, 0.80f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(0.72f, 0.38f, 0.68f, 1.00f));
        if (ImGui::Begin("使用说明", &显示帮助窗口, ImGuiWindowFlags_NoCollapse))
        {
            //左右分栏：左侧帮助正文（自动换行 + 滚动），右侧昔涟图片
            float 可用宽度 = ImGui::GetContentRegionAvail().x;
            float 左栏宽度 = 可用宽度 * 0.58f;

            // —— 左侧：帮助正文（自动换行；内容超出自动滚动）——
            ImGui::BeginChild("##帮助正文", ImVec2(左栏宽度, 0), true);
            {
                ImGui::PushTextWrapPos(0.0f);   //0.0f = 换行到当前区域右缘

                ImGui::TextColored(ImVec4(1.0f, 0.80f, 0.94f, 1.0f), "《实体配置字段说明》（config/entities/）");
                ImGui::TextWrapped("• type：实体类型标识（必填，非空，保存后配置文件会自动迁移）");
                ImGui::TextWrapped("• decision_load_path：决策树行为脚本路径（必填，相对 assets/ 目录，Entity_Manager 使用；旧配置缺少该字段时需补填）");
                ImGui::TextWrapped("• acls：从属权限列表（必填，非空，引擎加载时逐项校验）");
                ImGui::TextWrapped("• needed_events：订阅事件列表（必填，非空，每项含 事件源/事件名）");

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                ImGui::TextColored(ImVec4(1.0f, 0.80f, 0.94f, 1.0f), "《属性槽配置字段说明》（config/property/）");
                ImGui::TextWrapped("• type：实体类型标识（必填，与实体配置 type 一致）");
                ImGui::TextWrapped("• initialize_path：属性槽初始化 Lua 脚本路径（必填，相对 assets/ 目录，Property_Manager 使用）");

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                ImGui::TextColored(ImVec4(1.0f, 0.80f, 0.94f, 1.0f), "《常用操作》");
                ImGui::TextWrapped("• 左侧列表点选实体，右侧即可编辑属性，修改后记得保存");
                ImGui::TextWrapped("• 「创建配置」可新建实体，自动生成默认 acls 与 needed_events");
                ImGui::TextWrapped("• 修改 type 后保存，配置文件会自动迁移到新路径");
                ImGui::TextWrapped("• 删除实体仅移除路由条目，实体配置文件本身会保留");

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                ImGui::TextColored(ImVec4(1.0f, 0.80f, 0.94f, 1.0f), "《校验规则》");
                ImGui::TextWrapped("• 引擎 Entity_Manager::config_field_parse 要求 acls 与 needed_events 均不能为空，否则引擎拒绝加载");
                ImGui::TextWrapped("• 右侧「校验结果」面板可实时查看错误与警告，全部通过后保存更稳妥");

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                //自定义模块字段说明（动态读取格式定义，格式有更新时说明自动跟随）
                ImGui::TextColored(ImVec4(1.0f, 0.80f, 0.94f, 1.0f), "《自定义模块字段说明》（config/custom/）");
                {
                    const auto& 格式集合 = 仓库.获取格式全部();
                    bool 有自定义 = false;
                    for (const auto& fmt : 格式集合)
                    {
                        if (fmt.内置)
                            continue;
                        有自定义 = true;
                        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.92f, 1.0f),
                            "◆ %s（config/%s/）", fmt.模块名.c_str(), fmt.配置目录.c_str());
                        for (const auto& f : fmt.字段)
                        {
                            std::string 行 = "• " + f.字段名;
                            if (!f.显示名.empty() && f.显示名 != f.字段名)
                                行 += "（" + f.显示名 + "）";
                            行 += f.必填 ? "（必填）" : "（可选）";
                            if (!f.说明.empty())
                                行 += "：" + f.说明;
                            ImGui::TextWrapped("%s", 行.c_str());
                        }
                    }
                    if (!有自定义)
                        ImGui::TextWrapped("• （暂无自定义模块，可在「工具→配置格式管理」中创建）");
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                ImGui::TextColored(ImVec4(1.0f, 0.80f, 0.94f, 1.0f), "《快捷键》");
                ImGui::TextWrapped("• F1：打开 / 关闭本帮助窗口");
                ImGui::TextWrapped("• F5：重新加载全部配置（文件菜单中也有）");

                ImGui::PopTextWrapPos();
            }
            ImGui::EndChild();

            ImGui::SameLine();

            // —— 右侧：昔涟的图片（懒加载纹理；加载失败时回退为字符画）——
            ImGui::BeginChild("##帮助笑脸", ImVec2(0, 0));
            {
                if (!加载帮助图片())
                {
                    //回退：粉色字符画（图片缺失时仍保留笑脸）
                    const char* 笑脸[] =
                    {
                        "   ♪ ／￣＼　／￣＼ ♪",
                        "    （ ＾ω＾ ）",
                        "     ＼＿／＼＿／",
                        "",
                        "「愿世界，如你我所愿♪」",
                    };
                    const int 行数 = (int)(sizeof(笑脸) / sizeof(笑脸[0]));
                    const float 行高 = ImGui::GetTextLineHeightWithSpacing();
                    float 可用高 = ImGui::GetContentRegionAvail().y;
                    float 内容高 = 行数 * 行高;
                    if (可用高 > 内容高)
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (可用高 - 内容高) * 0.5f);
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.70f, 0.88f, 1.0f));
                    for (int i = 0; i < 行数; ++i)
                    {
                        float 文本宽 = ImGui::CalcTextSize(笑脸[i]).x;
                        float 区域宽 = ImGui::GetContentRegionAvail().x;
                        if (文本宽 < 区域宽)
                            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (区域宽 - 文本宽) * 0.5f);
                        ImGui::TextUnformatted(笑脸[i]);
                    }
                    ImGui::PopStyleColor();
                }
                else
                {
                    //图片：按区域等比缩放、整体垂直居中
                    float 区域宽 = ImGui::GetContentRegionAvail().x - 10.0f;
                    float 区域高 = ImGui::GetContentRegionAvail().y - 10.0f;
                    float 图片比例 = (float)帮助图片宽 / (float)帮助图片高;
                    float 显示宽 = 区域宽;
                    float 显示高 = 显示宽 / 图片比例;
                    if (显示高 > 区域高)
                    {
                        显示高 = 区域高;
                        显示宽 = 显示高 * 图片比例;
                    }

                    const float 行高 = ImGui::GetTextLineHeightWithSpacing();
                    const float 提示高 = 行高 + 8.0f;
                    float 内容总高 = 显示高 + 提示高;
                    if (区域高 > 内容总高)
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (区域高 - 内容总高) * 0.5f);

                    //水平居中
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (区域宽 - 显示宽) * 0.5f);
                    ImGui::Image((ImTextureID)(intptr_t)帮助图片纹理, ImVec2(显示宽, 显示高));

                    //图片下方的小字（居中）
                    const char* 寄语 = "「愿世界，如你我所愿♪」";
                    float 文本宽 = ImGui::CalcTextSize(寄语).x;
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (区域宽 - 文本宽) * 0.5f);
                    ImGui::TextColored(ImVec4(1.0f, 0.80f, 0.94f, 1.0f), "%s", 寄语);
                }
            }
            ImGui::EndChild();
        }
        ImGui::End();
        //恢复默认背景样式
        ImGui::PopStyleColor(3);
    }

    //渲染关闭确认窗口（有未保存修改时弹出：保存并退出 / 直接退出 / 取消）
    void 配置编辑器::渲染关闭确认窗口()
    {
        const ImVec2 屏幕 = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowPos(ImVec2(屏幕.x * 0.5f, 屏幕.y * 0.5f),
            ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(480, 0), ImGuiCond_Always);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.55f, 0.32f, 0.58f, 1.00f));
        if (ImGui::Begin("未保存的修改", &显示关闭确认,
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
        {
            ImGui::TextWrapped("还有未保存的修改，退出前要先保存吗？");
            ImGui::Spacing();
            if (ImGui::Button("保存并退出", ImVec2(140, 0)))
            {
                保存全部();
                显示关闭确认 = false;
                关闭已确认 = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("直接退出", ImVec2(140, 0)))
            {
                显示关闭确认 = false;
                关闭已确认 = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("取消", ImVec2(140, 0)))
                显示关闭确认 = false;
        }
        ImGui::End();
        ImGui::PopStyleColor();
    }

    //渲染孤儿配置文件清理窗口（列出扫描结果，确认后删除）
    void 配置编辑器::渲染孤儿清理窗口()
    {
        if (!显示孤儿清理)
            return;

        const ImVec2 屏幕 = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowPos(ImVec2(屏幕.x * 0.5f, 屏幕.y * 0.5f),
            ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(580, 440), ImGuiCond_Appearing);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.55f, 0.32f, 0.58f, 1.00f));
        if (ImGui::Begin("清理孤儿配置文件", &显示孤儿清理, ImGuiWindowFlags_NoCollapse))
        {
            //首次打开时扫描（列表为空且消息为空 = 还没扫过）
            if (孤儿清理列表.empty() && 孤儿清理消息.empty())
            {
                std::string error;
                int 扫描数 = 仓库.清理孤儿配置(孤儿清理列表, error);
                if (扫描数 < 0)
                {
                    孤儿清理消息 = "扫描失败：" + error;
                    孤儿清理列表.clear();
                }
                else
                    孤儿清理消息 = "发现 " + std::to_string((int)孤儿清理列表.size()) +
                        " 个未被任何路由引用的配置文件";
            }

            if (!孤儿清理消息.empty())
                ImGui::TextWrapped("%s", 孤儿清理消息.c_str());
            ImGui::Separator();

            if (!孤儿清理列表.empty())
            {
                //孤儿文件列表（限高滚动）
                ImGui::BeginChild("##孤儿列表", ImVec2(0, ImGui::GetContentRegionAvail().y - 70.0f), true);
                for (const auto& path : 孤儿清理列表)
                    ImGui::BulletText("%s", path.c_str());
                ImGui::EndChild();

                ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.5f, 1.0f),
                    "删除后不可恢复，请确认这些文件确实不再需要。");
                if (ImGui::Button("确认删除", ImVec2(140, 0)))
                {
                    std::string error;
                    std::vector<std::string> 已删;
                    int 删除数 = 仓库.清理孤儿配置(已删, error);
                    if (删除数 < 0)
                        孤儿清理消息 = "删除失败：" + error;
                    else
                    {
                        孤儿清理消息 = "已删除 " + std::to_string(删除数) + " 个孤儿配置文件";
                        孤儿清理列表.clear();
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("关闭", ImVec2(140, 0)))
                    显示孤儿清理 = false;
            }
            else
            {
                ImGui::TextWrapped("没有发现孤儿配置文件。");
                if (ImGui::Button("关闭", ImVec2(140, 0)))
                    显示孤儿清理 = false;
            }
        }
        ImGui::End();
        ImGui::PopStyleColor();
    }

    //渲染状态栏
    void 配置编辑器::渲染状态栏()
    {
        ImGui::Separator();
        //左侧：当前模块数量 + 加载消息
        int 当前数量 = 0;
        if (当前模块 == "Entity_Manager")
            当前数量 = (int)仓库.获取全部().size();
        else if (当前模块 == "Property_Manager")
            当前数量 = (int)仓库.获取属性槽全部().size();
        else
        {
            for (const auto& cfg : 仓库.获取通用配置全部())
                if (cfg.模块名 == 当前模块)
                    当前数量++;
        }
        ImGui::Text("%s 配置数量：%d", 当前模块.c_str(), 当前数量);
        ImGui::SameLine();
        ImGui::TextDisabled("%s", 加载消息.c_str());
        //右侧：状态消息
        if (!状态消息.empty())
        {
            float 可用宽度 = ImGui::GetContentRegionAvail().x;
            float 文本宽度 = ImGui::CalcTextSize(状态消息.c_str()).x;
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - (文本宽度 < 可用宽度 ? 文本宽度 : 可用宽度));
            ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.95f, 1.0f), "%s", 状态消息.c_str());
        }
    }
}
