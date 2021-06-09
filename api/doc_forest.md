# Header file `forest.h`

{% raw %}
``` cpp
#include "take.h"

#include "iterator.h"

#include "assertion.h"

namespace clu
{
    template <typename T, typename Alloc = std::allocator<T>>>
    class forest;

    namespace pmr
    {
        template <typename T>
        using forest = forest<T, std::pmr::polymorphic_allocator<T>>;
    }
}
```
{% endraw %}

### Class `clu::forest`

{% raw %}
``` cpp
template <typename T, typename Alloc = std::allocator<T>>>
class forest
{
};
```
{% endraw %}

STL-style hierarchical container type.

#### Template parameters

  - `T` - The value type.
  - `Alloc` - The allocator type. Must not allocate fancy pointers.

-----
