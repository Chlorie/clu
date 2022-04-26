#pragma once

#include <memory>

#include "../execution/context.h"

namespace clu
{
    namespace detail::static_tp
    {
        class CLU_API pool
        {
        public:
            explicit pool(std::size_t size);
            pool(const pool&) = delete;
            pool(pool&&) = delete;
            pool& operator=(const pool&) = delete;
            pool& operator=(pool&&) = delete;

            ~pool() noexcept;
            void finish();
            auto get_scheduler() noexcept { return exec::context_scheduler<pool>(this); }

        private:
            using ops_base = exec::context_operation_state_base<pool>;
            class thread_res;

            std::size_t size_ = 0;
            thread_res* res_ = nullptr;
            std::atomic_size_t index_ = 0;

            friend void tag_invoke(exec::add_operation_t, pool& self, ops_base& task) { self.enqueue(&task); }
            void enqueue(ops_base* task);
            void work(std::size_t index);
        };
    } // namespace detail::static_tp

    using static_thread_pool = detail::static_tp::pool;
} // namespace clu
