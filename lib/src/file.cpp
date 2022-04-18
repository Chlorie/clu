#include "clu/file.h"

namespace clu
{
    std::string read_all_text(const std::filesystem::path& path)
    {
        std::ifstream fs(path);
        if (fs.fail())
            throw std::runtime_error("failed to open text file");
        std::stringstream buf;
        buf << fs.rdbuf();
        return std::move(buf).str();
    }

    void write_all_bytes(const std::filesystem::path& path, const const_buffer bytes)
    {
        std::ofstream fs(path, std::ios::out | std::ios::binary);
        if (fs.fail())
            throw std::runtime_error("failed to open binary file");
        fs.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }

    void write_all_text(const std::filesystem::path& path, const std::string_view text)
    {
        std::ofstream fs(path);
        if (fs.fail())
            throw std::runtime_error("failed to open text file");
        fs << text;
    }
} // namespace clu
