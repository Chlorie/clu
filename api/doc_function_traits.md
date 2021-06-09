# Header file `function_traits.h`

{% raw %}
``` cpp
#include "type_traits.h"

#include "meta_list.h"

namespace clu
{
    template <typename F>
    struct function_traits;

    template <typename R, typename ... Ts>
    struct function_traits<R(Ts...)>;

    template <typename R, typename ... Ts>
    struct function_traits<R(Ts..., ...)>;

    template <typename R, typename ... Ts>
    struct function_traits<R(Ts...)const>;

    template <typename R, typename ... Ts>
    struct function_traits<R(Ts..., ...)const>;

    template <typename R, typename ... Ts>
    struct function_traits<R(Ts...)volatile>;

    template <typename R, typename ... Ts>
    struct function_traits<R(Ts..., ...)volatile>;

    template <typename R, typename ... Ts>
    struct function_traits<R(Ts...)const volatile>;

    template <typename R, typename ... Ts>
    struct function_traits<R(Ts..., ...)const volatile>;

    template <typename R, typename ... Ts>
    struct function_traits<R(Ts...)&>;

    template <typename R, typename ... Ts>
    struct function_traits<R(Ts..., ...)&>;

    template <typename R, typename ... Ts>
    struct function_traits<R(Ts...)const&>;

    template <typename R, typename ... Ts>
    struct function_traits<R(Ts..., ...)const&>;

    template <typename R, typename ... Ts>
    struct function_traits<R(Ts...)volatile&>;

    template <typename R, typename ... Ts>
    struct function_traits<R(Ts..., ...)volatile&>;

    template <typename R, typename ... Ts>
    struct function_traits<R(Ts...)const volatile&>;

    template <typename R, typename ... Ts>
    struct function_traits<R(Ts..., ...)const volatile&>;

    template <typename R, typename ... Ts>
    struct function_traits<R(Ts...) && >;

    template <typename R, typename ... Ts>
    struct function_traits<R(Ts..., ...) && >;

    template <typename R, typename ... Ts>
    struct function_traits<R(Ts...)const && >;

    template <typename R, typename ... Ts>
    struct function_traits<R(Ts..., ...)const && >;

    template <typename R, typename ... Ts>
    struct function_traits<R(Ts...)volatile && >;

    template <typename R, typename ... Ts>
    struct function_traits<R(Ts..., ...)volatile && >;

    template <typename R, typename ... Ts>
    struct function_traits<R(Ts...)const volatile && >;

    template <typename R, typename ... Ts>
    struct function_traits<R(Ts..., ...)const volatile && >;

    template <typename R, typename ... Ts>
    struct function_traits<R(Ts...)noexcept>;

    template <typename R, typename ... Ts>
    struct function_traits<R(Ts..., ...)noexcept>;

    template <typename R, typename ... Ts>
    struct function_traits<R(Ts...)const noexcept>;

    template <typename R, typename ... Ts>
    struct function_traits<R(Ts..., ...)const noexcept>;

    template <typename R, typename ... Ts>
    struct function_traits<R(Ts...)volatile noexcept>;

    template <typename R, typename ... Ts>
    struct function_traits<R(Ts..., ...)volatile noexcept>;

    template <typename R, typename ... Ts>
    struct function_traits<R(Ts...)const volatile noexcept>;

    template <typename R, typename ... Ts>
    struct function_traits<R(Ts..., ...)const volatile noexcept>;

    template <typename R, typename ... Ts>
    struct function_traits<R(Ts...)&noexcept>;

    template <typename R, typename ... Ts>
    struct function_traits<R(Ts..., ...)&noexcept>;

    template <typename R, typename ... Ts>
    struct function_traits<R(Ts...)const&noexcept>;

    template <typename R, typename ... Ts>
    struct function_traits<R(Ts..., ...)const&noexcept>;

    template <typename R, typename ... Ts>
    struct function_traits<R(Ts...)volatile&noexcept>;

    template <typename R, typename ... Ts>
    struct function_traits<R(Ts..., ...)volatile&noexcept>;

    template <typename R, typename ... Ts>
    struct function_traits<R(Ts...)const volatile&noexcept>;

    template <typename R, typename ... Ts>
    struct function_traits<R(Ts..., ...)const volatile&noexcept>;

    template <typename R, typename ... Ts>
    struct function_traits<R(Ts...) && noexcept>;

    template <typename R, typename ... Ts>
    struct function_traits<R(Ts..., ...) && noexcept>;

    template <typename R, typename ... Ts>
    struct function_traits<R(Ts...)const && noexcept>;

    template <typename R, typename ... Ts>
    struct function_traits<R(Ts..., ...)const && noexcept>;

    template <typename R, typename ... Ts>
    struct function_traits<R(Ts...)volatile && noexcept>;

    template <typename R, typename ... Ts>
    struct function_traits<R(Ts..., ...)volatile && noexcept>;

    template <typename R, typename ... Ts>
    struct function_traits<R(Ts...)const volatile && noexcept>;

    template <typename R, typename ... Ts>
    struct function_traits<R(Ts..., ...)const volatile && noexcept>;
}
```
{% endraw %}
