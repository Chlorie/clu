# Header file `string_utils.h`

{% raw %}
``` cpp
#include "concepts.h"

#include "type_traits.h"

namespace clu
{
    template <typename T>concept char_type = same_as_any_of<T, char, unsigned char, signed char, char8_t, char16_t, char32_t>;
}
```
{% endraw %}
