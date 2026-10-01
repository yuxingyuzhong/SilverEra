#pragma once
//============================================================================
// Test_Selection_Model —— 运行时测试选择（纯逻辑）
// ---------------------------------------------------------------------------
// 位置：白银纪元/测试层/src/主调/测试选择模型.h
// 职责：
//   1. 通过 googletest 反射枚举当前已注册的套件与用例，建立两层选择树；
//   2. 维护套件 / 用例的勾选态（含「半选」判定）；
//   3. 按勾选结果生成 --gtest_filter 过滤串。
// 本文件不依赖任何图形库：图形选择窗口与控制台选择菜单共用同一模型，
// 单元测试也可直接构造模型验证过滤串生成逻辑。
//============================================================================
#include <cstddef>
#include <string>
#include <vector>

//引擎命名空间
namespace engine
{
    //单个用例的选择条目
    struct Case_Entry
    {
        std::string name;    //用例名（不含套件前缀）
        bool checked = true;  //用例是否被勾选
    };

    //单个套件的选择条目
    struct Suite_Entry
    {
        std::string name;              //套件名
        std::vector<Case_Entry> case_list;  //套件下的用例
        bool expanded = false;           //界面上是否已展开用例列表
    };

    //测试选择模型
    class Test_Selection_Model
    {
    public:
        //从 googletest 反射建立套件树（全部用例默认勾选）
        void suite_tree_build();

        // —— 查询 ——

        //套件表（只读）
        const std::vector<Suite_Entry>& suite_table_get() const;
        //套件数量
        std::size_t suite_count() const;
        //用例总数
        std::size_t case_count() const;
        //已勾选用例数
        std::size_t checked_case_count() const;
        //套件是否全选
        bool suite_all_checked(std::size_t 套件序号) const;
        //套件是否半选（部分勾选）
        bool suite_half_checked(std::size_t 套件序号) const;
        //套件是否已展开
        bool suite_expanded(std::size_t 套件序号) const;

        // —— 修改 ——

        //设置套件勾选（整组全选或全不选）
        void suite_check_set(std::size_t 套件序号, bool 勾选);
        //设置单个用例勾选
        void case_check_set(std::size_t 套件序号, std::size_t 用例序号, bool 勾选);
        //设置套件展开态
        void suite_expand_set(std::size_t 套件序号, bool 展开);
        //全部勾选
        void all_check();
        //全部清空
        void all_clear();
        //反向勾选
        void check_invert();

        // —— 输出 ——

        //生成 --gtest_filter 过滤串：
        //  全不选视为「全部」，返回 "*"；
        //  整套件全选压缩为 "套件.*"，部分勾选逐条列为 "套件.用例"；
        //  多条之间用 ':' 连接。
        std::string filter_string_build() const;

    private:
        std::vector<Suite_Entry> suite_table_;   //套件树
    };

    //解析控制台选择输入：接受 "all" 或 "1,3-5" 形式（编号从 1 开始），
    //结果写入 勾选表（长度等于 套件数）；返回 false 表示输入非法。
    bool 解析控制台选择(const std::string& 输入, std::size_t 套件数, std::vector<bool>& 勾选表);
}
