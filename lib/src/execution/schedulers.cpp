#include "clu/execution/schedulers.h"

namespace clu::exec::detail::trmp_schd
{
    namespace
    {
        thread_local struct trampoline_state* current = nullptr;

        struct trampoline_state
        {
            std::size_t depth = 1;
            ops_base_t* top = nullptr;

            trampoline_state() noexcept { current = this; }
            ~trampoline_state() noexcept { current = nullptr; }
            CLU_IMMOVABLE_TYPE(trampoline_state);

            void deplete() noexcept
            {
                while (top)
                {
                    depth = 1;
                    ops_base_t* popped = top;
                    top = top->next();
                    popped->execute();
                }
            }
        };
    } // namespace

    void ops_base_t::add_operation() noexcept
    {
        if (!current)
        {
            trampoline_state state;
            execute();
            state.deplete();
        }
        else if (current->depth < depth_)
        {
            current->depth++;
            execute();
        }
        else // recursed too deeply
        {
            next_ = current->top;
            current->top = this;
        }
    }
} // namespace clu::exec::detail::trmp_schd
