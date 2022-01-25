#pragma once

#if __has_include(<coroutine>)
#   include <coroutine>
#   define CLU_CORO_NS ::std
#elif __has_include(<experimental/coroutine>)
#   include <experimental/coroutine>
#   define CLU_CORO_NS ::std::experimental
#else
#   error Cannot find coroutine library header
#endif

namespace clu
{
	namespace coro = CLU_CORO_NS;
}

#undef CLU_CORO_NS
