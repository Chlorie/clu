# Header file `polymorphic_value.h`

{% raw %}
``` cpp
#include "concepts.h"

namespace clu
{
    template <typename C, typename T>concept copier_of = std::copy_constructible<C> && requires(C copier, const T&value){{copier(value)}->std::convertible_to<T*>;};

    template <typename D, typename T>concept deleter_of = std::copy_constructible<D> && requires(D deleter, T*ptr){{deleter(ptr)}noexcept;};

    template <typename T>
    struct default_copy;

    class bad_polymorphic_value_construction;

    template <typename T>
    class polymorphic_value;
}
```
{% endraw %}
