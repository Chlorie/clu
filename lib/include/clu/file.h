#pragma once

#include <vector>
#include <filesystem>
#include <fstream>
#include <string>

#include "buffer.h"

namespace clu
{
    /**
     * \brief Read contents of a binary file into a `std::vector`.
     * \tparam T Value type of the result vector. It must be trivially copyable.
     * \param path The path to the file to read from.
     */
    template <typename T = std::byte>
        requires trivially_copyable<T>
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

    /**
     * \brief Read contents of a binary file into a `mutable_buffer`.
     * If the file is larger than the buffer, it is read until the buffer is full.
     * \param path The path to the file to read from.
     * \param bytes The buffer to write into.
     * \return Number of bytes written into the buffer.
     */
    std::size_t read_bytes(const std::filesystem::path& path, mutable_buffer bytes);

    /**
     * \brief Reads contents of a text file into a `std::string`.
     * \param path The path to the file to read from.
     */
    [[nodiscard]] std::string read_all_text(const std::filesystem::path& path);

    /**
     * \brief Writes bytes in a given buffer into a file. The file will be overwritten.
     * \param path The path to the file to write into.
     * \param bytes The byte buffer.
     */
    void write_all_bytes(const std::filesystem::path& path, const_buffer bytes);

    /**
     * \brief Writes given text into a file. The file will be overwritten.
     * \param path The path to the file to write into.
     * \param text The text to write.
     */
    void write_all_text(const std::filesystem::path& path, std::string_view text);
} // namespace clu
