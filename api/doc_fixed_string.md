# Header file `fixed_string.h`

{% raw %}
``` cpp
namespace clu
{
    template <typename T, size_t N>
    struct basic_fixed_string;

    template <size_t N>
    using fixed_string = basic_fixed_string<char, N>;

    template <size_t N>
    using fixed_wstring = basic_fixed_string<wchar_t, N>;

    template <size_t N>
    using fixed_u8string = basic_fixed_string<char8_t, N>;

    template <size_t N>
    using fixed_u16string = basic_fixed_string<char16_t, N>;

    template <size_t N>
    using fixed_u32string = basic_fixed_string<char32_t, N>;

    namespace literals
    {
    }
}
```
{% endraw %}
