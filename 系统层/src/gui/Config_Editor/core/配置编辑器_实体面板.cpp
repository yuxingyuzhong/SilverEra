//============================================================================
// 配置编辑器 —— 实体属性面板（属性面板/校验结果/权限列表/事件列表/脚本选择器）
// 由 配置编辑器.cpp 拆分而来（架构改革 阶段 1），行为与拆分前完全一致
//============================================================================
#include "gui/Config_Editor/配置编辑器.h"
#include "gui/Config_Editor/配置编辑器_内部工具.h"

namespace engine
{
    //渲染右侧属性面板
    void 配置编辑器::渲染属性面板()
    {
        if (选中索引 < 0 || 选中索引 >= (int)仓库.获取全部().size())
            return;
        实体配置& cfg = 仓库.获取全部()[选中索引];

        // —— 头部 ——
        ImGui::Text("实体配置：%s", cfg.type.c_str());
        ImGui::TextDisabled("配置文件：%s", cfg.config_path.c_str());
        ImGui::Separator();

        // —— type ——（全宽输入框）
        ImGui::Text("type（实体类型）");
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (输入文本("##type", cfg.type))
            状态消息 = "注意：修改 type 后保存时将自动迁移配置文件";
        ImGui::TextDisabled("(非空)");

        // —— decision_load_path ——
        ImGui::Spacing();
        ImGui::Text("decision_load_path（决策树行为脚本）");
        渲染脚本选择器("##decision_path", cfg.decision_load_path);
        ImGui::TextDisabled("(非空，相对 assets/ 的 Lua 脚本路径；Entity_Manager 用于加载实体行为决策树)");

        // —— acls ——
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("acls（从属权限列表）");
        ImGui::TextDisabled("(非空；master 为当前实体类型，此处填写允许从属的实体类型)");
        渲染权限列表(cfg);

        // —— needed_events ——
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("needed_events（订阅事件列表）");
        ImGui::TextDisabled("(非空；每项 = [分类, 标签]，如 [Entity, Request])");
        渲染事件列表(cfg);

        // —— 操作按钮 ——
        ImGui::Spacing();
        ImGui::Separator();
        if (ImGui::Button("保存当前实体", ImVec2(160, 0)))
        {
            std::string error;
            if (仓库.保存实体(cfg, error))
                状态消息 = "已保存：" + cfg.config_path;
            else
                状态消息 = "保存失败：" + error;
        }
        ImGui::SameLine();
        //删除按钮（二次确认）
        bool 正在确认 = (待确认删除类型 == cfg.type);
        if (ImGui::Button(正在确认 ? "再次点击确认删除" : "删除实体",
            ImVec2(180, 0)))
        {
            if (!正在确认)
                待确认删除类型 = cfg.type;
            else
            {
                std::string error;
                if (仓库.删除实体(cfg.type, error))
                {
                    状态消息 = "已删除实体（路由条目）：" + cfg.type;
                    选中索引 = -1;
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
                "注意：仅移除路由条目，实体文件保留");
        }

        // —— 校验结果（可折叠，默认展开但列表限高，不挤占编辑区）——
        ImGui::Spacing();
        ImGui::Separator();
        if (ImGui::CollapsingHeader("校验结果"))
        {
            渲染校验结果(cfg);
        }
    }

    //渲染校验结果
    void 配置编辑器::渲染校验结果(const 实体配置& cfg)
    {
        std::vector<std::string> errors, warnings;
        bool 通过 = 仓库.校验(cfg, errors, warnings);

        ImGui::Text("校验结果：");
        if (通过 && warnings.empty())
        {
            ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.4f, 1.0f), "  ✓ 校验通过，配置可被引擎加载");
        }
        else
        {
            if (!errors.empty())
            {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.3f, 1.0f),
                    "  ✗ %d 个错误（将导致引擎拒绝加载）：", (int)errors.size());
                //错误列表限高滚动，避免挤占编辑区空间
                ImGui::BeginChild("##错误列表", ImVec2(0, 110.0f), true);
                for (const auto& e : errors)
                    ImGui::BulletText("%s", e.c_str());
                ImGui::EndChild();
            }
            if (!warnings.empty())
            {
                ImGui::TextColored(ImVec4(0.95f, 0.8f, 0.3f, 1.0f),
                    "  ! %d 个警告（不影响加载，但建议修复）：", (int)warnings.size());
                //警告列表限高滚动
                ImGui::BeginChild("##警告列表", ImVec2(0, 90.0f), true);
                for (const auto& w : warnings)
                    ImGui::BulletText("%s", w.c_str());
                ImGui::EndChild();
            }
        }
    }

    //渲染 acls 编辑列表
    void 配置编辑器::渲染权限列表(实体配置& cfg)
    {
        int 删除索引 = -1;

        for (int i = 0; i < (int)cfg.acls.size(); ++i)
        {
            std::string label = "##acl_" + std::to_string(i);
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 40.0f);
            输入文本(label.c_str(), cfg.acls[i]);
            ImGui::SameLine();
            if (ImGui::SmallButton(("删除##delacl_" + std::to_string(i)).c_str()))
                删除索引 = i;
        }

        //移除被删除的条目
        if (删除索引 >= 0)
            cfg.acls.erase(cfg.acls.begin() + 删除索引);

        //添加新条目
        if (ImGui::SmallButton("+ 添加从属类型"))
            cfg.acls.push_back(std::string());
    }


    //渲染 needed_events 编辑列表
    void 配置编辑器::渲染事件列表(实体配置& cfg)
    {
        int 删除索引 = -1;

        for (int i = 0; i < (int)cfg.needed_events.size(); ++i)
        {
            //分类输入（均分剩余宽度，加宽输入框）
            float 宽度 = (ImGui::GetContentRegionAvail().x - 60.0f) * 0.5f;
            ImGui::SetNextItemWidth(宽度);
            输入文本(("##evt_cat_" + std::to_string(i)).c_str(), cfg.needed_events[i].first);
            ImGui::SameLine();
            //标签输入
            ImGui::SetNextItemWidth(宽度);
            输入文本(("##evt_tag_" + std::to_string(i)).c_str(), cfg.needed_events[i].second);
            ImGui::SameLine();
            if (ImGui::SmallButton(("删除##delevt_" + std::to_string(i)).c_str()))
                删除索引 = i;
        }

        if (删除索引 >= 0)
            cfg.needed_events.erase(cfg.needed_events.begin() + 删除索引);

        if (ImGui::SmallButton("+ 添加订阅事件"))
            cfg.needed_events.emplace_back("Entity", "Request");
    }

    //渲染脚本选择器（文本输入 + 脚本下拉，label 用于避免多个选择器 ID 冲突，path 为要编辑的目标路径字段）
    void 配置编辑器::渲染脚本选择器(const char* label, std::string& path)
    {
        //文本输入（label 拼接后缀，避免多个选择器 ID 冲突）
        std::string 输入框标签 = std::string(label) + "_input";
        float 宽度 = ImGui::GetContentRegionAvail().x;
        ImGui::SetNextItemWidth(宽度);
        输入文本(输入框标签.c_str(), path);

        //脚本下拉选择
        std::string 下拉标签 = std::string(label) + "_combo";
        if (ImGui::BeginCombo(下拉标签.c_str(), "从现有脚本选择…", 0))
        {
            for (const auto& script : 脚本列表)
            {
                bool selected = (script == path);
                if (ImGui::Selectable(script.c_str(), selected))
                {
                    path = script;
                    有未保存修改 = true;
                }
            }
            ImGui::EndCombo();
        }

        //刷新脚本列表按钮
        ImGui::SameLine();
        if (ImGui::SmallButton("刷新"))
            脚本列表 = 仓库.获取脚本列表();
    }

}
