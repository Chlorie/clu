#include "clu/async/async_scope.h"

namespace clu::detail::async_scp
{
    stop_token_env recv_base::get_env() const noexcept { return stop_token_env(scope_->get_stop_token()); }

    void recv_base::decrease_counter(async_scope* scope) noexcept
    {
        if (scope->count_.fetch_sub(1, std::memory_order::relaxed) == 1)
            scope->ev_.set();
    }
} // namespace clu::detail::async_scp
