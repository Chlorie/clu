# Header file `c_str_view.h`

{% raw %}
``` cpp
#include "string_utils.h"

namespace clu
{
    template <typename Char>
    class basic_c_str_view;
}

template <typename Char>inline constexpr bool std::ranges::enable_view<clu::basic_c_str_view<Char>> = true;

template <typename Char>inline constexpr bool std::ranges::enable_borrowed_range<clu::basic_c_str_view<Char>> = true;

template <typename Char>inline constexpr bool std::ranges::disable_sized_range<clu::basic_c_str_view<Char>> = true;
```
{% endraw %}
