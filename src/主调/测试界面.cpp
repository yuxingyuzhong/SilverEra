//测试操作界面 —— 实现（声明见 测试界面.h）
#include "src/主调/测试界面.h"

//获取测试运行器（读快照 / 下发请求）
#include "src/主调/测试运行器.h"
//获取引擎日志屏蔽（取日志文件路径）
#include "src/主调/引擎日志屏蔽.h"
//获取测试层级状态（按子 CMake 层级判断下层库是否已更新）
#include "src/主调/测试层级状态.h"
//获取路径字符串转换（中文日志路径 → filesystem::path）
#include "Engine/EngineFoundation/src/tools/Detail/路径字符串转换.h"

//Dear ImGui
#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>
#include <unordered_map>
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
        //失败用例重跑清单上限（避免超长清单）
        const int 失败清单上限 = 200;

        //—— 主题色 ——
        const ImVec4 主色(0.31f, 0.82f, 1.00f, 1.00f);
        const ImVec4 副色(0.45f, 0.55f, 1.00f, 1.00f);
        const ImVec4 成功色(0.34f, 0.89f, 0.54f, 1.00f);
        const ImVec4 失败色(1.00f, 0.36f, 0.48f, 1.00f);
        const ImVec4 跳过色(1.00f, 0.82f, 0.40f, 1.00f);
        const ImVec4 正文色(0.90f, 0.95f, 1.00f, 1.00f);
        const ImVec4 次要色(0.62f, 0.74f, 0.90f, 1.00f);

        //状态对应颜色
        ImVec4 状态色(Case_Status 状态)
        {
            if (状态 == Case_Status::Passed)
                return 成功色;
            if (状态 == Case_Status::Failed)
                return 失败色;
            return 跳过色;
        }

        //状态对应文字
        const char* 状态文字(Case_Status 状态)
        {
            if (状态 == Case_Status::Passed)
                return "通过";
            if (状态 == Case_Status::Failed)
                return "失败";
            return "跳过";
        }

        //按像素大小临时切换字号（字体系统支持动态字号）
        void 字号压入(float 字号)
        {
            ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0], 字号);
        }

        //卡片样式压入（半透明深蓝底 + 圆角 + 细边框）
        void 卡片样式压入(void)
        {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.055f, 0.095f, 0.17f, 0.66f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.35f, 0.62f, 0.90f, 0.38f));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 12.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        }

        //卡片样式弹出
        void 卡片样式弹出(void)
        {
            ImGui::PopStyleVar(3);
            ImGui::PopStyleColor(2);
        }

        //卡片标题（左侧亮条 + 标题 + 可选副标题）
        void 卡片标题(const char* 标题, const char* 副标题 = nullptr)
        {
            const ImVec2 起点 = ImGui::GetCursorScreenPos();
            //标题左侧的竖直亮条
            ImGui::GetWindowDrawList()->AddRectFilled(起点, ImVec2(起点.x + 3.0f, 起点.y + 18.0f),
                ImGui::GetColorU32(主色), 2.0f);
            ImGui::Dummy(ImVec2(9.0f, 1.0f));
            ImGui::SameLine(0.0f, 0.0f);
            字号压入(19.0f);
            ImGui::TextColored(正文色, "%s", 标题);
            ImGui::PopFont();
            if (副标题 != nullptr)
            {
                ImGui::SameLine(0.0f, 8.0f);
                ImGui::TextColored(次要色, "%s", 副标题);
            }
            ImGui::Spacing();
        }

        //渐变进度条（自绘：底槽 + 双色填充 + 居中文本）
        void 渐变进度条(float 进度, const std::string& 文本)
        {
            const ImVec2 尺寸(ImGui::GetContentRegionAvail().x, 18.0f);
            const ImVec2 起点 = ImGui::GetCursorScreenPos();
            //占位并响应交互（仅为占据布局空间）
            ImGui::InvisibleButton("##渐变进度", 尺寸);
            ImDrawList* 列表 = ImGui::GetWindowDrawList();
            const ImVec2 终点(起点.x + 尺寸.x, 起点.y + 尺寸.y);

            //进度底槽
            列表->AddRectFilled(起点, 终点, ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.09f)), 9.0f);
            //进度填充（左深右亮的两段渐变）
            const float 比例 = std::clamp(进度, 0.0f, 1.0f);
            const float 填充宽 = 尺寸.x * 比例;
            if (填充宽 > 2.0f)
            {
                const ImVec2 填充终点(起点.x + 填充宽, 终点.y);
                列表->AddRectFilledMultiColor(起点, 填充终点,
                    ImGui::GetColorU32(副色), ImGui::GetColorU32(主色),
                    ImGui::GetColorU32(主色), ImGui::GetColorU32(副色));
            }
            //居中文本
            const ImVec2 文本尺寸 = ImGui::CalcTextSize(文本.c_str());
            列表->AddText(ImVec2(起点.x + (尺寸.x - 文本尺寸.x) * 0.5f, 起点.y + (尺寸.y - 文本尺寸.y) * 0.5f),
                ImGui::GetColorU32(ImVec4(0.97f, 0.99f, 1.0f, 1.0f)), 文本.c_str());
        }

        //状态胶囊（圆角底 + 描边 + 居中文字）
        void 状态胶囊(const char* 文本, const ImVec4& 色)
        {
            const ImVec2 文本尺寸 = ImGui::CalcTextSize(文本);
            const ImVec2 起点 = ImGui::GetCursorScreenPos();
            const ImVec2 尺寸(文本尺寸.x + 18.0f, 文本尺寸.y + 5.0f);
            ImGui::Dummy(尺寸);
            ImDrawList* 列表 = ImGui::GetWindowDrawList();
            const ImVec2 终点(起点.x + 尺寸.x, 起点.y + 尺寸.y);
            ImVec4 底色 = 色;
            底色.w = 0.18f;
            列表->AddRectFilled(起点, 终点, ImGui::GetColorU32(底色), 9.0f);
            列表->AddRect(起点, 终点, ImGui::GetColorU32(ImVec4(色.x, 色.y, 色.z, 0.55f)), 9.0f, 0, 1.0f);
            列表->AddText(ImVec2(起点.x + 9.0f, 起点.y + 2.0f), ImGui::GetColorU32(色), 文本);
        }

        //统计小块（数值 + 说明）
        void 统计小块(const char* 说明, const std::string& 数值, const ImVec4& 色)
        {
            ImGui::BeginGroup();
            字号压入(21.0f);
            ImGui::TextColored(色, "%s", 数值.c_str());
            ImGui::PopFont();
            ImGui::TextColored(次要色, "%s", 说明);
            ImGui::EndGroup();
        }

        //套件聚合统计（用于选择树的彩色状态点）
        struct 套件统计
        {
            int 通过 = 0;
            int 失败 = 0;
            int 跳过 = 0;
        };

        //关键词匹配（空关键词视为命中）
        bool 含关键词(const std::string& 文本, const std::string& 关键词)
        {
            if (关键词.empty())
                return true;
            return 文本.find(关键词) != std::string::npos;
        }

        //从当前轮失败用例构造「重跑失败」的用例清单（套件.用例…）
        std::vector<std::string> 失败用例清单生成(const std::vector<Case_Result>& 用例表)
        {
            std::vector<std::string> 清单;
            for (const Case_Result& 用例 : 用例表)
            {
                if (用例.status != Case_Status::Failed)
                    continue;
                清单.push_back(用例.suite + "." + 用例.name);
                if (static_cast<int>(清单.size()) >= 失败清单上限)
                    break;
            }
            return 清单;
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

        //绘制柔光晕（多层同心圆近似径向渐变）
        void 绘制光晕(ImDrawList* 列表, const ImVec2& 圆心, float 半径, const ImVec4& 色)
        {
            const int 层数 = 26;
            for (int 层 = 层数; 层 >= 1; --层)
            {
                const float 比例 = static_cast<float>(层) / static_cast<float>(层数);
                ImVec4 层色 = 色;
                层色.w = 0.006f + 0.028f * (1.0f - 比例);
                列表->AddCircleFilled(圆心, 半径 * 比例, ImGui::GetColorU32(层色), 64);
            }
        }

        //单个套件行绘制（按层级分组时复用）：状态点 + 套件复选框 + 展开树 + 右键菜单
        void 套件行绘制(Test_Selection_Model& 模型, std::size_t 套件序号, const std::string& 关键词,
            std::unordered_map<std::string, 套件统计>& 套件结果, std::string& 运行提示)
        {
            const std::vector<Suite_Entry>& 套件表 = 模型.suite_table_get();
            const Suite_Entry& 套件 = 套件表[套件序号];

            ImGui::PushID(static_cast<int>(套件序号));
            //套件状态点：有失败显示红色，有通过显示绿色，未运行显示灰色（未运行的套件按默认值插入）
            const 套件统计& 套件聚合 = 套件结果[套件.name];
            const ImVec4 点色 = 套件聚合.失败 > 0 ? 失败色
                : (套件聚合.通过 > 0 ? 成功色 : 次要色);
            const ImVec2 点起点 = ImGui::GetCursorScreenPos();
            const float 行高 = ImGui::GetTextLineHeight();
            ImGui::Dummy(ImVec2(11.0f, 行高));
            ImGui::GetWindowDrawList()->AddCircleFilled(
                ImVec2(点起点.x + 4.5f, 点起点.y + 行高 * 0.5f), 3.5f,
                ImGui::GetColorU32(点色), 16);
            ImGui::SameLine(0.0f, 4.0f);
            //套件级复选框
            bool 套件勾选 = 模型.suite_all_checked(套件序号);
            if (ImGui::Checkbox("##套件勾选", &套件勾选))
                模型.suite_check_set(套件序号, 套件勾选);
            ImGui::SameLine(0.0f, 6.0f);

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
            ImGui::SetNextItemOpen(模型.suite_expanded(套件序号), ImGuiCond_Always);
            const bool 展开 = ImGui::TreeNodeEx("##节点",
                ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth,
                "%s", 节点标签.c_str());
            //右键菜单：默认全部勾选时「点复选框」是取消，故提供显式的只选 / 全选 / 清空
            if (ImGui::BeginPopupContextItem("##套件菜单"))
            {
                if (ImGui::MenuItem("只选此项"))
                {
                    模型.all_clear();
                    模型.suite_check_set(套件序号, true);
                    运行提示.clear();
                }
                if (ImGui::MenuItem("全选此项"))
                {
                    模型.suite_check_set(套件序号, true);
                    运行提示.clear();
                }
                if (ImGui::MenuItem("清空此项"))
                    模型.suite_check_set(套件序号, false);
                ImGui::EndPopup();
            }
            if (展开 != 模型.suite_expanded(套件序号))
                模型.suite_expand_set(套件序号, 展开);
            if (展开)
            {
                for (std::size_t 用例序号 = 0; 用例序号 < 套件.case_list.size(); ++用例序号)
                {
                    const Case_Entry& 用例 = 套件.case_list[用例序号];
                    if (!含关键词(套件.name, 关键词) && !含关键词(用例.name, 关键词))
                        continue;
                    ImGui::PushID(static_cast<int>(用例序号));
                    bool 用例勾选 = 用例.checked;
                    if (ImGui::Checkbox(用例.name.c_str(), &用例勾选))
                        模型.case_check_set(套件序号, 用例序号, 用例勾选);
                    ImGui::PopID();
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }

        //层级分组绘制：标题行（层级复选框 + 只选此层 + 状态）+ 该层全部套件
        void 层级分组绘制(Test_Selection_Model& 模型, std::size_t 层级, const std::string& 关键词,
            std::unordered_map<std::string, 套件统计>& 套件结果,
            const Layer_Version_State* 状态, bool 隔离, std::string& 运行提示)
        {
            //空层级不显示（未识别层级的兜底分组同理）
            if (模型.layer_case_count(层级) == 0)
                return;

            ImGui::PushID(static_cast<int>(1000 + 层级));
            bool 层勾选 = 模型.layer_all_checked(层级);
            //三态观感：半选时用黄色文字提示，复选框本身仍为二态
            if (ImGui::Checkbox("##层勾选", &层勾选))
                模型.layer_check_set(层级, 层勾选);
            ImGui::SameLine(0.0f, 6.0f);
            const ImVec4 层色 = 隔离 ? 失败色
                : (模型.layer_half_checked(层级) ? 跳过色 : 正文色);
            字号压入(17.0f);
            ImGui::TextColored(层色, "%s（%zu/%zu）",
                test_layer_name(static_cast<Test_Layer>(层级)),
                模型.layer_checked_count(层级), 模型.layer_case_count(层级));
            ImGui::PopFont();
            ImGui::SameLine(0.0f, 10.0f);
            if (ImGui::SmallButton("只选此层"))
            {
                模型.layer_check_only(层级);
                运行提示.clear();
            }
            //待更新徽标与原因（仅隔离区显示）
            if (隔离 && 状态 != nullptr)
            {
                ImGui::SameLine(0.0f, 8.0f);
                状态胶囊("待更新", 失败色);
                for (const std::string& 原因 : 状态->reasons)
                    ImGui::TextColored(次要色, "    %s", 原因.c_str());
            }

            //该层全部套件
            const std::vector<Suite_Entry>& 套件表 = 模型.suite_table_get();
            for (std::size_t 套件序号 = 0; 套件序号 < 套件表.size(); ++套件序号)
            {
                const Suite_Entry& 套件 = 套件表[套件序号];
                //仅绘制属于本层的套件
                if (static_cast<std::size_t>(套件.layer) != 层级)
                    continue;
                //套件与用例都未命中关键词时隐藏
                bool 命中 = 含关键词(套件.name, 关键词);
                if (!命中)
                {
                    for (const Case_Entry& 用例 : 套件.case_list)
                    {
                        if (含关键词(用例.name, 关键词))
                        {
                            命中 = true;
                            break;
                        }
                    }
                }
                if (!命中)
                    continue;
                套件行绘制(模型, 套件序号, 关键词, 套件结果, 运行提示);
            }
            ImGui::PopID();
            ImGui::Spacing();
        }

        //绘制四角渐变背景 + 两个随时间缓慢漂移的光晕
        void 绘制渐变背景(void)
        {
            const ImGuiViewport* 视口 = ImGui::GetMainViewport();
            const ImVec2 左上 = 视口->Pos;
            const ImVec2 右下(左上.x + 视口->Size.x, 左上.y + 视口->Size.y);
            ImDrawList* 列表 = ImGui::GetBackgroundDrawList();

            //四角渐变：深海蓝 → 靛蓝 → 青绿
            列表->AddRectFilledMultiColor(左上, 右下,
                ImGui::GetColorU32(ImVec4(0.035f, 0.050f, 0.105f, 1.0f)),
                ImGui::GetColorU32(ImVec4(0.075f, 0.095f, 0.225f, 1.0f)),
                ImGui::GetColorU32(ImVec4(0.050f, 0.065f, 0.150f, 1.0f)),
                ImGui::GetColorU32(ImVec4(0.030f, 0.125f, 0.170f, 1.0f)));

            //缓慢漂移的青蓝与紫蓝光晕（增加层次，不喧宾夺主）
            const float 时间 = static_cast<float>(ImGui::GetTime());
            const ImVec2 光心一(
                左上.x + 视口->Size.x * (0.20f + 0.07f * std::sin(时间 * 0.21f)),
                左上.y + 视口->Size.y * (0.14f + 0.06f * std::cos(时间 * 0.17f)));
            const ImVec2 光心二(
                左上.x + 视口->Size.x * (0.82f + 0.05f * std::cos(时间 * 0.19f)),
                左上.y + 视口->Size.y * (0.78f + 0.06f * std::sin(时间 * 0.15f)));
            绘制光晕(列表, 光心一, 视口->Size.x * 0.42f, ImVec4(0.20f, 0.62f, 1.00f, 1.0f));
            绘制光晕(列表, 光心二, 视口->Size.x * 0.34f, ImVec4(0.48f, 0.35f, 1.00f, 1.0f));
        }
    }

    //测试操作界面绘制
    void 测试界面_绘制(Test_Runner& runner, Test_Selection_Model& model, bool& exit_requested,
        const Ui_Runtime_Stats* 运行状态)
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
        //层级版本状态（首次进入界面时读取；「重新检测」按钮可刷新）
        static std::vector<Layer_Version_State> 层级状态;
        //层级状态是否已读取
        static bool 层级状态已读 = false;
        //是否已按待更新层级做过默认隔离（整层取消勾选）
        static bool 待更新已隔离 = false;

        //背景：四角渐变 + 漂移光晕（每帧重绘）
        绘制渐变背景();

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

        //层级版本状态：首次进入（或点「重新检测」后）读取；首次读到待更新层级时默认整层取消勾选
        if (!层级状态已读)
        {
            层级状态 = layer_version_states_get();
            层级状态已读 = true;
            if (!待更新已隔离)
            {
                for (const Layer_Version_State& 状态 : 层级状态)
                {
                    if (!状态.outdated)
                        continue;
                    model.layer_check_set(static_cast<std::size_t>(状态.layer), false);
                }
                待更新已隔离 = true;
            }
        }
        //待更新层级数（顶部提示与左栏隔离区共用）
        int 待更新层数 = 0;
        for (const Layer_Version_State& 状态 : 层级状态)
        {
            if (状态.outdated)
                ++待更新层数;
        }

        //统计当前快照中的状态计数
        int 通过数 = 0;
        int 失败数 = 0;
        int 跳过数 = 0;
        //按套件聚合结果（用于选择树的实时状态点）
        std::unordered_map<std::string, 套件统计> 套件结果;
        for (const Case_Result& 用例 : 用例表)
        {
            套件统计& 统计 = 套件结果[用例.suite];
            if (用例.status == Case_Status::Passed)
            {
                ++通过数;
                ++统计.通过;
            }
            else if (用例.status == Case_Status::Failed)
            {
                ++失败数;
                ++统计.失败;
            }
            else
            {
                ++跳过数;
                ++统计.跳过;
            }
        }

        //—— 顶部：品牌 + 状态 + 进度 ——
        if (ImGui::BeginTable("##顶栏", 2, ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn("##品牌", ImGuiTableColumnFlags_WidthStretch, 0.50f);
            ImGui::TableSetupColumn("##状态", ImGuiTableColumnFlags_WidthStretch, 0.50f);
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            字号压入(28.0f);
            ImGui::TextColored(正文色, "白银纪元 · 测试控制台");
            ImGui::PopFont();
            ImGui::TextColored(次要色, "分层测试 / 多轮回归 / 实时结果");

            ImGui::TableSetColumnIndex(1);
            //运行状态胶囊
            ImGui::BeginGroup();
            状态胶囊(runner.is_running() ? "运行中" : "空闲", runner.is_running() ? 主色 : 次要色);
            ImGui::SameLine(0.0f, 10.0f);
            ImGui::TextColored(次要色, "已推进 %llu 帧", static_cast<unsigned long long>(运行状态 ? 运行状态->frames : 0u));
            ImGui::EndGroup();

            //进度条与计数
            const float 进度 = 总数 > 0 ? static_cast<float>(已完成) / static_cast<float>(总数) : 0.0f;
            const std::string 进度文本 = (总数 == 0 && 已完成 == 0)
                ? "等待运行" : (std::to_string(已完成) + " / " + std::to_string(总数));
            渐变进度条(进度, 进度文本);
            ImGui::TextColored(成功色, "通过 %d", 通过数);
            ImGui::SameLine(0.0f, 18.0f);
            ImGui::TextColored(失败色, "失败 %d", 失败数);
            ImGui::SameLine(0.0f, 18.0f);
            ImGui::TextColored(跳过色, "跳过 %d", 跳过数);
            ImGui::SameLine(0.0f, 18.0f);
            ImGui::TextColored(次要色, "本轮耗时 %lld ms",
                static_cast<long long>(轮次表.empty() ? 0 : 轮次表.back().elapsed_ms));

            ImGui::EndTable();
        }

        //—— 运行控制条 ——
        //最近一次运行请求的提示（空表示无提示）
        static std::string 运行提示;

        if (ImGui::Button("运行选中"))
        {
            //按勾选结果生成用例清单；空清单不启动（不再隐式全跑）
            if (runner.request_run(model.selected_cases()))
                运行提示.clear();
            else
                运行提示 = "未选择任何用例：请勾选套件，或右键套件选「只选此项」";
        }
        ImGui::SameLine();
        //重跑失败（无失败则禁用）
        const std::vector<std::string> 失败清单 = 失败用例清单生成(用例表);
        const bool 有失败 = !失败清单.empty();
        ImGui::BeginDisabled(!有失败);
        if (ImGui::Button("重跑失败"))
        {
            //仅重跑本轮失败用例
            if (runner.request_run(失败清单))
                运行提示.clear();
        }
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
        ImGui::SameLine();
        ImGui::TextColored(次要色, "Enter 运行 / Esc 退出");

        //空选择与启动被拒时的提示（已选非空且无提示时不占行）
        if (model.checked_case_count() == 0)
            ImGui::TextColored(失败色, "未选择任何用例：点套件复选框，或右键套件选「只选此项」");
        else if (!运行提示.empty())
            ImGui::TextColored(失败色, "%s", 运行提示.c_str());

        //层级版本提示：下层库已更新但该层测试库未重建时，常驻提醒
        if (待更新层数 > 0)
            ImGui::TextColored(跳过色,
                "提示：%d 个测试层级的下层库已更新，已在左栏隔离；重建对应子工程后点「重新检测」",
                待更新层数);

        //—— 主体：左选择树 / 中结果 / 右详情 ——
        const float 底部控制台高 = 178.0f;
        const float 页脚高 = 26.0f;
        const float 主体高 = (std::max)(ImGui::GetContentRegionAvail().y - 底部控制台高 - 页脚高 - 18.0f, 240.0f);

        ImGui::BeginChild("##主体", ImVec2(0.0f, 主体高), ImGuiChildFlags_None);
        {
            //左栏：测试范围
            卡片样式压入();
            ImGui::BeginChild("##左栏", ImVec2(336.0f, 0.0f), ImGuiChildFlags_Borders);
            {
                卡片标题("测试范围", "勾选要运行的模块");

                if (ImGui::Button("全选"))
                    model.all_check();
                ImGui::SameLine();
                if (ImGui::Button("清空"))
                    model.all_clear();
                ImGui::SameLine();
                if (ImGui::Button("反选"))
                    model.check_invert();
                ImGui::SameLine();
                if (ImGui::Button("重新检测"))
                {
                    //重读各层级构建记录，并允许再次按待更新层级做默认隔离
                    层级状态已读 = false;
                    待更新已隔离 = false;
                }
                ImGui::SetNextItemWidth(-1.0f);
                ImGui::InputTextWithHint("##搜索", "搜索模块或用例名…", 搜索缓冲, sizeof(搜索缓冲));
                ImGui::TextColored(次要色, "已选 %zu / %zu",
                    model.checked_case_count(), model.case_count());
                ImGui::TextColored(次要色, "右键套件：只选此项 / 全选 / 清空");
                ImGui::Separator();

                //可滚动的层级分组树
                ImGui::BeginChild("##选择树", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None);
                {
                    const std::string 关键词 = 搜索缓冲;

                    //待更新层级：单独置顶隔离（下层库已更新，但该层测试库尚未重建）
                    if (待更新层数 > 0)
                    {
                        ImGui::TextColored(失败色, "待更新层级（已默认隔离，重建后再跑）");
                        ImGui::TextColored(次要色, "重建步骤：cmake --build <子工程>/out/build/x64-%s",
                            current_config_name());
                        for (const Layer_Version_State& 状态 : 层级状态)
                        {
                            if (!状态.outdated)
                                continue;
                            层级分组绘制(model, static_cast<std::size_t>(状态.layer), 关键词, 套件结果,
                                &状态, true, 运行提示);
                        }
                        ImGui::Separator();
                        ImGui::TextColored(成功色, "已同步层级");
                    }

                    //已同步层级：按枚举顺序分组
                    for (int 层级 = 0; 层级 < test_layer_count; ++层级)
                    {
                        //已在上方隔离区绘制过的层级跳过
                        bool 已隔离 = false;
                        for (const Layer_Version_State& 状态 : 层级状态)
                        {
                            if (状态.outdated && static_cast<int>(状态.layer) == 层级)
                            {
                                已隔离 = true;
                                break;
                            }
                        }
                        if (已隔离)
                            continue;
                        层级分组绘制(model, static_cast<std::size_t>(层级), 关键词, 套件结果, nullptr,
                            false, 运行提示);
                    }

                    //兜底：来源未识别的用例（正常不应出现，出现时仍允许运行）
                    if (model.layer_case_count(static_cast<std::size_t>(Test_Layer::Unknown)) > 0)
                        层级分组绘制(model, static_cast<std::size_t>(Test_Layer::Unknown), 关键词,
                            套件结果, nullptr, false, 运行提示);
                }
                ImGui::EndChild();
            }
            ImGui::EndChild();
            卡片样式弹出();

            ImGui::SameLine(0.0f, 10.0f);

            //中栏：运行总览 + 结果表
            卡片样式压入();
            ImGui::BeginChild("##中栏", ImVec2(-380.0f, 0.0f), ImGuiChildFlags_Borders);
            {
                卡片标题("运行总览", "当前轮结果");
                ImGui::Checkbox("只看失败", &只看失败);
                ImGui::SameLine(0.0f, 16.0f);
                ImGui::TextColored(次要色, "结果 %d 条", static_cast<int>(用例表.size()));
                ImGui::Separator();

                if (ImGui::BeginTable("##结果表", 3,
                    ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp))
                {
                    ImGui::TableSetupColumn("状态", ImGuiTableColumnFlags_WidthFixed, 72.0f);
                    ImGui::TableSetupColumn("用例", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("耗时", ImGuiTableColumnFlags_WidthFixed, 82.0f);
                    ImGui::TableHeadersRow();
                    for (int 序号 = 0; 序号 < static_cast<int>(用例表.size()); ++序号)
                    {
                        const Case_Result& 用例 = 用例表[static_cast<std::size_t>(序号)];
                        //只看失败时过滤
                        if (只看失败 && 用例.status != Case_Status::Failed)
                            continue;
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        状态胶囊(状态文字(用例.status), 状态色(用例.status));
                        ImGui::TableSetColumnIndex(1);
                        //行选中（用于失败详情）
                        const std::string 行标签 = 用例.suite + "." + 用例.name;
                        if (ImGui::Selectable(行标签.c_str(), 选中结果 == 序号,
                            ImGuiSelectableFlags_SpanAllColumns))
                            选中结果 = 序号;
                        ImGui::TableSetColumnIndex(2);
                        ImGui::TextColored(次要色, "%lld ms", static_cast<long long>(用例.elapsed_ms));
                    }
                    ImGui::EndTable();
                }
            }
            ImGui::EndChild();
            卡片样式弹出();

            ImGui::SameLine(0.0f, 10.0f);

            //右栏：详情 / 轮次 / 日志
            卡片样式压入();
            ImGui::BeginChild("##右栏", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
            {
                卡片标题("详情与历史");

                //选中用例详情
                if (选中结果 >= 0 && 选中结果 < static_cast<int>(用例表.size()))
                {
                    const Case_Result& 选中用例 = 用例表[static_cast<std::size_t>(选中结果)];
                    状态胶囊(状态文字(选中用例.status), 状态色(选中用例.status));
                    ImGui::SameLine(0.0f, 8.0f);
                    ImGui::TextColored(正文色, "%s", 选中用例.name.c_str());
                    ImGui::TextColored(次要色, "%s · %lld ms",
                        选中用例.suite.c_str(), static_cast<long long>(选中用例.elapsed_ms));
                    if (!选中用例.failures.empty())
                    {
                        ImGui::BeginChild("##失败详情", ImVec2(0.0f, 130.0f), ImGuiChildFlags_Borders);
                        for (const std::string& 消息 : 选中用例.failures)
                            ImGui::TextColored(失败色, "%s", 消息.c_str());
                        ImGui::EndChild();
                    }
                }
                else
                {
                    ImGui::TextColored(次要色, "点击结果行查看详情");
                }

                ImGui::Separator();
                //轮次历史：每条一个堆叠比例条
                ImGui::TextColored(正文色, "轮次历史");
                if (轮次表.empty())
                    ImGui::TextColored(次要色, "尚未运行任何一轮测试");
                for (const Round_Summary& 汇总 : 轮次表)
                {
                    const ImVec2 起点 = ImGui::GetCursorScreenPos();
                    const ImVec2 尺寸(ImGui::GetContentRegionAvail().x, 18.0f);
                    ImGui::InvisibleButton(("##轮" + std::to_string(汇总.round)).c_str(), 尺寸);
                    ImDrawList* 列表 = ImGui::GetWindowDrawList();
                    const float 总 = static_cast<float>((std::max)(汇总.total, 1));
                    float 偏移 = 0.0f;
                    //通过 / 失败 / 跳过 三段
                    const int 段数 = 3;
                    const int 数量[段数] = { 汇总.passed, 汇总.failed, 汇总.skipped };
                    const ImVec4 段色[段数] = { 成功色, 失败色, 跳过色 };
                    for (int 段 = 0; 段 < 段数; ++段)
                    {
                        const float 宽 = 尺寸.x * (static_cast<float>(数量[段]) / 总);
                        if (宽 > 0.0f)
                            列表->AddRectFilled(ImVec2(起点.x + 偏移, 起点.y),
                                ImVec2(起点.x + 偏移 + 宽, 起点.y + 8.0f),
                                ImGui::GetColorU32(段色[段]), 3.0f);
                        偏移 += 宽;
                    }
                    ImGui::SetCursorScreenPos(ImVec2(起点.x, 起点.y + 9.0f));
                    ImGui::TextColored(次要色, "第 %d 轮 · 通过 %d / 失败 %d / 跳过 %d · %lld ms",
                        汇总.round, 汇总.passed, 汇总.failed, 汇总.skipped,
                        static_cast<long long>(汇总.elapsed_ms));
                }

                ImGui::Separator();
                //引擎日志（可折叠）
                if (ImGui::CollapsingHeader("引擎日志（尾部）"))
                {
                    //每帧刷新日志尾部
                    引擎日志刷新(日志行);
                    ImGui::BeginChild("##引擎日志", ImVec2(0.0f, 120.0f), ImGuiChildFlags_Borders);
                    for (const std::string& 行 : 日志行)
                        ImGui::TextColored(次要色, "%s", 行.c_str());
                    ImGui::EndChild();
                }
            }
            ImGui::EndChild();
            卡片样式弹出();
        }
        ImGui::EndChild();

        //—— 底部：gtest 输出镜像 ——
        ImGui::Spacing();
        卡片样式压入();
        ImGui::BeginChild("##控制台", ImVec2(0.0f, 底部控制台高), ImGuiChildFlags_Borders);
        {
            卡片标题("gtest 输出镜像", "与控制台同风格");
            ImGui::Checkbox("自动滚动", &自动滚动);
            ImGui::SameLine(0.0f, 16.0f);
            ImGui::TextColored(次要色, "%d 行", static_cast<int>(输出表.size()));
            ImGui::SameLine(0.0f);
            ImGui::BeginChild("##输出镜像", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None);
            {
                for (const Output_Line& 行 : 输出表)
                {
                    ImVec4 颜色(0.86f, 0.92f, 1.0f, 1.0f);
                    if (行.level == 3)
                        颜色 = 失败色;
                    else if (行.level == 2)
                        颜色 = 成功色;
                    else if (行.level == 4)
                        颜色 = 跳过色;
                    else if (行.level == 0)
                        颜色 = 次要色;
                    ImGui::TextColored(颜色, "%s", 行.text.c_str());
                }
                if (自动滚动)
                    ImGui::SetScrollHereY(1.0f);
            }
            ImGui::EndChild();
        }
        ImGui::EndChild();
        卡片样式弹出();

        //—— 页脚：渲染器帧统计 ——
        if (运行状态 != nullptr)
        {
            ImGui::TextColored(次要色,
                "帧 %llu · 主机 %.2f ms · GPU %.2f ms（%s） · 绘制 %llu/%llu · 实例 %llu · 后处理 %u 级",
                static_cast<unsigned long long>(运行状态->frames),
                运行状态->frame_ms,
                运行状态->gpu_ms,
                运行状态->gpu_supported ? "支持" : "不支持",
                static_cast<unsigned long long>(运行状态->drawn_items),
                static_cast<unsigned long long>(运行状态->draw_items),
                static_cast<unsigned long long>(运行状态->instances),
                运行状态->post_stages);
        }

        //—— 键盘快捷键 ——
        ImGuiIO& io = ImGui::GetIO();
        if (!io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Enter))
        {
            //回车上等同于「运行选中」：空选择同样拒绝启动
            if (runner.request_run(model.selected_cases()))
                运行提示.clear();
            else
                运行提示 = "未选择任何用例：请勾选套件，或右键套件选「只选此项」";
        }
        //退出键只在按下沿生效（repeat=false），按住 Esc 不会逐帧退出
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            exit_requested = true;
    }
}
