#pragma once

#include <vector>
#include <filesystem>
#include <fstream>

namespace clu
{
    template <typename T> requires std::is_trivial_v<T>
    std::vector<T> read_all_bytes(const std::filesystem::path& path)
    {
        std::ifstream fs(path, std::ios::in | std::ios::binary);
        if (fs.fail()) throw std::runtime_error("failed to open binary file");
        fs.ignore(std::numeric_limits<std::streamsize>::max());
        const std::streamsize size = fs.gcount();
        const size_t vec_size = static_cast<size_t>(size / sizeof(T) + (size % sizeof(T) != 0));
        std::vector<T> buffer(vec_size);
        fs.seekg(0, std::ios::beg);
        fs.read(reinterpret_cast<char*>(buffer.data()), size);
        if (fs.fail() && !fs.eof()) throw std::runtime_error("failed to read binary file");
        return buffer;
    }
}
