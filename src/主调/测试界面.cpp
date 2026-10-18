//测试操作界面 —— 实现（声明见 测试界面.h）
#include "src/主调/测试界面.h"

//获取测试运行器（读快照 / 下发请求）
#include "src/主调/测试运行器.h"
//获取引擎日志屏蔽（取日志文件路径）
#include "src/主调/引擎日志屏蔽.h"
//获取路径字符串转换（中文日志路径 → filesystem::path）
#include "Engine/EngineCore/src/tools/Detail/路径字符串转换.h"

//Dear ImGui
#include "imgui.h"

#include <algorithm>
#include <fstream>
#include <string>
#include <vector>

//引擎命名空间
namespace engine
{
    //文件内部工具
    namespace
    {
        //界面可见的最大输出行数
        const int 输出上限 = 400;
        //界面可见的最大用例结果行数
        const int 结果上限 = 400;
        //界面可见的最大引擎日志行数
        const int 日志上限 = 300;
        //失败过滤串上限（避免超长过滤串）
        const int 失败过滤上限 = 200;

        //关键词匹配（空关键词视为命中）
        bool 含关键词(const std::string& 文本, const std::string& 关键词)
        {
            if (关键词.empty())
                return true;
            return 文本.find(关键词) != std::string::npos;
        }

        //从当前轮失败用例构造「重跑失败」过滤串（套件.用例:套件.用例…）
        std::string 失败过滤串生成(const std::vector<Case_Result>& 用例表)
        {
            std::string 过滤串;
            int 计数 = 0;
            for (const Case_Result& 用例 : 用例表)
            {
                if (用例.status != Case_Status::Failed)
                    continue;
                if (!过滤串.empty())
                    过滤串 += ":";
                过滤串 += 用例.suite + "." + 用例.name;
                if (++计数 >= 失败过滤上限)
                    break;
            }
            return 过滤串;
        }

        //引擎日志逐帧全量读取并保留尾部（文件不大，简单可靠）
        void 引擎日志刷新(std::vector<std::string>& 日志行)
        {
            //清空旧行
            日志行.clear();
            //日志文件绝对路径（UTF-8 文本 → path）
            const std::string 路径文本 = 日志屏蔽_文件路径获取();
            //打开日志文件（中文路径须经 string_to_path）
            std::ifstream 流(detail::string_to_path(路径文本), std::ios::in);
            if (!流)
                return;
            //逐行读取
            std::string 行;
            while (std::getline(流, 行))
            {
                if (行.empty())
                    continue;
                日志行.push_back(行);
            }
            //保留尾部，避免界面占满
            if (static_cast<int>(日志行.size()) > 日志上限)
                日志行.erase(日志行.begin(), 日志行.end() - 日志上限);
        }
    }

    //测试操作界面绘制
    void 测试界面_绘制(Test_Runner& runner, Test_Selection_Model& model, bool& exit_requested)
    {
        //—— 界面持有状态（跨帧保持） ——
        //搜索结果缓冲
        static char 搜索缓冲[128] = "";
        //选中结果行（-1 表示无选中）
        static int 选中结果 = -1;
        //是否只看失败
        static bool 只看失败 = false;
        //输出面板自动滚动
        static bool 自动滚动 = true;
        //引擎日志缓存
        static std::vector<std::string> 日志行;

        //每帧取只读快照
        std::vector<Case_Result> 用例表;
        runner.cases_snapshot(用例表, 结果上限);
        std::vector<Round_Summary> 轮次表;
        runner.rounds_snapshot(轮次表);
        std::vector<Output_Line> 输出表;
        runner.output_tail(输出表, 输出上限);
        int 已完成 = 0;
        int 总数 = 0;
        runner.progress_get(已完成, 总数);

        //—— 顶部：运行控制 + 进度 ——
        //运行选中套件 / 用例
        if (ImGui::Button("运行选中"))
            runner.request_run(model.filter_string_build());
        ImGui::SameLine();
        //重跑失败（无失败则禁用）
        const std::string 失败过滤 = 失败过滤串生成(用例表);
        const bool 有失败 = !失败过滤.empty();
        ImGui::BeginDisabled(!有失败);
        if (ImGui::Button("重跑失败"))
            runner.request_run(失败过滤);
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("清空结果"))
        {
            runner.clear_results();
            选中结果 = -1;
        }
        ImGui::SameLine();
        //停止：跑完当前轮后不再排队
        ImGui::BeginDisabled(!runner.is_running());
        if (ImGui::Button("停止"))
            runner.request_stop();
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("退出界面"))
            exit_requested = true;

        //进度条与计数
        const float 进度 = 总数 > 0 ? static_cast<float>(已完成) / static_cast<float>(总数) : 0.0f;
        ImGui::ProgressBar(进度, ImVec2(-1.0f, 0.0f),
            (已完成 == 0 && 总数 == 0) ? "空闲" : (std::to_string(已完成) + " / " + std::to_string(总数)).c_str());
        //统计当前快照中的状态计数
        int 通过数 = 0;
        int 失败数 = 0;
        int 跳过数 = 0;
        for (const Case_Result& 用例 : 用例表)
        {
            if (用例.status == Case_Status::Passed)
                ++通过数;
            else if (用例.status == Case_Status::Failed)
                ++失败数;
            else
                ++跳过数;
        }
        ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f), "通过 %d", 通过数);
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "失败 %d", 失败数);
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.4f, 1.0f), "跳过 %d", 跳过数);
        ImGui::Separator();

        //—— 中部：左侧选择树 / 右侧结果与输出 ——
        //左列：套件 / 用例选择树
        ImGui::BeginChild("##选择区", ImVec2(320.0f, 0.0f), ImGuiChildFlags_Borders);
        {
            //批量操作与搜索
            if (ImGui::Button("全选"))
                model.all_check();
            ImGui::SameLine();
            if (ImGui::Button("清空"))
                model.all_clear();
            ImGui::SameLine();
            if (ImGui::Button("反选"))
                model.check_invert();
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputTextWithHint("##搜索", "搜索模块或用例名…", 搜索缓冲, sizeof(搜索缓冲));
            ImGui::Text("已选 %zu / %zu", model.checked_case_count(), model.case_count());
            ImGui::Separator();

            const std::string 关键词 = 搜索缓冲;
            const std::vector<Suite_Entry>& 套件表 = model.suite_table_get();
            for (std::size_t 套件序号 = 0; 套件序号 < 套件表.size(); ++套件序号)
            {
                const Suite_Entry& 套件 = 套件表[套件序号];
                //套件名是否命中关键词
                const bool 套件命中 = 含关键词(套件.name, 关键词);
                //是否有用例命中关键词
                bool 有用例命中 = false;
                for (const Case_Entry& 用例 : 套件.case_list)
                {
                    if (含关键词(用例.name, 关键词))
                    {
                        有用例命中 = true;
                        break;
                    }
                }
                //都未命中则整组隐藏
                if (!套件命中 && !有用例命中)
                    continue;

                ImGui::PushID(static_cast<int>(套件序号));
                //套件级复选框
                bool 套件勾选 = model.suite_all_checked(套件序号);
                if (ImGui::Checkbox("##套件勾选", &套件勾选))
                    model.suite_check_set(套件序号, 套件勾选);
                ImGui::SameLine();
                //已选计数标签
                std::size_t 套件已选 = 0;
                for (const Case_Entry& 用例 : 套件.case_list)
                {
                    if (用例.checked)
                        ++套件已选;
                }
                const std::string 节点标签 = 套件.name + "（" + std::to_string(套件已选)
                    + "/" + std::to_string(套件.case_list.size()) + "）";
                //展开态由模型持有
                ImGui::SetNextItemOpen(model.suite_expanded(套件序号), ImGuiCond_Always);
                const bool 展开 = ImGui::TreeNodeEx("##节点",
                    ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth,
                    "%s", 节点标签.c_str());
                if (展开 != model.suite_expanded(套件序号))
                    model.suite_expand_set(套件序号, 展开);
                if (展开)
                {
                    for (std::size_t 用例序号 = 0; 用例序号 < 套件.case_list.size(); ++用例序号)
                    {
                        const Case_Entry& 用例 = 套件.case_list[用例序号];
                        if (!套件命中 && !含关键词(用例.name, 关键词))
                            continue;
                        ImGui::PushID(static_cast<int>(用例序号));
                        bool 用例勾选 = 用例.checked;
                        if (ImGui::Checkbox(用例.name.c_str(), &用例勾选))
                            model.case_check_set(套件序号, 用例序号, 用例勾选);
                        ImGui::PopID();
                    }
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
        }
        ImGui::EndChild();
        ImGui::SameLine();

        //右列：结果表（上） + gtest 输出镜像（下）
        ImGui::BeginChild("##结果区", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
        {
            //「只看失败」开关
            ImGui::Checkbox("只看失败", &只看失败);
            ImGui::SameLine();
            ImGui::Text("（点击结果行查看失败详情）");
            ImGui::Separator();

            //结果表
            if (ImGui::BeginTable("##结果表", 3,
                ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY))
            {
                ImGui::TableSetupColumn("用例", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("状态", ImGuiTableColumnFlags_WidthFixed, 60.0f);
                ImGui::TableSetupColumn("耗时", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                ImGui::TableHeadersRow();
                for (int 序号 = 0; 序号 < static_cast<int>(用例表.size()); ++序号)
                {
                    const Case_Result& 用例 = 用例表[static_cast<std::size_t>(序号)];
                    //只看失败时过滤
                    if (只看失败 && 用例.status != Case_Status::Failed)
                        continue;
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    //行选中（用于失败详情）
                    const std::string 行标签 = 用例.suite + "." + 用例.name;
                    if (ImGui::Selectable(行标签.c_str(), 选中结果 == 序号,
                        ImGuiSelectableFlags_SpanAllColumns))
                        选中结果 = 序号;
                    ImGui::TableSetColumnIndex(1);
                    if (用例.status == Case_Status::Passed)
                        ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f), "通过");
                    else if (用例.status == Case_Status::Failed)
                        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "失败");
                    else
                        ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.4f, 1.0f), "跳过");
                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("%lld ms", static_cast<long long>(用例.elapsed_ms));
                }
                ImGui::EndTable();
            }

            //选中行的失败详情
            if (选中结果 >= 0 && 选中结果 < static_cast<int>(用例表.size()))
            {
                const Case_Result& 选中用例 = 用例表[static_cast<std::size_t>(选中结果)];
                if (!选中用例.failures.empty())
                {
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "失败详情：%s.%s",
                        选中用例.suite.c_str(), 选中用例.name.c_str());
                    for (const std::string& 消息 : 选中用例.failures)
                        ImGui::TextWrapped("%s", 消息.c_str());
                }
            }
            ImGui::Separator();

            //gtest 输出镜像（滚动文本，按级别着色）
            ImGui::Checkbox("自动滚动", &自动滚动);
            ImGui::SameLine();
            ImGui::Text("gtest 输出（已镜像）");
            ImGui::BeginChild("##输出镜像", ImVec2(0.0f, 220.0f), ImGuiChildFlags_Borders);
            {
                for (const Output_Line& 行 : 输出表)
                {
                    ImVec4 颜色(0.9f, 0.9f, 0.9f, 1.0f);
                    if (行.level == 3)
                        颜色 = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
                    else if (行.level == 2)
                        颜色 = ImVec4(0.4f, 0.9f, 0.4f, 1.0f);
                    else if (行.level == 4)
                        颜色 = ImVec4(0.9f, 0.9f, 0.4f, 1.0f);
                    else if (行.level == 0)
                        颜色 = ImVec4(0.7f, 0.8f, 1.0f, 1.0f);
                    ImGui::TextColored(颜色, "%s", 行.text.c_str());
                }
                if (自动滚动)
                    ImGui::SetScrollHereY(1.0f);
            }
            ImGui::EndChild();
        }
        ImGui::EndChild();

        //—— 底部：轮次历史 + 引擎日志 ——
        ImGui::Separator();
        //轮次历史（横条）
        if (轮次表.empty())
        {
            ImGui::TextDisabled("尚未运行任何一轮测试");
        }
        else
        {
            ImGui::Text("轮次历史：");
            ImGui::SameLine();
            for (std::size_t 轮 = 0; 轮 < 轮次表.size(); ++轮)
            {
                const Round_Summary& 汇总 = 轮次表[轮];
                if (轮 > 0)
                    ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.7f, 0.8f, 1.0f, 1.0f), "第%d轮", 汇总.round);
                ImGui::SameLine();
                ImGui::Text("通过 %d / 失败 %d / 跳过 %d · %lld ms",
                    汇总.passed, 汇总.failed, 汇总.skipped, static_cast<long long>(汇总.elapsed_ms));
            }
        }
        //引擎日志（可折叠）
        if (ImGui::CollapsingHeader("引擎日志（尾部）"))
        {
            //每帧刷新日志尾部
            引擎日志刷新(日志行);
            ImGui::BeginChild("##引擎日志", ImVec2(0.0f, 120.0f), ImGuiChildFlags_Borders);
            for (const std::string& 行 : 日志行)
                ImGui::TextUnformatted(行.c_str());
            ImGui::EndChild();
        }
    }
}
