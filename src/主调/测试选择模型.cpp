//测试选择模型实现：googletest 反射建树、勾选态维护、过滤串生成与控制台输入解析
#include "Application/Test/src/主调/测试选择模型.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

//文件内部工具
namespace
{
    //把纯数字字符串解析为长整数（仅接受十进制数字，超长数字判非法）
    bool 数字解析(const std::string& 文本, long& 结果)
    {
        //空串非法
        if (文本.empty())
            return false;

        long 值 = 0;
        for (char 字符 : 文本)
        {
            //出现非数字字符即非法
            if (字符 < '0' || 字符 > '9')
                return false;
            值 = 值 * 10 + (字符 - '0');
            //防御：套件编号不可能达到该量级，直接判非法
            if (值 > 1000000)
                return false;
        }
        结果 = 值;
        return true;
    }
}

//引擎命名空间
namespace engine
{
    //========================================================================
    // 建立套件树
    //========================================================================
    void Test_Selection_Model::suite_tree_build()
    {
        suite_table_.clear();

        //取 googletest 单例（用例在静态初始化期已完成注册）
        ::testing::UnitTest* 单元测试 = ::testing::UnitTest::GetInstance();
        const int 套件总数 = 单元测试->total_test_suite_count();

        for (int 套件序号 = 0; 套件序号 < 套件总数; ++套件序号)
        {
            const ::testing::TestSuite* 套件 = 单元测试->GetTestSuite(套件序号);
            //防御：反射返回空指针时跳过
            if (套件 == nullptr)
                continue;

            Suite_Entry 条目;
            条目.name = 套件->name();

            //逐条登记用例，默认全部勾选
            const int 用例总数 = 套件->total_test_count();
            for (int 用例序号 = 0; 用例序号 < 用例总数; ++用例序号)
            {
                const ::testing::TestInfo* 用例 = 套件->GetTestInfo(用例序号);
                if (用例 == nullptr)
                    continue;

                Case_Entry 用例记录;
                用例记录.name = 用例->name();
                用例记录.checked = true;
                条目.case_list.push_back(用例记录);
            }

            suite_table_.push_back(条目);
        }
    }

    //========================================================================
    // 查询
    //========================================================================
    const std::vector<Suite_Entry>& Test_Selection_Model::suite_table_get() const
    {
        return suite_table_;
    }

    std::size_t Test_Selection_Model::suite_count() const
    {
        return suite_table_.size();
    }

    std::size_t Test_Selection_Model::case_count() const
    {
        std::size_t 总数 = 0;
        for (const Suite_Entry& 套件 : suite_table_)
            总数 += 套件.case_list.size();
        return 总数;
    }

    std::size_t Test_Selection_Model::checked_case_count() const
    {
        std::size_t 总数 = 0;
        for (const Suite_Entry& 套件 : suite_table_)
        {
            for (const Case_Entry& 用例 : 套件.case_list)
            {
                if (用例.checked)
                    ++总数;
            }
        }
        return 总数;
    }

    bool Test_Selection_Model::suite_all_checked(std::size_t 套件序号) const
    {
        //越界或空套件一律视为未全选
        if (套件序号 >= suite_table_.size() || suite_table_[套件序号].case_list.empty())
            return false;

        for (const Case_Entry& 用例 : suite_table_[套件序号].case_list)
        {
            if (!用例.checked)
                return false;
        }
        return true;
    }

    bool Test_Selection_Model::suite_half_checked(std::size_t 套件序号) const
    {
        //越界或空套件不存在半选态
        if (套件序号 >= suite_table_.size() || suite_table_[套件序号].case_list.empty())
            return false;

        //统计本套件已勾选数
        std::size_t 已选 = 0;
        for (const Case_Entry& 用例 : suite_table_[套件序号].case_list)
        {
            if (用例.checked)
                ++已选;
        }
        //部分勾选即为半选
        return 已选 > 0 && 已选 < suite_table_[套件序号].case_list.size();
    }

    bool Test_Selection_Model::suite_expanded(std::size_t 套件序号) const
    {
        //越界视为未展开
        if (套件序号 >= suite_table_.size())
            return false;
        return suite_table_[套件序号].expanded;
    }

    //========================================================================
    // 修改
    //========================================================================
    void Test_Selection_Model::suite_check_set(std::size_t 套件序号, bool 勾选)
    {
        //越界忽略
        if (套件序号 >= suite_table_.size())
            return;

        for (Case_Entry& 用例 : suite_table_[套件序号].case_list)
            用例.checked = 勾选;
    }

    void Test_Selection_Model::case_check_set(std::size_t 套件序号, std::size_t 用例序号, bool 勾选)
    {
        //越界忽略
        if (套件序号 >= suite_table_.size())
            return;
        if (用例序号 >= suite_table_[套件序号].case_list.size())
            return;

        suite_table_[套件序号].case_list[用例序号].checked = 勾选;
    }

    void Test_Selection_Model::suite_expand_set(std::size_t 套件序号, bool 展开)
    {
        //越界忽略
        if (套件序号 >= suite_table_.size())
            return;

        suite_table_[套件序号].expanded = 展开;
    }

    void Test_Selection_Model::all_check()
    {
        for (Suite_Entry& 套件 : suite_table_)
        {
            for (Case_Entry& 用例 : 套件.case_list)
                用例.checked = true;
        }
    }

    void Test_Selection_Model::all_clear()
    {
        for (Suite_Entry& 套件 : suite_table_)
        {
            for (Case_Entry& 用例 : 套件.case_list)
                用例.checked = false;
        }
    }

    void Test_Selection_Model::check_invert()
    {
        for (Suite_Entry& 套件 : suite_table_)
        {
            for (Case_Entry& 用例 : 套件.case_list)
                用例.checked = !用例.checked;
        }
    }

    //========================================================================
    // 生成过滤串
    //========================================================================
    std::string Test_Selection_Model::filter_string_build() const
    {
        //全不选视为全跑（与 gtest 的 "*" 语义一致）
        if (checked_case_count() == 0)
            return "*";

        std::vector<std::string> 片段;
        for (const Suite_Entry& 套件 : suite_table_)
        {
            //统计本套件的勾选数
            std::size_t 已选 = 0;
            for (const Case_Entry& 用例 : 套件.case_list)
            {
                if (用例.checked)
                    ++已选;
            }

            //整组未勾选：跳过
            if (已选 == 0)
                continue;

            //整组全选：压缩成 套件.*
            if (已选 == 套件.case_list.size())
            {
                片段.push_back(套件.name + ".*");
                continue;
            }

            //部分勾选：逐条列出
            for (const Case_Entry& 用例 : 套件.case_list)
            {
                if (用例.checked)
                    片段.push_back(套件.name + "." + 用例.name);
            }
        }

        //用 ':' 连接各片段
        std::string 过滤串;
        for (std::size_t i = 0; i < 片段.size(); ++i)
        {
            if (i != 0)
                过滤串 += ":";
            过滤串 += 片段[i];
        }
        return 过滤串;
    }

    //========================================================================
    // 控制台输入解析
    //========================================================================
    bool 解析控制台选择(const std::string& 输入, std::size_t 套件数, std::vector<bool>& 勾选表)
    {
        勾选表.assign(套件数, false);

        //去掉首尾空白，便于接受带空格的输入
        const std::size_t 首 = 输入.find_first_not_of(" \t\r\n");
        if (首 == std::string::npos)
            return false;
        const std::size_t 尾 = 输入.find_last_not_of(" \t\r\n");
        const std::string 文本 = 输入.substr(首, 尾 - 首 + 1);

        //all / a / * ：全选
        if (文本 == "all" || 文本 == "ALL" || 文本 == "a" || 文本 == "*")
        {
            勾选表.assign(套件数, true);
            return true;
        }

        //按 ',' 切段，逐段解析编号或区间
        std::size_t 起点 = 0;
        while (true)
        {
            const std::size_t 逗号 = 文本.find(',', 起点);
            const std::size_t 段尾 = (逗号 == std::string::npos) ? 文本.size() : 逗号;
            const std::string 段 = 文本.substr(起点, 段尾 - 起点);

            //空段非法（如 "1,,3" 或 "1,"）
            if (段.empty())
                return false;

            const std::size_t 横线 = 段.find('-');
            if (横线 != std::string::npos)
            {
                //区间形式 a-b
                const std::string 左文本 = 段.substr(0, 横线);
                const std::string 右文本 = 段.substr(横线 + 1);
                long 左值 = 0;
                long 右值 = 0;

                //区间两侧必须都是数字，且不得再出现横线
                if (左文本.empty() || 右文本.empty())
                    return false;
                if (右文本.find('-') != std::string::npos)
                    return false;
                if (!数字解析(左文本, 左值) || !数字解析(右文本, 右值))
                    return false;

                //编号从 1 开始，且不得越过套件总数
                if (左值 < 1 || 右值 < 左值 || static_cast<std::size_t>(右值) > 套件数)
                    return false;

                for (long 编号 = 左值; 编号 <= 右值; ++编号)
                    勾选表[static_cast<std::size_t>(编号) - 1] = true;
            }
            else
            {
                //单编号形式
                long 编号 = 0;
                if (!数字解析(段, 编号))
                    return false;
                if (编号 < 1 || static_cast<std::size_t>(编号) > 套件数)
                    return false;

                勾选表[static_cast<std::size_t>(编号) - 1] = true;
            }

            //已到末段
            if (逗号 == std::string::npos)
                break;
            起点 = 逗号 + 1;
        }

        return true;
    }
}
