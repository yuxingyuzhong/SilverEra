//============================================================================
// 配置编辑器 —— 模块面板（新建配置区/模块列表/模块面板/属性槽面板/通用配置面板/字段编辑器/通用校验/构建候选）
// 由 配置编辑器.cpp 拆分而来（架构改革 阶段 1），行为与拆分前完全一致
//============================================================================
#include "gui/Config_Editor/配置编辑器.h"
#include "gui/Config_Editor/配置编辑器_内部工具.h"

namespace engine
{
    //渲染左侧新建配置区（模块选择 + 条目名 + 创建按钮）
    void 配置编辑器::渲染新建配置区()
    {
        // —— 目标模块选择 ——
        ImGui::Text("新建配置");
        //模块下拉：内置两种 + 自定义模块
        {
            //构造模块候选列表（与浏览下拉共用同一实现）
            std::vector<std::string> 候选;
            std::vector<std::string> 标签;
            构建模块候选(候选, 标签);

            //确保 新建模块 是有效候选
            if (std::find(候选.begin(), 候选.end(), 新建模块) == 候选.end())
                新建模块 = 候选.front();

            //当前选中项在候选中的索引
            int 当前索引 = 0;
            for (int i = 0; i < (int)候选.size(); ++i)
                if (候选[i] == 新建模块)
                {
                    当前索引 = i;
                    break;
                }

            if (ImGui::BeginCombo("##新建模块", 标签[当前索引].c_str(), 0))
            {
                for (int i = 0; i < (int)候选.size(); ++i)
                {
                    bool selected = (i == 当前索引);
                    if (ImGui::Selectable(标签[i].c_str(), selected))
                    {
                        新建模块 = 候选[i];
                        //切换模块后清空名称输入，避免误带到新格式
                        新条目名输入.clear();
                    }
                }
                ImGui::EndCombo();
            }
        }
        ImGui::TextDisabled("(选择目标模块后按格式创建配置)");

        // —— 条目名输入 ——
        输入文本提示("##新条目名", 新条目名输入, "条目名（如 Goblin / MyItem）");

        // —— 创建按钮 ——
        if (ImGui::Button("创建配置", ImVec2(-1, 0)))
        {
            //去掉首尾空白
            std::string name = 新条目名输入;
            while (!name.empty() && name.front() == ' ') name.erase(name.begin());
            while (!name.empty() && name.back() == ' ') name.pop_back();

            if (name.empty())
            {
                状态消息 = "条目名不能为空";
            }
            else if (新建模块 == "Entity_Manager")
            {
                //实体格式：仅创建实体配置（属性槽配置请在 Property_Manager 模块下创建）
                实体配置 cfg;
                cfg.type = name;
                cfg.decision_load_path = "scripts/behavior/" + name + "_Behavior.lua";
                cfg.acls.push_back(name);
                cfg.needed_events.emplace_back("Entity", "Request");

                std::string error;
                if (仓库.新建实体(cfg, error))
                {
                    状态消息 = "已创建实体配置：" + cfg.type;

                    新条目名输入.clear();
                    脚本列表 = 仓库.获取脚本列表();
                    当前模块 = "Entity_Manager";
                    选中索引 = (int)仓库.获取全部().size() - 1;
                }
                else
                    状态消息 = "创建失败：" + error;
            }
            else if (新建模块 == "Property_Manager")
            {
                //属性槽格式：仅创建属性槽配置
                属性槽配置 prop;
                prop.type = name;
                prop.initialize_path = "scripts/initialize/" + name + ".lua";
                std::string error;
                if (仓库.新建属性槽(prop, error))
                {
                    状态消息 = "已创建属性槽配置：" + prop.type;
                    新条目名输入.clear();
                    脚本列表 = 仓库.获取脚本列表();
                    当前模块 = "Property_Manager";
                    选中索引 = (int)仓库.获取属性槽全部().size() - 1;
                }
                else
                    状态消息 = "创建失败：" + error;
            }
            else
            {
                //自定义模块：按格式定义创建通用配置
                配置格式* fmt = 仓库.查找格式(新建模块);
                if (fmt == nullptr)
                {
                    状态消息 = "模块格式不存在：" + 新建模块;
                }
                else
                {
                    通用配置 cfg;
                    cfg.模块名 = 新建模块;
                    cfg.条目名 = name;
                    cfg.字段值 = 仓库.生成默认字段值(*fmt);
                    //若格式定义含 type/name 字段，将条目名填入
                    if (cfg.字段值.contains("type") && cfg.字段值["type"].is_string())
                        cfg.字段值["type"] = name;
                    else if (cfg.字段值.contains("name") && cfg.字段值["name"].is_string())
                        cfg.字段值["name"] = name;

                    std::string error;
                    if (仓库.新建通用配置(cfg, error))
                    {
                        状态消息 = "已创建配置：" + 新建模块 + "/" + cfg.条目名;
                        新条目名输入.clear();
                        当前模块 = 新建模块;
                        选中通用配置索引 = (int)仓库.获取通用配置全部().size() - 1;
                    }
                    else
                        状态消息 = "创建失败：" + error;
                }
            }
        }
    }

    //渲染左侧模块列表（按 当前模块 显示对应配置）
    void 配置编辑器::渲染模块列表()
    {
        // —— 当前模块下拉（浏览切换）——
        {
            std::vector<std::string> 候选;
            std::vector<std::string> 标签;
            构建模块候选(候选, 标签);
            if (std::find(候选.begin(), 候选.end(), 当前模块) == 候选.end())
                当前模块 = 候选.front();

            int 当前索引 = 0;
            for (int i = 0; i < (int)候选.size(); ++i)
                if (候选[i] == 当前模块)
                {
                    当前索引 = i;
                    break;
                }

            ImGui::Text("当前模块：");
            if (ImGui::BeginCombo("##当前模块", 标签[当前索引].c_str(), 0))
            {
                for (int i = 0; i < (int)候选.size(); ++i)
                {
                    bool selected = (i == 当前索引);
                    if (ImGui::Selectable(标签[i].c_str(), selected))
                    {
                        当前模块 = 候选[i];
                        //切换模块时重置选中状态
                        选中索引 = -1;
                        选中通用配置索引 = -1;
                        待确认删除类型.clear();
                        待确认删除条目.clear();
                    }
                }
                ImGui::EndCombo();
            }
        }
        ImGui::Separator();

        //搜索过滤（不计入未保存修改：搜索只是过滤列表，不改配置）
        输入文本提示("##搜索", 搜索文本, "搜索…", false);
        ImGui::Separator();

        if (当前模块 == "Entity_Manager")
        {
            // —— 实体配置列表 ——
            const auto& 实体集合 = 仓库.获取全部();
            ImGui::BeginChild("##实体列表滚动");
            for (int i = 0; i < (int)实体集合.size(); ++i)
            {
                const auto& cfg = 实体集合[i];
                if (!搜索文本.empty())
                {
                    if (cfg.type.find(搜索文本) == std::string::npos &&
                        cfg.config_path.find(搜索文本) == std::string::npos)
                        continue;
                }
                bool selected = (i == 选中索引);
                if (ImGui::Selectable(cfg.type.c_str(), selected))
                    选中索引 = i;
            }
            ImGui::EndChild();
        }
        else if (当前模块 == "Property_Manager")
        {
            // —— 属性槽配置列表 ——
            const auto& 属性槽集合 = 仓库.获取属性槽全部();
            ImGui::BeginChild("##属性槽列表滚动");
            for (int i = 0; i < (int)属性槽集合.size(); ++i)
            {
                const auto& cfg = 属性槽集合[i];
                if (!搜索文本.empty())
                {
                    if (cfg.type.find(搜索文本) == std::string::npos &&
                        cfg.config_path.find(搜索文本) == std::string::npos)
                        continue;
                }
                bool selected = (i == 选中索引);
                if (ImGui::Selectable(cfg.type.c_str(), selected))
                    选中索引 = i;
            }
            ImGui::EndChild();
        }
        else
        {
            // —— 自定义模块通用配置列表 ——
            const auto& 通用集合 = 仓库.获取通用配置全部();
            ImGui::BeginChild("##通用列表滚动");
            for (int i = 0; i < (int)通用集合.size(); ++i)
            {
                const auto& cfg = 通用集合[i];
                if (cfg.模块名 != 当前模块)
                    continue;
                if (!搜索文本.empty())
                {
                    if (cfg.条目名.find(搜索文本) == std::string::npos &&
                        cfg.config_path.find(搜索文本) == std::string::npos)
                        continue;
                }
                bool selected = (i == 选中通用配置索引);
                if (ImGui::Selectable(cfg.条目名.c_str(), selected))
                    选中通用配置索引 = i;
            }
            ImGui::EndChild();
        }
    }

    //渲染右侧面板（按 当前模块 分发）
    void 配置编辑器::渲染模块面板()
    {
        if (当前模块 == "Entity_Manager")
        {
            if (选中索引 >= 0 && 选中索引 < (int)仓库.获取全部().size())
                渲染属性面板();
            else
                ImGui::TextWrapped("请从左侧列表选择一个实体进行编辑，或点击「创建配置」。");
        }
        else if (当前模块 == "Property_Manager")
        {
            if (选中索引 >= 0 && 选中索引 < (int)仓库.获取属性槽全部().size())
                渲染属性槽面板();
            else
                ImGui::TextWrapped("请从左侧列表选择一个属性槽配置进行编辑，或点击「创建配置」。");
        }
        else
        {
            if (选中通用配置索引 >= 0 &&
                选中通用配置索引 < (int)仓库.获取通用配置全部().size() &&
                仓库.获取通用配置全部()[选中通用配置索引].模块名 == 当前模块)
                渲染通用配置面板();
            else
                ImGui::TextWrapped("请从左侧列表选择一个配置进行编辑，或点击「创建配置」。");
        }
    }

    //渲染属性槽配置面板（Property_Manager 模块）
    void 配置编辑器::渲染属性槽面板()
    {
        if (选中索引 < 0 || 选中索引 >= (int)仓库.获取属性槽全部().size())
            return;
        属性槽配置& prop = 仓库.获取属性槽全部()[选中索引];

        // —— 头部 ——
        ImGui::Text("属性槽配置：%s", prop.type.c_str());
        ImGui::TextDisabled("配置文件：%s", prop.config_path.c_str());
        ImGui::Separator();

        // —— type ——
        ImGui::Text("type（实体类型）");
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (输入文本("##prop_type", prop.type))
            状态消息 = "注意：修改 type 后保存时将自动迁移配置文件";
        ImGui::TextDisabled("(非空)");

        // —— initialize_path ——
        ImGui::Spacing();
        ImGui::Text("initialize_path（属性槽初始化脚本）");
        渲染脚本选择器("##prop_initialize", prop.initialize_path);
        ImGui::TextDisabled("(非空，相对 assets/ 的 Lua 脚本路径；Property_Manager 用于构建属性槽)");

        // —— 操作按钮 ——
        ImGui::Spacing();
        ImGui::Separator();
        if (ImGui::Button("保存属性槽配置", ImVec2(180, 0)))
        {
            std::string error;
            if (仓库.保存属性槽(prop, error))
                状态消息 = "已保存属性槽配置：" + prop.config_path;
            else
                状态消息 = "保存失败：" + error;
        }
        ImGui::SameLine();
        bool 正在确认 = (待确认删除条目 == std::string("属性槽:") + prop.type);
        if (ImGui::Button(正在确认 ? "再次点击确认删除" : "删除属性槽配置", ImVec2(200, 0)))
        {
            if (!正在确认)
                待确认删除条目 = std::string("属性槽:") + prop.type;
            else
            {
                std::string error;
                if (仓库.删除属性槽(prop.type, error))
                {
                    状态消息 = "已删除属性槽配置（路由条目）：" + prop.type;
                    选中索引 = -1;
                }
                else
                    状态消息 = "删除失败：" + error;
                待确认删除条目.clear();
            }
        }
        if (正在确认)
        {
            ImGui::SameLine();
            if (ImGui::SmallButton("取消"))
                待确认删除条目.clear();
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.3f, 1.0f),
                "注意：仅移除路由条目，配置文件保留");
        }

        // —— 校验结果 ——
        ImGui::Spacing();
        ImGui::Separator();
        if (ImGui::CollapsingHeader("校验结果"))
        {
            std::vector<std::string> errors, warnings;
            bool 通过 = 仓库.校验属性槽(prop, errors, warnings);
            ImGui::Text("校验结果：");
            if (通过 && warnings.empty())
            {
                ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.4f, 1.0f), "  ✓ 校验通过");
            }
            else
            {
                if (!errors.empty())
                {
                    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.3f, 1.0f),
                        "  ✗ %d 个错误：", (int)errors.size());
                    ImGui::BeginChild("##属性槽错误列表", ImVec2(0, 90.0f), true);
                    for (const auto& e : errors)
                        ImGui::BulletText("%s", e.c_str());
                    ImGui::EndChild();
                }
                if (!warnings.empty())
                {
                    ImGui::TextColored(ImVec4(0.95f, 0.8f, 0.3f, 1.0f),
                        "  ! %d 个警告：", (int)warnings.size());
                    ImGui::BeginChild("##属性槽警告列表", ImVec2(0, 70.0f), true);
                    for (const auto& w : warnings)
                        ImGui::BulletText("%s", w.c_str());
                    ImGui::EndChild();
                }
            }
        }
    }

    //渲染通用配置面板（自定义模块）
    void 配置编辑器::渲染通用配置面板()
    {
        if (选中通用配置索引 < 0 ||
            选中通用配置索引 >= (int)仓库.获取通用配置全部().size())
            return;
        通用配置& cfg = 仓库.获取通用配置全部()[选中通用配置索引];
        配置格式* fmt = 仓库.查找格式(cfg.模块名);
        if (fmt == nullptr)
        {
            ImGui::TextWrapped("模块格式不存在（可能已被删除），请检查「工具→配置格式管理」。");
            return;
        }

        // —— 头部 ——
        ImGui::Text("配置：%s / %s", cfg.模块名.c_str(), cfg.条目名.c_str());
        ImGui::TextDisabled("配置文件：%s", cfg.config_path.c_str());
        ImGui::Separator();

        // —— 条目名（通用标识字段）——
        ImGui::Text("条目名（配置标识，保存后用于文件名）");
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (输入文本("##通用条目名", cfg.条目名))
            状态消息 = "注意：修改条目名后保存时将自动迁移配置文件";
        ImGui::Separator();

        // —— 按格式字段动态渲染 ——
        渲染通用字段编辑器(cfg, *fmt);

        // —— 操作按钮 ——
        ImGui::Spacing();
        ImGui::Separator();
        if (ImGui::Button("保存配置", ImVec2(160, 0)))
        {
            std::string error;
            if (仓库.保存通用配置(cfg, error))
                状态消息 = "已保存：" + cfg.config_path;
            else
                状态消息 = "保存失败：" + error;
        }
        ImGui::SameLine();
        bool 正在确认 = (待确认删除条目 == cfg.条目名 && cfg.模块名 == 当前模块);
        if (ImGui::Button(正在确认 ? "再次点击确认删除" : "删除配置", ImVec2(180, 0)))
        {
            if (!正在确认)
                待确认删除条目 = cfg.条目名;
            else
            {
                std::string error;
                if (仓库.删除通用配置(cfg.模块名, cfg.条目名, error))
                {
                    状态消息 = "已删除配置（路由条目）：" + cfg.模块名 + "/" + cfg.条目名;
                    选中通用配置索引 = -1;
                }
                else
                    状态消息 = "删除失败：" + error;
                待确认删除条目.clear();
            }
        }
        if (正在确认)
        {
            ImGui::SameLine();
            if (ImGui::SmallButton("取消"))
                待确认删除条目.clear();
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.3f, 1.0f),
                "注意：仅移除路由条目，配置文件保留");
        }

        // —— 校验结果 ——
        ImGui::Spacing();
        ImGui::Separator();
        if (ImGui::CollapsingHeader("校验结果"))
        {
            渲染通用校验结果(*fmt, cfg);
        }
    }

    //渲染通用配置的单个字段编辑器
    void 配置编辑器::渲染通用字段编辑器(通用配置& cfg, const 配置格式& fmt)
    {
        for (const auto& f : fmt.字段)
        {
            //确保字段存在（缺失时补默认值）
            if (!cfg.字段值.contains(f.字段名))
                cfg.字段值[f.字段名] = 生成默认值(f);

            //显示名（有显示名用显示名，否则用字段名）
            const char* 标签 = f.显示名.empty() ? f.字段名.c_str() : f.显示名.c_str();

            ImGui::Text("%s", 标签);
            switch (f.类型)
            {
            case 配置字段类型::文本:
            {
                std::string value = cfg.字段值[f.字段名].is_string()
                    ? cfg.字段值[f.字段名].get<std::string>() : std::string();
                std::string label = "##field_" + f.字段名;
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                if (输入文本(label.c_str(), value))
                    cfg.字段值[f.字段名] = value;
                break;
            }
            case 配置字段类型::脚本路径:
            {
                std::string value = cfg.字段值[f.字段名].is_string()
                    ? cfg.字段值[f.字段名].get<std::string>() : std::string();
                std::string label = "##field_" + f.字段名;
                渲染脚本选择器(label.c_str(), value);
                cfg.字段值[f.字段名] = value;
                break;
            }
            case 配置字段类型::文本列表:
            {
                //从 JSON 读取字符串列表
                std::vector<std::string> list;
                if (cfg.字段值[f.字段名].is_array())
                {
                    for (const auto& item : cfg.字段值[f.字段名])
                        if (item.is_string())
                            list.push_back(item.get<std::string>());
                }

                int 删除索引 = -1;
                for (int i = 0; i < (int)list.size(); ++i)
                {
                    std::string label = "##field_" + f.字段名 + "_" + std::to_string(i);
                    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 40.0f);
                    输入文本(label.c_str(), list[i]);
                    ImGui::SameLine();
                    if (ImGui::SmallButton((std::string("删除##") + f.字段名 + "_" + std::to_string(i)).c_str()))
                        删除索引 = i;
                }
                if (删除索引 >= 0)
                    list.erase(list.begin() + 删除索引);
                if (ImGui::SmallButton((std::string("+ 添加##") + f.字段名).c_str()))
                    list.push_back(std::string());

                //写回 JSON
                cfg.字段值[f.字段名] = nlohmann::json::array();
                for (const auto& item : list)
                    cfg.字段值[f.字段名].push_back(item);
                break;
            }
            case 配置字段类型::事件对列表:
            {
                //从 JSON 读取二元组列表
                std::vector<std::pair<std::string, std::string>> list;
                if (cfg.字段值[f.字段名].is_array())
                {
                    for (const auto& item : cfg.字段值[f.字段名])
                    {
                        if (item.is_array() && item.size() >= 2 &&
                            item[0].is_string() && item[1].is_string())
                            list.emplace_back(item[0].get<std::string>(), item[1].get<std::string>());
                        else if (item.is_object() &&
                            item.contains("category") && item["category"].is_string() &&
                            item.contains("tag") && item["tag"].is_string())
                            list.emplace_back(item["category"].get<std::string>(),
                                item["tag"].get<std::string>());
                    }
                }

                int 删除索引 = -1;
                for (int i = 0; i < (int)list.size(); ++i)
                {
                    float 宽度 = (ImGui::GetContentRegionAvail().x - 60.0f) * 0.5f;
                    ImGui::SetNextItemWidth(宽度);
                    输入文本((std::string("##field_") + f.字段名 + "_cat_" + std::to_string(i)).c_str(),
                        list[i].first);
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(宽度);
                    输入文本((std::string("##field_") + f.字段名 + "_tag_" + std::to_string(i)).c_str(),
                        list[i].second);
                    ImGui::SameLine();
                    if (ImGui::SmallButton((std::string("删除##") + f.字段名 + "_" + std::to_string(i)).c_str()))
                        删除索引 = i;
                }
                if (删除索引 >= 0)
                    list.erase(list.begin() + 删除索引);
                if (ImGui::SmallButton((std::string("+ 添加##") + f.字段名).c_str()))
                    list.emplace_back("", "");

                //写回 JSON（两元素数组）
                cfg.字段值[f.字段名] = nlohmann::json::array();
                for (const auto& item : list)
                    cfg.字段值[f.字段名].push_back({ item.first, item.second });
                break;
            }
            case 配置字段类型::整数:
            {
                int value = cfg.字段值[f.字段名].is_number_integer()
                    ? cfg.字段值[f.字段名].get<int>() : 0;
                ImGui::SetNextItemWidth(200.0f);
                if (ImGui::InputInt((std::string("##field_") + f.字段名).c_str(), &value))
                {
                    cfg.字段值[f.字段名] = value;
                    有未保存修改 = true;
                }
                break;
            }
            case 配置字段类型::浮点数:
            {
                float value = cfg.字段值[f.字段名].is_number()
                    ? cfg.字段值[f.字段名].get<float>() : 0.0f;
                ImGui::SetNextItemWidth(200.0f);
                if (ImGui::InputFloat((std::string("##field_") + f.字段名).c_str(), &value))
                {
                    cfg.字段值[f.字段名] = value;
                    有未保存修改 = true;
                }
                break;
            }
            case 配置字段类型::布尔:
            {
                bool value = cfg.字段值[f.字段名].is_boolean()
                    ? cfg.字段值[f.字段名].get<bool>() : false;
                if (ImGui::Checkbox((std::string("##field_") + f.字段名).c_str(), &value))
                {
                    cfg.字段值[f.字段名] = value;
                    有未保存修改 = true;
                }
                break;
            }
            }

            //字段说明 + 必填标记
            if (!f.说明.empty() || f.必填)
            {
                std::string 提示 = f.必填 ? "(必填) " : "(可选) ";
                提示 += f.说明;
                ImGui::TextDisabled("%s", 提示.c_str());
            }
            ImGui::Spacing();
        }
    }

    //渲染通用配置校验结果
    void 配置编辑器::渲染通用校验结果(const 配置格式& fmt, const 通用配置& cfg)
    {
        std::vector<std::string> errors, warnings;
        bool 通过 = 仓库.校验通用配置(fmt, cfg, errors, warnings);

        ImGui::Text("校验结果：");
        if (通过 && warnings.empty())
        {
            ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.4f, 1.0f), "  ✓ 校验通过");
        }
        else
        {
            if (!errors.empty())
            {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.3f, 1.0f),
                    "  ✗ %d 个错误（将导致引擎拒绝加载）：", (int)errors.size());
                ImGui::BeginChild("##通用错误列表", ImVec2(0, 110.0f), true);
                for (const auto& e : errors)
                    ImGui::BulletText("%s", e.c_str());
                ImGui::EndChild();
            }
            if (!warnings.empty())
            {
                ImGui::TextColored(ImVec4(0.95f, 0.8f, 0.3f, 1.0f),
                    "  ! %d 个警告（不影响加载，但建议修复）：", (int)warnings.size());
                ImGui::BeginChild("##通用警告列表", ImVec2(0, 90.0f), true);
                for (const auto& w : warnings)
                    ImGui::BulletText("%s", w.c_str());
                ImGui::EndChild();
            }
        }
    }

    //构建模块候选列表（新建下拉与浏览下拉共用，消除重复代码）
    void 配置编辑器::构建模块候选(std::vector<std::string>& 候选, std::vector<std::string>& 标签) const
    {
        候选.clear();
        标签.clear();
        for (const auto& fmt : 仓库.获取格式全部())
        {
            候选.push_back(fmt.模块名);
            标签.push_back(fmt.模块名 + (fmt.内置 ? "（内置）" : ""));
        }
        //格式集合为空（首次运行异常）时兜底内置模块，保证下拉框可用
        if (候选.empty())
        {
            候选.push_back("Entity_Manager");
            标签.push_back("Entity_Manager（内置）");
        }
    }

}
