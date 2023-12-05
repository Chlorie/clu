#pragma once

#include <cstdio>

namespace clu
{
    /// A verbose struct for debugging lifetime management issues
    struct verbose
    {
        inline static size_t counter = 0;
        size_t id = 0;

        verbose(): id(counter++) { std::printf("Default c'tor [%zu]\n", id); }
        ~verbose() noexcept { std::printf("D'tor [%zu]\n", id); }
        verbose(const verbose& other): id(counter++) { std::printf("Copy c'tor [%zu <- %zu]\n", id, other.id); }
        verbose(verbose&& other) noexcept: id(counter++) { std::printf("Move c'tor [%zu <- %zu]\n", id, other.id); }
        verbose& operator=(const verbose& other)
        {
            std::printf("Copy = [%zu <- %zu]\n", id, other.id);
            return *this;
        }
        verbose& operator=(verbose&& other) noexcept
        {
            std::printf("Move = [%zu <- %zu]\n", id, other.id);
            return *this;
        }
    };
} // namespace clu
