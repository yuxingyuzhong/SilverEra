//============================================================================
// 配置编辑器 —— 主界面 + 实体档案列表/详情（页面导航框架）
// 架构改革 阶段 4：新增程序主界面与页面导航状态机
//   - 主界面：封面图 + 「实体配置」入口按钮 + 次级入口（工具菜单保留）
//   - 实体档案列表：列出全部实体类型，选择后进入详情
//   - 实体档案详情：阶段 4 为基础展示（档案视觉/图片/条目跳转在阶段 5-7 完善）
//============================================================================
#include "src/tools/GUI/Config_Editor/配置编辑器.h"
#include "src/tools/GUI/Config_Editor/配置编辑器_内部工具.h"

namespace engine
{
    //页面切换（架构改革 阶段 4）：设置当前页面并重置选中索引
    void 配置编辑器::切换页面(编辑页面 页面)
    {
        当前页面 = 页面;
        //每次进入页面重置选中索引，避免残留上一页的选中状态
        选中索引 = -1;
        选中通用配置索引 = -1;
    }

    //渲染主界面（封面图 + 实体配置入口按钮 + 次级入口）
    void 配置编辑器::渲染主界面()
    {
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("配置编辑器", nullptr,
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_MenuBar);

        //菜单栏（工具/帮助等次级入口保留）
        渲染菜单栏();

        // —— 居中内容区 ——
        const ImVec2 视口 = ImGui::GetIO().DisplaySize;
        const float 内容宽 = 视口.x * 0.72f;
        const float 内容高 = 视口.y * 0.80f;
        ImGui::SetCursorPos(ImVec2((视口.x - 内容宽) * 0.5f, (视口.y - 内容高) * 0.5f));
        ImGui::BeginChild("##主界面内容", ImVec2(内容宽, 内容高), true);
        {
            //标题
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.92f, 1.0f, 1.0f));
            ImGui::SetWindowFontScale(1.6f);
            const char* 标题 = "配置编辑器";
            float 标题宽 = ImGui::CalcTextSize(标题).x;
            ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - 标题宽) * 0.5f);
            ImGui::TextUnformatted(标题);
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PopStyleColor();

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            //封面图（assets/UI/cyrene_cover.jpg，首次需要时加载一次）
            if (封面图已尝试 || 加载封面图())
            {
                if (封面图纹理 != 0)
                {
                    float 区域宽 = ImGui::GetContentRegionAvail().x - 20.0f;
                    float 区域高 = ImGui::GetContentRegionAvail().y * 0.58f;
                    if (区域宽 < 200.0f) 区域宽 = 200.0f;
                    if (区域高 < 150.0f) 区域高 = 150.0f;
                    float 缩放 = 区域宽 / 封面图宽;
                    if (封面图高 * 缩放 > 区域高)
                        缩放 = 区域高 / 封面图高;
                    float 显示宽 = 封面图宽 * 缩放;
                    float 显示高 = 封面图高 * 缩放;
                    ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - 显示宽) * 0.5f);
                    ImGui::Image((ImTextureID)(intptr_t)封面图纹理, ImVec2(显示宽, 显示高));
                }
                else
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.75f, 0.90f, 1.0f));
                    ImGui::TextWrapped("封面图缺失（assets/UI/cyrene_cover.jpg），以占位文本代替。");
                    ImGui::PopStyleColor();
                }
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // —— 入口按钮 ——
            const float 按钮宽 = 内容宽 * 0.5f;
            ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - 按钮宽) * 0.5f);
            if (ImGui::Button("实体配置", ImVec2(按钮宽, 48.0f)))
            {
                切换页面(编辑页面::实体档案列表);
            }
            ImGui::Spacing();
            ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - 按钮宽) * 0.5f);
            if (ImGui::Button("模块配置编辑（工具）", ImVec2(按钮宽, 36.0f)))
            {
                返回目标页面 = 编辑页面::主界面;
                切换页面(编辑页面::模块配置编辑);
            }
            ImGui::Spacing();
            ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - 按钮宽) * 0.5f);
            ImGui::TextDisabled("自定义模块 / 通用配置 / 格式管理请使用上方菜单栏「工具」入口");
        }
        ImGui::EndChild();

        ImGui::End();
    }

    //渲染实体档案列表页（档案卡片风格 + 搜索过滤，架构改革 阶段 5）
    //复用仓库全部实体集合，按「档案卡片」展示：type 标题 + 决策脚本摘要 + 事件/权限统计
    void 配置编辑器::渲染实体档案列表()
    {
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("实体档案列表", nullptr,
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_MenuBar);

        //菜单栏（保留工具/帮助入口）
        渲染菜单栏();

        //返回导航
        if (ImGui::Button("← 返回主界面"))
            切换页面(编辑页面::主界面);
        ImGui::SameLine();
        ImGui::TextDisabled("实体档案列表：选择实体进入档案详情");

        //搜索过滤（复用现有 搜索文本 成员；搜索不计入未保存修改）
        ImGui::SameLine();
        ImGui::SetNextItemWidth(220.0f);
        输入文本提示("##档案搜索", 搜索文本, "搜索实体类型…", false);

        ImGui::Separator();

        const auto& 实体集合 = 仓库.获取全部();
        if (实体集合.empty())
        {
            ImGui::TextWrapped("暂无实体配置。请先在「模块配置编辑」中新建实体。");
        }
        else
        {
            ImGui::BeginChild("##档案列表", ImVec2(0, 0), false);
            for (int i = 0; i < (int)实体集合.size(); ++i)
            {
                const 实体配置& 配置 = 实体集合[i];
                //搜索过滤：type 子串匹配
                if (!搜索文本.empty() &&
                    配置.type.find(搜索文本) == std::string::npos)
                    continue;

                // —— 档案卡片 ——
                ImGui::PushID(i);
                const bool 选中 = (选中索引 == i);
                //卡片背景：选中时浅粉高亮，未选中米白；边框同理
                ImGui::PushStyleColor(ImGuiCol_ChildBg,
                    选中 ? ImVec4(1.00f, 0.88f, 0.93f, 1.0f)
                         : ImVec4(0.99f, 0.97f, 0.93f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Border,
                    选中 ? ImVec4(0.90f, 0.55f, 0.68f, 1.0f)
                         : ImVec4(0.85f, 0.80f, 0.72f, 1.0f));
                ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
                ImGui::BeginChild("##档案卡片", ImVec2(-1, 58), true);
                {
                    //标题行：type + 右侧统计
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.40f, 0.22f, 0.30f, 1.0f));
                    ImGui::TextUnformatted(配置.type.c_str());
                    ImGui::PopStyleColor();

                    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 220.0f);
                    const std::string 统计 =
                        "事件 " + std::to_string((int)配置.needed_events.size()) +
                        " · 权限 " + std::to_string((int)配置.acls.size());
                    ImGui::TextDisabled("%s", 统计.c_str());

                    //第二行：决策脚本摘要
                    ImGui::TextDisabled("决策脚本：%s", 配置.decision_load_path.c_str());
                }
                ImGui::EndChild();
                //逆序弹出：Border → ChildBorderSize → ChildRounding → ChildBg
                ImGui::PopStyleColor();
                ImGui::PopStyleVar();
                ImGui::PopStyleVar();
                ImGui::PopStyleColor();

                //整卡命中：点击卡片进入详情
                if (ImGui::IsItemClicked())
                {
                    选中索引 = i;
                    当前页面 = 编辑页面::实体档案详情;
                }
                ImGui::PopID();
                ImGui::Spacing();
            }
            ImGui::EndChild();
        }

        ImGui::End();
    }

    //渲染实体档案详情页（档案夹视觉外壳，架构改革 阶段 5）
    //视觉：深酒红档案夹外壳 + 顶部金属夹子装饰 + 米白纸张 Child；内容为只读展示
    //（图片上传 / 基础信息编辑 / 条目跳转分别在阶段 6 / 7 实现）
    void 配置编辑器::渲染实体档案详情()
    {
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("实体档案详情", nullptr,
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_MenuBar);

        //菜单栏（保留工具/帮助入口）
        渲染菜单栏();

        //返回导航
        if (ImGui::Button("← 返回列表"))
            切换页面(编辑页面::实体档案列表);
        ImGui::SameLine();
        if (ImGui::Button("返回主界面"))
            切换页面(编辑页面::主界面);

        ImGui::Separator();
        ImGui::Spacing();

        auto& 实体集合 = 仓库.获取全部();
        if (选中索引 < 0 || 选中索引 >= (int)实体集合.size())
        {
            ImGui::TextWrapped("未选中实体，请返回列表重新选择。");
            ImGui::End();
            return;
        }

        实体配置& 配置 = 实体集合[选中索引];

        // —— 档案夹视觉外壳（DrawList 绘制） ——
        ImDrawList* 画布 = ImGui::GetWindowDrawList();
        const ImVec2 视口 = ImGui::GetIO().DisplaySize;
        const float 夹宽 = 视口.x * 0.80f;
        const float 夹高 = 视口.y * 0.72f;
        const ImVec2 夹起点 = ImVec2((视口.x - 夹宽) * 0.5f, 64.0f);
        const ImVec2 夹终点 = ImVec2(夹起点.x + 夹宽, 夹起点.y + 夹高);

        //1) 档案夹封皮：深酒红圆角矩形
        画布->AddRectFilled(夹起点, 夹终点, IM_COL32(120, 60, 78, 255), 14.0f);
        //2) 顶部夹子翻盖带：更深一档的酒红色
        画布->AddRectFilled(
            ImVec2(夹起点.x + 8.0f, 夹起点.y + 8.0f),
            ImVec2(夹终点.x - 8.0f, 夹起点.y + 46.0f),
            IM_COL32(98, 46, 64, 255), 10.0f);
        //3) 金属夹子：中央大夹（银灰主体 + 顶部高光）+ 两侧小夹
        const float 夹中 = 夹起点.x + 夹宽 * 0.5f;
        画布->AddRectFilled(
            ImVec2(夹中 - 26.0f, 夹起点.y + 32.0f),
            ImVec2(夹中 + 26.0f, 夹起点.y + 52.0f),
            IM_COL32(198, 194, 206, 255), 4.0f);
        画布->AddRectFilled(
            ImVec2(夹中 - 26.0f, 夹起点.y + 32.0f),
            ImVec2(夹中 + 26.0f, 夹起点.y + 42.0f),
            IM_COL32(232, 228, 240, 255), 4.0f);
        for (float 侧 : { 0.18f, 0.82f })
        {
            画布->AddRectFilled(
                ImVec2(夹起点.x + 夹宽 * 侧 - 8.0f, 夹起点.y + 34.0f),
                ImVec2(夹起点.x + 夹宽 * 侧 + 8.0f, 夹起点.y + 50.0f),
                IM_COL32(185, 180, 192, 255), 3.0f);
        }
        //4) 纸张：米白圆角矩形（档案内页），加一层极淡的阴影边
        const ImVec2 纸起点 = ImVec2(夹起点.x + 16.0f, 夹起点.y + 52.0f);
        const ImVec2 纸终点 = ImVec2(夹终点.x - 16.0f, 夹终点.y - 16.0f);
        画布->AddRectFilled(纸起点, 纸终点, IM_COL32(252, 248, 240, 255), 8.0f);
        画布->AddRect(纸起点, 纸终点, IM_COL32(0, 0, 0, 20), 8.0f);

        // —— 纸张内内容（Child 从纸张左上角 + 内边距开始） ——
        ImGui::SetCursorScreenPos(ImVec2(纸起点.x + 18.0f, 纸起点.y + 18.0f));
        ImGui::BeginChild("##档案纸张",
            ImVec2(纸终点.x - 纸起点.x - 36.0f, 纸终点.y - 纸起点.y - 36.0f),
            false);
        {
            //纸张上的文字统一深棕色
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.32f, 0.22f, 0.24f, 1.0f));

            //档案标题：实体类型
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.30f, 0.38f, 1.0f));
            ImGui::SetWindowFontScale(1.5f);
            ImGui::TextUnformatted(配置.type.c_str());
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PopStyleColor();
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            //左上角图片展示框（阶段 6：显示实体图片纹理或占位 + 上传按钮）
            const float 图宽 = 150.0f;
            const float 图高 = 170.0f;
            ImGui::BeginChild("##图片框", ImVec2(图宽, 图高), true);
            {
                if (加载实体图片(配置.type))
                {
                    //等比缩放到框内（居中显示）
                    const float 显示宽 = 图宽 - 12.0f;
                    const float 显示高 = 图高 - 12.0f;
                    float 缩放 = 显示宽 / 实体图片宽表[配置.type];
                    if (实体图片高表[配置.type] * 缩放 > 显示高)
                        缩放 = 显示高 / 实体图片高表[配置.type];
                    const ImVec2 光标 = ImGui::GetCursorScreenPos();
                    ImGui::SetCursorScreenPos(ImVec2(
                        光标.x + (显示宽 - 实体图片宽表[配置.type] * 缩放) * 0.5f,
                        光标.y + (显示高 - 实体图片高表[配置.type] * 缩放) * 0.5f));
                    ImGui::Image((ImTextureID)(intptr_t)实体图片纹理表[配置.type],
                        ImVec2(实体图片宽表[配置.type] * 缩放,
                               实体图片高表[配置.type] * 缩放));
                }
                else
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.50f, 0.46f, 1.0f));
                    ImGui::TextWrapped("暂无图片\n点击下方按钮上传");
                    ImGui::PopStyleColor();
                }
                //上传按钮（固定在框内底部）
                ImGui::SetCursorPos(ImVec2(8.0f, 图高 - 34.0f));
                if (ImGui::Button("上传图片", ImVec2(图宽 - 16.0f, 26.0f)))
                    上传实体图片(配置.type);
            }
            ImGui::EndChild();

            ImGui::SameLine();

            //右侧基础信息（阶段 6：type 可编辑 + 保存；决策脚本/权限/事件只读展示）
            ImGui::BeginGroup();
            {
                ImGui::Text("实体类型（type）：");
                ImGui::SetNextItemWidth(280.0f);
                输入文本("##档案类型编辑", 配置.type);
                ImGui::Spacing();

                ImGui::Text("决策脚本：");
                ImGui::TextWrapped("%s", 配置.decision_load_path.c_str());
                ImGui::Spacing();
                ImGui::Text("权限（acls）：%d 项", (int)配置.acls.size());
                for (const auto& 权限 : 配置.acls)
                    ImGui::BulletText("%s", 权限.c_str());
                ImGui::Spacing();
                ImGui::Text("订阅事件（needed_events）：%d 项",
                    (int)配置.needed_events.size());
                for (const auto& 事件 : 配置.needed_events)
                    ImGui::BulletText("%s / %s", 事件.first.c_str(), 事件.second.c_str());
                ImGui::Spacing();

                //保存档案（type 变更会同步迁移文件与路由表）
                if (ImGui::Button("保存档案"))
                {
                    std::string error;
                    if (仓库.保存实体(配置, error))
                    {
                        状态消息 = "已保存：" + 配置.config_path;
                        有未保存修改 = false;
                    }
                    else
                        状态消息 = "保存失败：" + error;
                }
            }
            ImGui::EndGroup();

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            //下方条目跳转按钮（架构改革 阶段 7）：进入对应模块配置编辑子页面
            //进入时记录返回目标与实体类型，子页面编辑后返回档案仍能恢复定位
            ImGui::TextUnformatted("条目跳转：");
            ImGui::SameLine();
            if (ImGui::Button("实体管理器配置", ImVec2(180, 0)))
            {
                子页面返回实体类型 = 配置.type;
                当前模块 = "Entity_Manager";
                返回目标页面 = 编辑页面::实体档案详情;
                切换页面(编辑页面::模块配置编辑);
                //自动定位到当前档案对应的实体条目（切换页面会重置选中索引，故在其后设置）
                const auto& 实体集合 = 仓库.获取全部();
                for (int i = 0; i < (int)实体集合.size(); ++i)
                    if (实体集合[i].type == 子页面返回实体类型)
                    {
                        选中索引 = i;
                        break;
                    }
            }
            ImGui::SameLine();
            if (ImGui::Button("属性槽配置", ImVec2(180, 0)))
            {
                子页面返回实体类型 = 配置.type;
                当前模块 = "Property_Manager";
                返回目标页面 = 编辑页面::实体档案详情;
                切换页面(编辑页面::模块配置编辑);
                //自动定位到与当前档案同 type 的属性槽条目（不存在则保持未选中，由右侧面板提示）
                const auto& 属性槽集合 = 仓库.获取属性槽全部();
                for (int i = 0; i < (int)属性槽集合.size(); ++i)
                    if (属性槽集合[i].type == 子页面返回实体类型)
                    {
                        选中索引 = i;
                        break;
                    }
            }
            ImGui::SameLine();
            ImGui::TextDisabled("点开子页面编辑各模块配置，返回后回到本档案");

            ImGui::PopStyleColor();
        }
        ImGui::EndChild();

        ImGui::End();
    }

}
