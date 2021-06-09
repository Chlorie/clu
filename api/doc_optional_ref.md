# Header file `optional_ref.h`

{% raw %}
``` cpp
namespace clu
{
    template <typename T>
    class optional_ref final;

    template <typename T>
    using optional_param = optional_ref<const T>;
}
```
{% endraw %}
