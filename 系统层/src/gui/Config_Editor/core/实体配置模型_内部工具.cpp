#include "gui/Config_Editor/实体配置模型_内部工具.h"

//引擎命名空间
namespace engine
{
        //std::string (UTF-8) → std::filesystem::path
        std::filesystem::path utf8_path(const std::string& s)
        {
            std::u8string u8s;
            u8s.reserve(s.size());
            for (char c : s)
                u8s.push_back(static_cast<char8_t>(c));
            return std::filesystem::path(u8s);
        }

        //std::filesystem::path → std::string (UTF-8)
        std::string path_utf8(const std::filesystem::path& p)
        {
            std::u8string u8s = p.u8string();
            std::string out;
            out.reserve(u8s.size());
            for (char8_t c : u8s)
                out.push_back(static_cast<char>(c));
            return out;
        }

        //原子写入文件：写临时文件 → 校验流状态 → 备份原文件为 .bak → rename 替换
        //成功返回 true；失败返回 false 并把原因写入 error（目标文件始终保持完整，不产生半截 JSON）
        bool 原子写入文件(const std::filesystem::path& path, const std::string& content, std::string& error)
        {
            try
            {
                std::filesystem::create_directories(path.parent_path());

                //1. 写临时文件（同一目录，保证 rename 原子性）
                std::filesystem::path tmp = path;
                tmp += ".tmp";
                {
                    std::ofstream file(tmp, std::ios::out | std::ios::trunc | std::ios::binary);
                    if (!file.is_open())
                    {
                        error = "临时文件打开失败：" + path_utf8(tmp);
                        return false;
                    }
                    file << content;
                    file.flush();
                    //关键：检查流状态（磁盘满/写入失败时 ofstream 不抛异常，必须显式检查）
                    if (!file)
                    {
                        error = "写入临时文件失败：" + path_utf8(tmp);
                        file.close();
                        std::error_code ec;
                        std::filesystem::remove(tmp, ec);
                        return false;
                    }
                }

                //2. 备份原文件（存在时复制为 .bak）
                if (std::filesystem::exists(path))
                {
                    std::filesystem::path bak = path;
                    bak += ".bak";
                    std::error_code ec;
                    std::filesystem::copy_file(path, bak,
                        std::filesystem::copy_options::overwrite_existing, ec);
                    if (ec)
                    {
                        error = "备份原文件失败：" + path_utf8(bak) + "（" + ec.message() + "）";
                        std::error_code ec2;
                        std::filesystem::remove(tmp, ec2);
                        return false;
                    }
                }

                //3. rename 替换（MSVC 实现使用 MoveFileEx(REPLACE_EXISTING)，可覆盖已存在文件）
                std::error_code ec;
                std::filesystem::rename(tmp, path, ec);
                if (ec)
                {
                    error = "替换文件失败：" + path_utf8(path) + "（" + ec.message() + "）";
                    std::error_code ec2;
                    std::filesystem::remove(tmp, ec2);
                    return false;
                }
                return true;
            }
            catch (const std::exception& e)
            {
                error = std::string("写入异常：") + e.what();
                return false;
            }
        }
}
