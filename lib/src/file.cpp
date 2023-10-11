#include "clu/file.h"

#include <algorithm>
#include <sstream>

namespace clu
{
    std::size_t read_bytes(const std::filesystem::path& path, const mutable_buffer bytes)
    {
        std::ifstream fs(path, std::ios::in | std::ios::binary);
        if (fs.fail())
            throw std::runtime_error("failed to open binary file");
        fs.ignore(std::numeric_limits<std::streamsize>::max());
        const auto size = static_cast<std::size_t>(fs.gcount());
        const auto read_size = std::min(size, bytes.size());
        fs.seekg(0, std::ios::beg);
        fs.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(read_size));
        if (fs.fail() && !fs.eof())
            throw std::runtime_error("failed to read binary file");
        return read_size;
    }

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
