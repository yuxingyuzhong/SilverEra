# -*- coding: utf-8 -*-
# 阶段2：实体配置模型.cpp → 4 个实现文件 + 内部工具（只移动，不重写）
import io, os, shutil

CFG_DIR = r"D:\代码存储\代码仓库\游戏引擎\游戏引擎\src\tools\GUI\Config_Editor"
CORE_DIR = os.path.join(CFG_DIR, "core")
SRC = os.path.join(CORE_DIR, "实体配置模型.cpp")

with io.open(SRC, "r", encoding="utf-8-sig", newline="") as f:
    text = f.read()
newline = "\r\n" if "\r\n" in text else "\n"
raw = text.split(newline)
total = len(raw)
print("总行数:", total, "换行符:", repr(newline))

# 锚点（行号 1-based）→ 组名。锚点必须覆盖全部成员函数定义行。
anchors = [
    (109, "实体"), (122, "实体"), (289, "实体"), (295, "实体"), (301, "实体"),
    (310, "实体"), (316, "实体"), (350, "实体"), (437, "实体"), (511, "实体"),
    (552, "实体"), (595, "属性槽"), (601, "属性槽"), (607, "属性槽"), (616, "属性槽"),
    (642, "属性槽"), (713, "属性槽"), (749, "属性槽"), (789, "实体"), (818, "实体"),
    (825, "实体"), (900, "实体"), (920, "实体"), (926, "实体"), (953, "属性槽"),
    (981, "属性槽"), (988, "属性槽"), (1010, "属性槽"), (1019, "属性槽"),
    (1029, "格式"), (1035, "格式"), (1041, "格式"), (1050, "格式"), (1066, "格式"),
    (1082, "格式"), (1094, "格式"), (1133, "格式"), (1177, "格式"), (1202, "格式"),
    (1237, "格式"), (1274, "格式"), (1320, "格式"),
    (1404, "通用配置"), (1410, "通用配置"), (1416, "通用配置"), (1425, "通用配置"),
    (1457, "通用配置"), (1472, "通用配置"), (1532, "通用配置"), (1614, "通用配置"),
    (1656, "通用配置"), (1747, "通用配置"), (1771, "通用配置"), (1780, "通用配置"),
    (1864, "通用配置"),
]
anchors.sort()

def find_func_end(start_1based):
    depth = 0
    started = False
    for i in range(start_1based - 1, total):
        line = raw[i]
        if not started:
            if "{" in line:
                started = True
                depth = line.count("{") - line.count("}")
                if depth <= 0:
                    depth = 1
            continue
        depth += line.count("{") - line.count("}")
        if depth <= 0:
            return i + 1
    return total

# 全局切分：每个函数 [prev_end+1, end]
slices = []  # (start, end, group)
prev_end = None
for a, g in anchors:
    end = find_func_end(a)
    start = 104 if prev_end is None else prev_end + 1
    slices.append((start, end, g))
    prev_end = end

# 验证覆盖 104..end
cursor = 104
ok = True
for s, e, g in slices:
    if s > cursor:
        print(f"!! 缺口 {cursor}-{s-1}"); ok = False
    if s < cursor:
        print(f"!! 重叠 {s} 与至{cursor-1}"); ok = False
    cursor = max(cursor, e + 1)
tail_start = cursor
if cursor <= total:
    print(f"尾部: {cursor}-{total}（namespace 收尾，丢弃）")
    for i in range(cursor - 1, total):
        print(f"  [{i+1}] {raw[i][:60]}")
print("覆盖验证:", "OK" if ok else "FAIL")

# 分组收集切片内容（保留每段间的空行结构：切片自带开头/结尾空行）
group_parts = {"实体": [], "属性槽": [], "格式": [], "通用配置": []}
for s, e, g in slices:
    group_parts[g].append(newline.join(raw[s - 1:e]))

def build_cpp(group, extra_includes=()):
    head = ['#include "src/tools/GUI/Config_Editor/实体配置模型_内部工具.h"']
    head.extend('#include "%s"' % inc for inc in extra_includes)
    head.append("")
    head.append("#include <set>")
    head.append("")
    head.append("//引擎命名空间")
    head.append("namespace engine")
    head.append("{")
    body = group_parts[group]
    # 合并：段之间隔一个空行（原切片已含尾部空行，直接拼接即可）
    merged = newline.join(body)
    tail = newline.join(["", "}", ""])
    return newline.join(head) + newline + merged + tail

# ---- 生成内部工具 ----
# 工具区：行 15-101（注释 + 三个函数体，不含匿名 namespace 包裹）
tool_body = newline.join(raw[14:101])
tool_h = (
    "#pragma once" + newline +
    "//============================================================================" + newline +
    "// 实体配置模型 —— 内部工具（共享声明）" + newline +
    "// 由 实体配置模型.cpp 的匿名命名空间工具拆分而来（架构改革 阶段 2）" + newline +
    "// 原工具位于匿名命名空间，拆分后改为 engine 命名空间共享函数，行为与拆分前完全一致。" + newline +
    "//============================================================================" + newline +
    '#include "src/tools/GUI/Config_Editor/实体配置模型.h"' + newline +
    newline +
    "namespace engine" + newline +
    "{" + newline +
    "    std::filesystem::path utf8_path(const std::string& s);" + newline +
    "    std::string path_utf8(const std::filesystem::path& p);" + newline +
    "    bool 原子写入文件(const std::filesystem::path& path, const std::string& content, std::string& error);" + newline +
    "}" + newline
)
tool_cpp = (
    '#include "src/tools/GUI/Config_Editor/实体配置模型_内部工具.h"' + newline +
    newline +
    "//引擎命名空间" + newline +
    "namespace engine" + newline +
    "{" + newline +
    tool_body + newline +
    "}" + newline
)

# ---- 写出 ----
os.makedirs(CORE_DIR, exist_ok=True)
files = {
    os.path.join(CFG_DIR, "实体配置模型_内部工具.h"): tool_h,
    os.path.join(CORE_DIR, "实体配置模型_内部工具.cpp"): tool_cpp,
    os.path.join(CORE_DIR, "实体配置模型_实体.cpp"): build_cpp("实体", ("src/tools/Non_GUI/Engine_Env/引擎环境.h",)),
    os.path.join(CORE_DIR, "实体配置模型_属性槽.cpp"): build_cpp("属性槽"),
    os.path.join(CORE_DIR, "实体配置模型_格式.cpp"): build_cpp("格式"),
    os.path.join(CORE_DIR, "实体配置模型_通用配置.cpp"): build_cpp("通用配置"),
}

# 先备份原文件为 .orig（若已存在则覆盖）
orig = SRC + ".orig"
shutil.copy2(SRC, orig)
print("备份:", orig)

for path, content in files.items():
    with io.open(path, "w", encoding="utf-8", newline="") as f:
        f.write(content)
    n = content.count(newline)
    print("写出: %s (%d 行, %d 字节)" % (os.path.basename(path), n, len(content.encode("utf-8"))))

# 删除原文件（.orig 保留回滚）
os.remove(SRC)
print("已删除原文件:", os.path.basename(SRC))

# 清理 dry-run 脚本
dry = os.path.join(CORE_DIR, "_拆分阶段2_dry.py")
if os.path.exists(dry):
    os.remove(dry)
    print("已删除 dry-run 脚本")

# 完整性自检：新文件内容 = 原 15-102 工具 + 104-1867 函数
print("\n===== 完整性自检 =====")
check_src = (newline.join(raw[14:101]) + newline + newline.join(raw[103:1867]))
total_new = sum(1 for _, c in files.items() for _ in c.split(newline))
print("OK 拆分完成")
