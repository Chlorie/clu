# Header file `iterator.h`

{% raw %}
``` cpp
#include "type_traits.h"

#include "concepts.h"

namespace clu
{
    template <typename It>
    class iterator_adapter;
}
```
{% endraw %}

### Class `clu::iterator_adapter`

{% raw %}
``` cpp
template <typename It>
class iterator_adapter
: public It
{
public:
    using base_type = It;

    using iterator_category = /*see-below*/;};
```
{% endraw %}

An adapter type for cutting down boilerplates when implementing iterators.

#### Template parameters

  - `It` - Base implementation type

### Type alias `clu::iterator_adapter::base_type`

{% raw %}
``` cpp
using base_type = It;
```
{% endraw %}

Base implementation type.

-----

### Type alias `clu::iterator_adapter::iterator_category`

{% raw %}
``` cpp
using iterator_category = /*see-below*/;
```
{% endraw %}

Iterator category of the adapted iterator type.

The iterator category is inferred from the base implementation with the following process:

  - If [`base_type`](doc_iterator.md#standardese-clu__iterator_adapter-It-__base_type) has an `iterator_category` member alias, or an `iterator_concept` member alias, use that.

  - If [`base_type`](doc_iterator.md#standardese-clu__iterator_adapter-It-__base_type) does not implement a self-equality operator, the category is inferred as [`std::input_iterator_tag`](http://en.cppreference.com/mwiki/index.php?title=Special%3ASearch&search=std::input_iterator_tag).

  - If [`base_type`](doc_iterator.md#standardese-clu__iterator_adapter-It-__base_type) implements `operator-` between two objects of its type, the category is inferred as [`std::random_access_iterator_tag`](http://en.cppreference.com/mwiki/index.php?title=Special%3ASearch&search=std::random_access_iterator_tag).

  - If [`base_type`](doc_iterator.md#standardese-clu__iterator_adapter-It-__base_type) implements prefix `operator--`, the category is inferred as [`std::bidirectional_iterator_tag`](http://en.cppreference.com/mwiki/index.php?title=Special%3ASearch&search=std::bidirectional_iterator_tag).

  - Otherwise, the category is inferred as [`std::forward_iterator_tag`](http://en.cppreference.com/mwiki/index.php?title=Special%3ASearch&search=std::forward_iterator_tag).

-----

-----
