# Header file `function_ref.h`

{% raw %}
``` cpp
#include "concepts.h"

namespace clu
{
    template <typename R, typename ... Ts>
    class 'hidden'<R(Ts...)> final;
}
```
{% endraw %}

### Class `clu::function_ref`

{% raw %}
``` cpp
template <typename R, typename ... Ts>
class 'hidden'<R(Ts...)> final
{
public:
    //=== empty_ctors ===//
    constexpr function_ref() noexcept = default;
    constexpr function_ref(std::nullptr_t) noexcept;
};
```
{% endraw %}

Non-owning type erased wrapper for invocable objects.

### Constructor `clu::function_ref::function_ref`

{% raw %}
``` cpp
(1) constexpr function_ref() noexcept = default;

(2) constexpr function_ref(std::nullptr_t) noexcept;
```
{% endraw %}

Construct an empty `function_ref`. Calling thus an object leads to undefined behavior.

-----

-----
