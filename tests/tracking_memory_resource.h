#pragma once

#include <memory_resource>

namespace clu::testing
{
    class tracking_memory_resource final : public std::pmr::memory_resource
    {
    public:
        explicit tracking_memory_resource(memory_resource* base = std::pmr::get_default_resource()) noexcept:
            base_(base)
        {
        }

        std::size_t bytes_allocated() const noexcept { return allocated_; }

    protected:
        void* do_allocate(const std::size_t bytes, const std::size_t align) override
        {
            void* ptr = base_->allocate(bytes, align);
            allocated_ += bytes;
            return ptr;
        }

        void do_deallocate(void* ptr, const std::size_t bytes, const std::size_t align) override
        {
            base_->deallocate(ptr, bytes, align);
            allocated_ -= bytes;
        }

        bool do_is_equal(const memory_resource& other) const noexcept override { return &other == this; }

    private:
        memory_resource* base_;
        std::size_t allocated_ = 0;
    };
} // namespace clu::testing
