# Header file `flags.h`

{% raw %}
``` cpp
#include "concepts.h"

#include "type_traits.h"

namespace clu
{
    template <typename Enum>concept flag_enum = enumeration<Enum> && (to_underlying(Enum::flags_bit_size)>0);

    template <typename Enum>
    class flags;

    inline namespace flag_enum_operators
    {
    }
}
```
{% endraw %}
