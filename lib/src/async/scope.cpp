#include "clu/async/scope.h"

namespace clu::async::detail::scp
{
    void recv_base::decrease_counter(scope* scope) noexcept
    {
        if (scope->count_.fetch_sub(1, std::memory_order::relaxed) == 1)
            scope->ev_.set();
    }
} // namespace clu::async::detail::scp
