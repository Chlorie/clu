# Header file `meta_list.h`

{% raw %}
``` cpp
namespace clu
{
    template <typename ... Types>
    struct type_list;

    template <typename ... Types>
    using to_value_list = value_list<Types::value...>;

    template <auto values ... >
    struct value_list;
}
```
{% endraw %}
