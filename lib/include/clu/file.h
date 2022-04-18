#pragma once

#include <vector>
#include <filesystem>
#include <fstream>
#include <string>

#include "export.h"
#include "buffer.h"

namespace clu
{
    /// Read contents of a binary file into a `std::vector`.
    ///
    /// \tparam T Value type of the result vector, must be a trivial type.
    /// \param path Path to the file to read.
    /// \returns `std::vector` with the file content.
    template <typename T = std::byte>
        requires std::is_trivial_v<T>
    [[nodiscard]] std::vector<T> read_all_bytes(const std::filesystem::path& path)
    {
        std::ifstream fs(path, std::ios::in | std::ios::binary);
        if (fs.fail())
            throw std::runtime_error("failed to open binary file");
        fs.ignore(std::numeric_limits<std::streamsize>::max());
        const auto size = static_cast<std::size_t>(fs.gcount());
        const auto vec_size = size / sizeof(T) + (size % sizeof(T) != 0);
        std::vector<T> buffer(vec_size);
        fs.seekg(0, std::ios::beg);
        fs.read(reinterpret_cast<char*>(buffer.data()), size);
        if (fs.fail() && !fs.eof())
            throw std::runtime_error("failed to read binary file");
        return buffer;
    }

    [[nodiscard]] CLU_API std::string read_all_text(const std::filesystem::path& path);
    CLU_API void write_all_bytes(const std::filesystem::path& path, const_buffer bytes);
    CLU_API void write_all_text(const std::filesystem::path& path, std::string_view text);
} // namespace clu
