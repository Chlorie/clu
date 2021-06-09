# Header file `buffer.h`

{% raw %}
``` cpp
#include "concepts.h"

namespace clu
{
    template <typename T>concept alias_safe = same_as_any_of<T, unsigned char, char, std::byte>;

    template <typename T>concept buffer_safe = trivially_copyable<T> || (std::is_array_v<T> && trivially_copyable<std::remove_all_extents_t<T>>);

    template <typename T>concept trivial_range = std::ranges::contiguous_range<T> && std::ranges::sized_range<T> && trivially_copyable<std::ranges::range_value_t<T>>;

    template <typename T>concept mutable_trivial_range = trivial_range<T> && !std::is_const_v<std::ranges::range_value_t<T>>;

    template <typename T>
    class basic_buffer final;
}
```
{% endraw %}
