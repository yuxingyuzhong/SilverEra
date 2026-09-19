//============================================================================
// 配置编辑器 —— 主界面 + 实体档案列表/详情（页面导航框架）
// 架构改革 阶段 4：新增程序主界面与页面导航状态机
//   - 主界面：封面图 + 「实体配置」入口按钮 + 次级入口（工具菜单保留）
//   - 实体档案列表：列出全部实体类型，选择后进入详情
//   - 实体档案详情：阶段 4 为基础展示（档案视觉/图片/条目跳转在阶段 5-7 完善）
//============================================================================
#include "src/tools/GUI/Config_Editor/配置编辑器.h"
#include "src/tools/GUI/Config_Editor/配置编辑器_内部工具.h"
#include "src/tools/GUI/Config_Editor/core/配置编辑器_视觉.h"

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
            //标题（渐变文字：两层文字叠出立体感 + 渐变装饰线）
            const char* 标题 = "配置编辑器";
            const float 标题缩放 = 1.6f;
            ImGui::SetWindowFontScale(标题缩放);
            const ImVec2 标题尺寸 = ImGui::CalcTextSize(标题);
            const float 标题x = (ImGui::GetContentRegionAvail().x - 标题尺寸.x) * 0.5f;
            const ImVec2 光标位置 = ImGui::GetCursorScreenPos();
            const ImVec2 标题位置 = ImVec2(光标位置.x + 标题x, 光标位置.y);
            ImDrawList* 主画布 = ImGui::GetWindowDrawList();
            //阴影层（偏移 2px 深紫，形成立体感）
            主画布->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                ImVec2(标题位置.x + 2.0f, 标题位置.y + 2.0f),
                IM_COL32(110, 40, 130, 130), 标题);
            //主体层（亮粉白）
            主画布->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                标题位置, IM_COL32(255, 236, 250, 255), 标题);
            ImGui::SetWindowFontScale(1.0f);

            //渐变装饰线（标题下方：浅粉 → 深粉紫）
            const float 装饰线宽 = 标题尺寸.x * 0.9f;
            const float 装饰x = 标题位置.x + (标题尺寸.x - 装饰线宽) * 0.5f;
            const float 装饰y = 标题位置.y + 标题尺寸.y + 8.0f;
            主画布->AddRectFilledMultiColor(
                ImVec2(装饰x, 装饰y), ImVec2(装饰x + 装饰线宽, 装饰y + 4.0f),
                IM_COL32(255, 170, 220, 255),   //左上：浅粉
                IM_COL32(255, 170, 220, 255),   //右上：浅粉
                IM_COL32(160, 80, 180, 255),    //右下：深粉紫
                IM_COL32(160, 80, 180, 255));   //左下：深粉紫
            //光标推进到装饰线下方（保持后续布局间距）
            ImGui::SetCursorScreenPos(ImVec2(标题位置.x, 装饰y + 8.0f));

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
                    const float 图x = (ImGui::GetContentRegionAvail().x - 显示宽) * 0.5f;
                    ImGui::SetCursorPosX(图x);
                    const ImVec2 图左上 = ImGui::GetCursorScreenPos();
                    const ImVec2 图右下(图左上.x + 显示宽, 图左上.y + 显示高);
                    //圆角 + 柔和投影 + 细边框（替代裸贴图）
                    绘制圆角图片(主画布, (ImTextureID)(intptr_t)封面图纹理,
                        图左上, 图右下, 14.0f);
                    //推进光标到图片下方，保持后续布局
                    ImGui::SetCursorScreenPos(ImVec2(图左上.x, 图右下.y + 8.0f));
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

            // —— 入口卡片（圆角底 + hover 浮现投影 + 图标字符） ——
            const float 按钮宽 = 内容宽 * 0.5f;
            const float 卡片高 = 52.0f;

            //卡片1：实体配置
            ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - 按钮宽) * 0.5f);
            ImGui::PushID("##主界面入口实体");
            if (ImGui::InvisibleButton("##入口实体配置", ImVec2(按钮宽, 卡片高)))
            {
                切换页面(编辑页面::实体档案列表);
            }
            {
                const bool 悬停实体 = ImGui::IsItemHovered();
                const ImVec2 卡左上 = ImGui::GetItemRectMin();
                const ImVec2 卡右下 = ImGui::GetItemRectMax();
                绘制投影卡片(主画布, 卡左上, 卡右下,
                    悬停实体 ? IM_COL32(250, 175, 225, 245) : IM_COL32(238, 155, 212, 235),
                    12.0f, 悬停实体 ? 8.0f : 4.0f, 4);
                const float 文字y = 卡左上.y + (卡片高 - ImGui::GetFontSize()) * 0.5f;
                主画布->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                    ImVec2(卡左上.x + 18.0f, 文字y),
                    IM_COL32(255, 242, 250, 255), "♪  实体配置");
            }
            ImGui::PopID();
            ImGui::Spacing();

            //卡片2：模块配置编辑（工具）
            ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - 按钮宽) * 0.5f);
            ImGui::PushID("##主界面入口工具");
            if (ImGui::InvisibleButton("##入口模块配置", ImVec2(按钮宽, 卡片高 - 8.0f)))
            {
                返回目标页面 = 编辑页面::主界面;
                切换页面(编辑页面::模块配置编辑);
            }
            {
                const bool 悬停工具 = ImGui::IsItemHovered();
                const ImVec2 卡左上 = ImGui::GetItemRectMin();
                const ImVec2 卡右下 = ImGui::GetItemRectMax();
                绘制投影卡片(主画布, 卡左上, 卡右下,
                    悬停工具 ? IM_COL32(245, 185, 230, 240) : IM_COL32(230, 165, 220, 230),
                    12.0f, 悬停工具 ? 8.0f : 4.0f, 4);
                const float 文字y = 卡左上.y + ((卡片高 - 8.0f) - ImGui::GetFontSize()) * 0.5f;
                主画布->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                    ImVec2(卡左上.x + 18.0f, 文字y),
                    IM_COL32(255, 242, 250, 255), "✓  模块配置编辑（工具）");
            }
            ImGui::PopID();
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
                ImDrawList* 列表画布 = ImGui::GetWindowDrawList();
                const ImVec2 卡左上 = ImGui::GetCursorScreenPos();
                const float 卡高 = 62.0f;
                const float 卡宽 = ImGui::GetContentRegionAvail().x;
                const ImVec2 卡右下 = ImVec2(卡左上.x + 卡宽, 卡左上.y + 卡高);

                //1) 多层投影 + 卡片主体底色（hover 时投影更明显）
                const ImU32 卡片底色 = 选中
                    ? IM_COL32(255, 214, 228, 255)
                    : IM_COL32(252, 247, 240, 255);
                绘制投影卡片(列表画布, 卡左上, 卡右下, 卡片底色, 10.0f, 4.0f, 4);

                //2) 左侧主题色条（按 type 哈希取色）
                const ImU32 主题色 = 类型主题色(配置.type);
                列表画布->AddRectFilled(
                    ImVec2(卡左上.x + 1.0f, 卡左上.y + 7.0f),
                    ImVec2(卡左上.x + 6.0f, 卡右下.y - 7.0f),
                    主题色, 2.0f);

                //3) 透明 Child 承载内容与点击（背景/边框交给 DrawList 画）
                ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(0, 0, 0, 0));
                ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
                ImGui::BeginChild("##档案卡片", ImVec2(-1, 卡高), true);
                {
                    //标题行：type（色条右侧留白）
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10.0f);
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.40f, 0.22f, 0.30f, 1.0f));
                    ImGui::TextUnformatted(配置.type.c_str());
                    ImGui::PopStyleColor();

                    //右侧：事件 / 权限 两个彩色圆角标签 chip（事件在左、权限在右）
                    ImDrawList* 卡片画布 = ImGui::GetWindowDrawList();
                    const std::string 事件文本 =
                        "事件 " + std::to_string((int)配置.needed_events.size());
                    const std::string 权限文本 =
                        "权限 " + std::to_string((int)配置.acls.size());
                    const ImVec2 事件尺寸 = ImGui::CalcTextSize(事件文本.c_str());
                    const ImVec2 权限尺寸 = ImGui::CalcTextSize(权限文本.c_str());
                    const float chip内边距x = 8.0f;
                    const float chip内边距y = 3.0f;
                    const float 事件宽 = chip内边距x * 2.0f + 事件尺寸.x;
                    const float 权限宽 = chip内边距x * 2.0f + 权限尺寸.x;
                    const float chip起点y = 卡左上.y + 6.0f;
                    //从右往左布局：先算权限 chip 起点，再算事件 chip 起点
                    const float 权限x = 卡右下.x - 12.0f - 权限宽;
                    const float 事件x = 权限x - 6.0f - 事件宽;
                    绘制标签chip(卡片画布,
                        ImVec2(事件x, chip起点y), 事件文本.c_str(),
                        IM_COL32(235, 120, 200, 255), IM_COL32(255, 250, 255, 255),
                        chip内边距x, chip内边距y, 8.0f);
                    绘制标签chip(卡片画布,
                        ImVec2(权限x, chip起点y), 权限文本.c_str(),
                        IM_COL32(155, 95, 175, 255), IM_COL32(255, 250, 255, 255),
                        chip内边距x, chip内边距y, 8.0f);

                    //第二行：决策脚本摘要
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10.0f);
                    ImGui::TextDisabled("决策脚本：%s", 配置.decision_load_path.c_str());
                }
                ImGui::EndChild();
                //逆序弹出：Border → ChildBorderSize → ChildRounding → ChildBg
                ImGui::PopStyleColor();
                ImGui::PopStyleVar();
                ImGui::PopStyleVar();
                ImGui::PopStyleColor();

                //hover 反馈：轻微提亮 + 边框变粉
                if (ImGui::IsItemHovered())
                {
                    列表画布->AddRectFilled(卡左上, 卡右下, IM_COL32(255, 255, 255, 30), 10.0f);
                    列表画布->AddRect(卡左上, 卡右下, IM_COL32(255, 150, 200, 170), 10.0f, 0, 1.5f);
                }

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

        //0) 档案夹整体投影（柔和多层浮起感）
        绘制投影卡片(画布, 夹起点, 夹终点, IM_COL32(120, 60, 78, 255), 14.0f, 6.0f, 5);

        //1) 封皮顶部高光渐变（上亮下暗，皮质感）——内缩 3px 避免盖住圆角
        绘制垂直渐变矩形(画布,
            ImVec2(夹起点.x + 3.0f, 夹起点.y + 3.0f),
            ImVec2(夹终点.x - 3.0f, 夹起点.y + 夹高 * 0.55f),
            IM_COL32(152, 84, 104, 255), IM_COL32(120, 60, 78, 255));
        //1.1) 封皮内衬过渡色：四周细描边，增强厚度感
        画布->AddRect(夹起点, 夹终点, IM_COL32(98, 46, 64, 255), 14.0f, 0, 2.0f);

        //2) 顶部夹子翻盖带：更深一档的酒红色 + 下缘细高光线
        画布->AddRectFilled(
            ImVec2(夹起点.x + 8.0f, 夹起点.y + 8.0f),
            ImVec2(夹终点.x - 8.0f, 夹起点.y + 46.0f),
            IM_COL32(98, 46, 64, 255), 10.0f);
        画布->AddRectFilled(
            ImVec2(夹起点.x + 8.0f, 夹起点.y + 42.0f),
            ImVec2(夹终点.x - 8.0f, 夹起点.y + 46.0f),
            IM_COL32(150, 82, 104, 255), 4.0f);
        //3) 金属夹子：中央大夹（银灰主体 + 顶部高光 + 底部暗部）+ 两侧小夹
        const float 夹中 = 夹起点.x + 夹宽 * 0.5f;
        画布->AddRectFilled(
            ImVec2(夹中 - 26.0f, 夹起点.y + 32.0f),
            ImVec2(夹中 + 26.0f, 夹起点.y + 52.0f),
            IM_COL32(198, 194, 206, 255), 4.0f);
        画布->AddRectFilled(
            ImVec2(夹中 - 26.0f, 夹起点.y + 32.0f),
            ImVec2(夹中 + 26.0f, 夹起点.y + 42.0f),
            IM_COL32(232, 228, 240, 255), 4.0f);
        画布->AddRectFilled(
            ImVec2(夹中 - 26.0f, 夹起点.y + 46.0f),
            ImVec2(夹中 + 26.0f, 夹起点.y + 52.0f),
            IM_COL32(150, 146, 158, 255), 2.0f);
        for (float 侧 : { 0.18f, 0.82f })
        {
            画布->AddRectFilled(
                ImVec2(夹起点.x + 夹宽 * 侧 - 8.0f, 夹起点.y + 34.0f),
                ImVec2(夹起点.x + 夹宽 * 侧 + 8.0f, 夹起点.y + 50.0f),
                IM_COL32(185, 180, 192, 255), 3.0f);
            画布->AddRectFilled(
                ImVec2(夹起点.x + 夹宽 * 侧 - 8.0f, 夹起点.y + 34.0f),
                ImVec2(夹起点.x + 夹宽 * 侧 + 8.0f, 夹起点.y + 40.0f),
                IM_COL32(220, 215, 226, 255), 2.0f);
        }
        //4) 纸张：米白圆角矩形（档案内页）+ 横线纹理 + 极淡阴影边
        const ImVec2 纸起点 = ImVec2(夹起点.x + 16.0f, 夹起点.y + 52.0f);
        const ImVec2 纸终点 = ImVec2(夹终点.x - 16.0f, 夹终点.y - 16.0f);
        画布->AddRectFilled(纸起点, 纸终点, IM_COL32(252, 248, 240, 255), 8.0f);
        绘制档案纸横线(画布,
            ImVec2(纸起点.x + 10.0f, 纸起点.y + 10.0f),
            ImVec2(纸终点.x - 10.0f, 纸终点.y - 10.0f),
            24.0f, IM_COL32(120, 90, 100, 20));
        画布->AddRect(纸起点, 纸终点, IM_COL32(0, 0, 0, 20), 8.0f);

        // —— 纸张内内容（Child 从纸张左上角 + 内边距开始；背景透明以露出横线） ——
        ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(0, 0, 0, 0));
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

            //左上角图片展示框（圆角卡片 + 柔和投影 + 照片角贴装饰）
            const float 图宽 = 150.0f;
            const float 图高 = 170.0f;
            const ImVec2 图左上 = ImGui::GetCursorScreenPos();
            const ImVec2 图右下 = ImVec2(图左上.x + 图宽, 图左上.y + 图高);
            //圆角卡片底（浅米白，浮起）
            绘制投影卡片(画布, 图左上, 图右下, IM_COL32(250, 240, 232, 255), 10.0f, 4.0f, 3);
            //照片角贴：左上 / 右下 两个半透明小三角（像贴照片的胶带）
            const ImU32 角贴色 = IM_COL32(245, 205, 220, 130);
            画布->AddTriangleFilled(图左上,
                ImVec2(图左上.x + 20.0f, 图左上.y),
                ImVec2(图左上.x, 图左上.y + 20.0f), 角贴色);
            画布->AddTriangleFilled(图右下,
                ImVec2(图右下.x - 20.0f, 图右下.y),
                ImVec2(图右下.x, 图右下.y - 20.0f), 角贴色);
            //透明 Child 承载图片 / 占位 / 上传按钮（背景由上面 DrawList 负责）
            ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(0, 0, 0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
            ImGui::BeginChild("##图片框", ImVec2(图宽, 图高), true);
            {
                if (加载实体图片(配置.type))
                {
                    //等比缩放到框内（居中显示）
                    const float 显示宽 = 图宽 - 24.0f;
                    const float 显示高 = 图高 - 42.0f;
                    float 缩放 = 显示宽 / 实体图片宽表[配置.type];
                    if (实体图片高表[配置.type] * 缩放 > 显示高)
                        缩放 = 显示高 / 实体图片高表[配置.type];
                    const float 图片宽 = 实体图片宽表[配置.type] * 缩放;
                    const float 图片高 = 实体图片高表[配置.type] * 缩放;
                    const ImVec2 光标 = ImGui::GetCursorScreenPos();
                    //圆角图片：阴影 + 圆角 + 细边框
                    绘制圆角图片(ImGui::GetWindowDrawList(),
                        (ImTextureID)(intptr_t)实体图片纹理表[配置.type],
                        ImVec2(光标.x + (显示宽 - 图片宽) * 0.5f,
                               光标.y + (显示高 - 图片高) * 0.5f),
                        ImVec2(光标.x + (显示宽 - 图片宽) * 0.5f + 图片宽,
                               光标.y + (显示高 - 图片高) * 0.5f + 图片高),
                        8.0f);
                }
                else
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.50f, 0.46f, 1.0f));
                    ImGui::TextWrapped("暂无图片\n点击下方按钮上传");
                    ImGui::PopStyleColor();
                }
                //上传按钮（固定在框内底部）
                ImGui::SetCursorPos(ImVec2(8.0f, 图高 - 36.0f));
                if (ImGui::Button("上传图片", ImVec2(图宽 - 16.0f, 26.0f)))
                    上传实体图片(配置.type);
            }
            ImGui::EndChild();
            //逆序弹出：ChildBorderSize → ChildRounding → Border → ChildBg
            ImGui::PopStyleVar();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();
            ImGui::PopStyleColor();

            ImGui::SameLine();

            //右侧基础信息（标签 + 值两栏排版；权限/事件用彩色 chip 流式展示）
            ImGui::BeginGroup();
            {
                ImDrawList* 纸张画布 = ImGui::GetWindowDrawList();
                float 值宽 = ImGui::GetContentRegionAvail().x - 130.0f;
                if (值宽 < 160.0f) 值宽 = 160.0f;

                //实体类型（type）：可编辑
                ImGui::Text("实体类型（type）：");
                ImGui::SetNextItemWidth(值宽);
                输入文本("##档案类型编辑", 配置.type);
                ImGui::Spacing();

                //决策脚本：只读
                ImGui::Text("决策脚本：");
                ImGui::TextWrapped("%s", 配置.decision_load_path.c_str());
                ImGui::Spacing();

                //权限（acls）：chip 流式
                ImGui::Text("权限（acls）：%d 项", (int)配置.acls.size());
                if (!配置.acls.empty())
                {
                    const ImVec2 起点 = ImGui::GetCursorScreenPos();
                    float x = 起点.x;
                    float y = 起点.y;
                    const float 行宽 = ImGui::GetContentRegionAvail().x;
                    for (const auto& 权限 : 配置.acls)
                    {
                        const float 宽 = 绘制标签chip(纸张画布, ImVec2(x, y),
                            权限.c_str(), IM_COL32(155, 95, 175, 255),
                            IM_COL32(255, 250, 255, 255));
                        x += 宽 + 6.0f;
                        if (x - 起点.x > 行宽 - 60.0f)
                        {
                            x = 起点.x;
                            y += 24.0f;
                        }
                    }
                    ImGui::SetCursorScreenPos(ImVec2(起点.x, y + 22.0f));
                }
                ImGui::Spacing();

                //订阅事件（needed_events）：chip 流式
                ImGui::Text("订阅事件（needed_events）：%d 项",
                    (int)配置.needed_events.size());
                if (!配置.needed_events.empty())
                {
                    const ImVec2 起点 = ImGui::GetCursorScreenPos();
                    float x = 起点.x;
                    float y = 起点.y;
                    const float 行宽 = ImGui::GetContentRegionAvail().x;
                    for (const auto& 事件 : 配置.needed_events)
                    {
                        const std::string 文本 = 事件.first + " / " + 事件.second;
                        const float 宽 = 绘制标签chip(纸张画布, ImVec2(x, y),
                            文本.c_str(), IM_COL32(235, 120, 200, 255),
                            IM_COL32(255, 250, 255, 255));
                        x += 宽 + 6.0f;
                        if (x - 起点.x > 行宽 - 60.0f)
                        {
                            x = 起点.x;
                            y += 24.0f;
                        }
                    }
                    ImGui::SetCursorScreenPos(ImVec2(起点.x, y + 22.0f));
                }
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

            //下方条目跳转：并排圆角卡片按钮（进入对应模块配置编辑子页面）
            //进入时记录返回目标与实体类型，子页面编辑后返回档案仍能恢复定位
            ImGui::TextUnformatted("条目跳转：");
            ImGui::Spacing();
            const float 跳转宽 = (ImGui::GetContentRegionAvail().x - 16.0f) * 0.5f;
            const float 跳转高 = 42.0f;

            //按钮1：实体管理器配置
            ImGui::PushID("##跳转实体管理器");
            if (ImGui::InvisibleButton("##实体管理器配置卡", ImVec2(跳转宽, 跳转高)))
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
            {
                const bool 悬停 = ImGui::IsItemHovered();
                const ImVec2 左上 = ImGui::GetItemRectMin();
                const ImVec2 右下 = ImGui::GetItemRectMax();
                绘制投影卡片(画布, 左上, 右下,
                    悬停 ? IM_COL32(236, 130, 200, 255) : IM_COL32(218, 112, 182, 240),
                    10.0f, 悬停 ? 8.0f : 4.0f, 3);
                const float 文字y = 左上.y + (跳转高 - ImGui::GetFontSize()) * 0.5f;
                画布->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                    ImVec2(左上.x + 16.0f, 文字y),
                    IM_COL32(255, 246, 252, 255), "♪  实体管理器配置");
            }
            ImGui::PopID();

            ImGui::SameLine();

            //按钮2：属性槽配置
            ImGui::PushID("##跳转属性槽");
            if (ImGui::InvisibleButton("##属性槽配置卡", ImVec2(跳转宽, 跳转高)))
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
            {
                const bool 悬停 = ImGui::IsItemHovered();
                const ImVec2 左上 = ImGui::GetItemRectMin();
                const ImVec2 右下 = ImGui::GetItemRectMax();
                绘制投影卡片(画布, 左上, 右下,
                    悬停 ? IM_COL32(165, 105, 190, 255) : IM_COL32(148, 92, 172, 240),
                    10.0f, 悬停 ? 8.0f : 4.0f, 3);
                const float 文字y = 左上.y + (跳转高 - ImGui::GetFontSize()) * 0.5f;
                画布->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                    ImVec2(左上.x + 16.0f, 文字y),
                    IM_COL32(255, 246, 252, 255), "✓  属性槽配置");
            }
            ImGui::PopID();

            ImGui::Spacing();
            ImGui::TextDisabled("点开子页面编辑各模块配置，返回后回到本档案");

            ImGui::PopStyleColor();
        }
        ImGui::EndChild();
        ImGui::PopStyleColor(); //弹出纸张 ChildBg 透明色

        ImGui::End();
    }

}
