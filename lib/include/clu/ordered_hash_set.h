#pragma once

#include <unordered_set>
#include <ranges>

#include "intrusive_list.h"

namespace clu
{
    namespace detail
    {
        template <typename Key>
        struct key_holder : intrusive_list_element_base<key_holder<Key>>
        {
            Key key;

            template <typename... Ts>
                requires std::constructible_from<Key, Ts...>
            explicit key_holder(Ts&&... args): key(static_cast<Ts&&>(args)...)
            {
            }
        };

        template <typename Key, typename Hash>
        struct key_hasher : Hash
        {
            using Hash::operator();
            constexpr std::size_t operator()(const key_holder<Key>& holder) const
                CLU_SINGLE_RETURN((*this)(holder.key));
        };

        template <typename Key, typename Equal>
        struct key_equal_to : Equal
        {
            using Equal::operator();

            constexpr bool operator()(const key_holder<Key>& lhs, const key_holder<Key>& rhs) const
                CLU_SINGLE_RETURN((*this)(lhs.key, rhs.key));

            template <typename T>
                requires callable<Equal, Key, T>
            constexpr bool operator()(const key_holder<Key>& lhs, const T& rhs) const
                CLU_SINGLE_RETURN((*this)(lhs.key, rhs));

            template <typename T>
                requires callable<Equal, T, Key>
            constexpr bool operator()(const T& lhs, const key_holder<Key>& rhs) const
                CLU_SINGLE_RETURN((*this)(lhs, rhs.key));
        };

        struct key_projection_t
        {
            template <typename Key>
            constexpr Key& operator()(key_holder<Key>& kh) const noexcept
            {
                return kh.key;
            }

            template <typename Key>
            constexpr const Key& operator()(const key_holder<Key>& kh) const noexcept
            {
                return kh.key;
            }
        } inline constexpr key_projection{};

        template <typename Key, typename Hash, typename Equal, typename Alloc,
            template <typename, typename, typename, typename> typename SetTempl, typename Self>
        class ordered_hash_set_impl
        {
        public:
            using key_type = Key;
            using value_type = Key;
            using size_type = std::size_t;
            using difference_type = std::ptrdiff_t;
            using hasher = Hash;
            using key_equal = Equal;
            using allocator_type = Alloc;
            using reference = Key&;
            using const_reference = const Key&;
            using pointer = typename std::allocator_traits<Alloc>::pointer;
            using const_pointer = typename std::allocator_traits<Alloc>::const_pointer;

        protected:
            using set_type = SetTempl< //
                key_holder<Key>, //
                key_hasher<Key, Hash>, //
                key_equal_to<Key, Equal>, //
                typename std::allocator_traits<Alloc>::template rebind_alloc<key_holder<Key>>>;
            using list_type = intrusive_list<key_holder<Key>>;

        private:
            template <typename Iter>
            class projected_set_iter_impl
            {
            public:
                projected_set_iter_impl() = default;
                explicit projected_set_iter_impl(const Iter iter) noexcept: iter_(iter) {}

                const Key& operator*() const noexcept { return iter_->key; }
                friend bool operator==(projected_set_iter_impl, projected_set_iter_impl) = default;

                projected_set_iter_impl& operator++()
                {
                    ++iter_;
                    return *this;
                }

            private:
                friend ordered_hash_set_impl;
                Iter iter_;
            };

            template <typename Iter>
            using projected_set_iter = iterator_adapter<projected_set_iter_impl<Iter>>;

            class projected_list_iter_impl
            {
            private:
                using base = typename list_type::const_iterator;

            public:
                projected_list_iter_impl() = default;
                explicit projected_list_iter_impl(const base iter) noexcept: iter_(iter) {}

                const Key& operator*() const noexcept { return iter_->key; }
                friend bool operator==(projected_list_iter_impl, projected_list_iter_impl) = default;

                projected_list_iter_impl& operator++()
                {
                    ++iter_;
                    return *this;
                }

                projected_list_iter_impl& operator--()
                {
                    --iter_;
                    return *this;
                }

            private:
                friend ordered_hash_set_impl;
                base iter_;
            };

            static constexpr bool nothrow_move_assignable = //
                std::allocator_traits<Alloc>::is_always_equal::value && //
                std::is_nothrow_move_assignable_v<Hash> && //
                std::is_nothrow_move_assignable_v<Equal>;

            static constexpr bool pocca = std::allocator_traits<Alloc>::propagate_on_container_copy_assignment::value;
            static constexpr bool pocma = std::allocator_traits<Alloc>::propagate_on_container_move_assignment::value;

        public:
            using iterator = iterator_adapter<projected_list_iter_impl>;
            using const_iterator = iterator;
            using reverse_iterator = std::reverse_iterator<iterator>;
            using const_reverse_iterator = std::reverse_iterator<const_iterator>;
            using set_iterator = projected_set_iter<typename set_type::iterator>;
            using const_set_iterator = projected_set_iter<typename set_type::const_iterator>;
            using local_iterator = projected_set_iter<typename set_type::local_iterator>;
            using const_local_iterator = projected_set_iter<typename set_type::const_local_iterator>;
            using node_type = typename set_type::node_type;

            ordered_hash_set_impl() = default;

            // clang-format off
            explicit ordered_hash_set_impl(const std::size_t bucket_count,
                const Hash& hash = Hash{}, const Equal& equal = Equal{}, const Alloc& alloc = Alloc{}):
                set_(bucket_count, key_hasher<Key, Hash>(hash), key_equal_to<Key, Equal>(equal), alloc) {}

            ordered_hash_set_impl(const std::size_t bucket_count, const Alloc& alloc):
                set_(bucket_count, key_hasher<Key, Hash>{}, key_equal_to<Key, Equal>{}, alloc) {}

            ordered_hash_set_impl(const std::size_t bucket_count,
                const Hash& hash, const Alloc& alloc):
                set_(bucket_count, key_hasher<Key, Hash>(hash), key_equal_to<Key, Equal>{}, alloc) {}

            template <std::input_iterator It, std::sentinel_for<It> Se>
            ordered_hash_set_impl(It first, const Se last) { this->append(first, last); }

            template <std::input_iterator It, std::sentinel_for<It> Se>
            ordered_hash_set_impl(It first, const Se last, const std::size_t bucket_count,
                const Hash& hash = Hash{}, const Equal& equal = Equal{}, const Alloc& alloc = Alloc{}):
                ordered_hash_set_impl(bucket_count, hash, equal, alloc) { this->append(first, last); }

            template <std::input_iterator It, std::sentinel_for<It> Se>
            ordered_hash_set_impl(It first, const Se last, const std::size_t bucket_count, const Alloc& alloc):
                ordered_hash_set_impl(bucket_count, alloc) { this->append(first, last); }

            template <std::input_iterator It, std::sentinel_for<It> Se>
            ordered_hash_set_impl(It first, const Se last,
                const std::size_t bucket_count, const Hash& hash, const Alloc& alloc):
                ordered_hash_set_impl(bucket_count, hash, alloc) { this->append(first, last); }

            template <std::ranges::input_range Rng>
            explicit ordered_hash_set_impl(Rng&& range) { this->append(range); }

            template <std::ranges::input_range Rng>
            ordered_hash_set_impl(Rng&& range, const std::size_t bucket_count,
                const Hash& hash = Hash{}, const Equal& equal = Equal{}, const Alloc& alloc = Alloc{}):
                ordered_hash_set_impl(bucket_count, hash, equal, alloc) { this->append(range); }

            template <std::ranges::input_range Rng>
            ordered_hash_set_impl(Rng&& range, const std::size_t bucket_count, const Alloc& alloc):
                ordered_hash_set_impl(bucket_count, alloc) { this->append(range); }

            template <std::ranges::input_range Rng>
            ordered_hash_set_impl(Rng&& range, const std::size_t bucket_count, const Hash& hash, const Alloc& alloc):
                ordered_hash_set_impl(bucket_count, hash, alloc) { this->append(range); }
            
            explicit ordered_hash_set_impl(const std::initializer_list<Key> ilist) { this->append(ilist); }
            
            ordered_hash_set_impl(const std::initializer_list<Key> ilist, const std::size_t bucket_count,
                const Hash& hash = Hash{}, const Equal& equal = Equal{}, const Alloc& alloc = Alloc{}):
                ordered_hash_set_impl(bucket_count, hash, equal, alloc) { this->append(ilist); }
            
            ordered_hash_set_impl(const std::initializer_list<Key> ilist,
                const std::size_t bucket_count, const Alloc& alloc):
                ordered_hash_set_impl(bucket_count, alloc) { this->append(ilist); }

            ordered_hash_set_impl(const std::initializer_list<Key> ilist, const std::size_t bucket_count,
                const Hash& hash, const Alloc& alloc):
                ordered_hash_set_impl(bucket_count, hash, alloc) { this->append(ilist); }

            ordered_hash_set_impl(const ordered_hash_set_impl& other):
                ordered_hash_set_impl(other,
                    std::allocator_traits<Alloc>::select_on_container_copy_construction(other.get_allocator())) {}
            // clang-format on

            ordered_hash_set_impl(const ordered_hash_set_impl& other, const Alloc& allocator):
                ordered_hash_set_impl(other.bucket_count(), other.hash_function(), other.key_eq(), allocator)
            {
                set_.max_load_factor(other.set_.max_load_factor());
                this->append(other.list_);
            }

            ordered_hash_set_impl(ordered_hash_set_impl&&) = default;

            ordered_hash_set_impl(ordered_hash_set_impl&& other, const Alloc& alloc): ordered_hash_set_impl(alloc)
            {
                list_ = std::move(other.list_);
                if (alloc == other.get_allocator())
                {
                    set_ = std::move(other.set_);
                    other.clear();
                    return;
                }
                this->move_from(std::move(other));
            }

            ~ordered_hash_set_impl() noexcept = default;

            ordered_hash_set_impl& operator=(const ordered_hash_set_impl& other)
            {
                if (&other == this)
                    return *this;
                if (pocca && get_allocator() != other.get_allocator())
                {
                    set_ = set_type(other.bucket_count(), other.hash_function(), other.key_eq(), other.get_allocator());
                    list_.clear();
                    set_.max_load_factor(other.set_.max_load_factor());
                }
                else
                    clear();
                this->append(other.list_);
                return *this;
            }

            ordered_hash_set_impl& operator=(ordered_hash_set_impl&& other) noexcept(nothrow_move_assignable)
            {
                if (&other == this)
                    return *this;
                if constexpr (pocma && get_allocator() != other.get_allocator())
                {
                    set_ = set_type(other.bucket_count(), other.hash_function(), other.key_eq(), other.get_allocator());
                    list_.clear();
                }
                else
                    clear();
                this->move_from(std::move(other));
                return *this;
            }

            ordered_hash_set_impl& operator=(const std::initializer_list<Key> ilist)
            {
                clear();
                this->append(ilist);
                return *this;
            }

            [[nodiscard]] Alloc get_allocator() const noexcept { return set_.get_allocator(); }

            // clang-format off
            [[nodiscard]] iterator begin() noexcept { return cbegin(); }
            [[nodiscard]] const_iterator begin() const noexcept { return cbegin(); }
            [[nodiscard]] const_iterator cbegin() const noexcept { return const_iterator(list_.begin()); }
            [[nodiscard]] iterator end() noexcept { return cend(); }
            [[nodiscard]] const_iterator end() const noexcept { return cend(); }
            [[nodiscard]] const_iterator cend() const noexcept { return const_iterator(list_.end()); }
            [[nodiscard]] reverse_iterator rbegin() noexcept { return crbegin(); }
            [[nodiscard]] const_reverse_iterator rbegin() const noexcept { return crbegin(); }
            [[nodiscard]] const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(list_.rbegin()); }
            [[nodiscard]] reverse_iterator rend() noexcept { return crend(); }
            [[nodiscard]] const_reverse_iterator rend() const noexcept { return crend(); }
            [[nodiscard]] const_reverse_iterator crend() const noexcept { return const_reverse_iterator(list_.rend()); }
            // clang-format on

            [[nodiscard]] bool empty() const noexcept { return set_.empty(); }
            [[nodiscard]] std::size_t size() const noexcept { return set_.size(); }
            [[nodiscard]] std::size_t max_size() const noexcept { return set_.max_size(); }

            void clear() noexcept
            {
                list_.clear();
                set_.clear();
            }

            // clang-format off
            template <typename... Ts>
                requires std::constructible_from<Key, Ts...>
            auto emplace(const const_iterator iter, Ts&&... args)
            {
                return self().emplace_with(
                    [&](key_holder<Key>& kh){ this->list_.insert(iter.base().iter_, kh); },
                    static_cast<Ts&&>(args)...);
            }

            template <typename... Ts>
                requires std::constructible_from<Key, Ts...>
            auto emplace_front(Ts&&... args)
            {
                return self().emplace_with(
                    [&](key_holder<Key>& kh){ this->list_.push_front(kh); },
                    static_cast<Ts&&>(args)...);
            }

            template <typename... Ts>
                requires std::constructible_from<Key, Ts...>
            auto emplace_back(Ts&&... args)
            {
                return self().emplace_with(
                    [&](key_holder<Key>& kh){ this->list_.push_back(kh); },
                    static_cast<Ts&&>(args)...);
            }
            // clang-format on

            decltype(auto) insert(const const_iterator iter, const Key& key) { return this->emplace(iter, key); }
            decltype(auto) insert(const const_iterator iter, Key&& key) { return this->emplace(iter, std::move(key)); }
            decltype(auto) push_front(const Key& key) { return this->emplace_front(key); }
            decltype(auto) push_front(Key&& key) { return this->emplace_front(std::move(key)); }
            decltype(auto) push_back(const Key& key) { return this->emplace_back(key); }
            decltype(auto) push_back(Key&& key) { return this->emplace_back(std::move(key)); }

            template <std::input_iterator It, std::sentinel_for<It> Se>
            void append(It first, const Se last)
            {
                if constexpr (std::random_access_iterator<It>)
                    set_.reserve(std::ranges::distance(first, last));
                while (first != last)
                {
                    (void)self().push_back(*first);
                    ++first;
                }
            }

            template <std::ranges::input_range Rng>
            void append(Rng&& range)
            {
                this->append(std::ranges::begin(range), std::ranges::end(range));
            }

            void append(const std::initializer_list<Key> ilist) { this->append(ilist.begin(), ilist.end()); }

            void swap(Self& other) noexcept(std::is_nothrow_swappable_v<set_type>)
            {
                set_.swap(other.set_);
                list_.swap(other.list_);
            }

            friend void swap(Self& lhs, Self& rhs) noexcept(std::is_nothrow_swappable_v<set_type>) { lhs.swap(rhs); }

            [[nodiscard]] std::size_t bucket_count() const noexcept { return set_.bucket_count(); }
            [[nodiscard]] std::size_t max_bucket_count() const noexcept { return set_.max_bucket_count(); }
            [[nodiscard]] std::size_t bucket_size() const noexcept { return set_.bucket_size(); }

            [[nodiscard]] float load_factor() const noexcept { return set_.load_factor(); }
            [[nodiscard]] float max_load_factor() const noexcept { return set_.max_load_factor(); }
            void max_load_factor(const float factor) { set_.max_load_factor(factor); }
            void rehash(const std::size_t count) { set_.rehash(count); }
            void reserve(const std::size_t count) { set_.reserve(count); }

            [[nodiscard]] Hash hash_function() const noexcept { return static_cast<Hash>(set_.hash_function()); }
            [[nodiscard]] Equal key_eq() const noexcept { return static_cast<Equal>(set_.key_eq()); }

            [[nodiscard]] friend bool operator==(const Self& lhs, const Self& rhs)
            {
                if (lhs.set_.size() != rhs.set_.size())
                    return false;
                return std::ranges::equal(lhs.list_, rhs.list_, //
                    std::equal_to{}, key_projection, key_projection);
            }

        protected:
            set_type set_;
            list_type list_;

            Self& self() noexcept { return static_cast<Self&>(*this); }
            const Self& self() const noexcept { return static_cast<const Self&>(*this); }

            auto insert_to_set_get_iter(key_holder<Key>&& kh)
            {
                // ReSharper disable once CppTooWideScopeInitStatement
                const auto res = set_.insert(std::move(kh));
                if constexpr (is_template_of_v<std::decay_t<decltype(res)>, std::pair>)
                    return res->first;
                else
                    return res;
            }

            void move_from(ordered_hash_set_impl&& other)
            {
                set_.max_load_factor(other.set_.max_load_factor());
                set_.reserve(other.set_.size());
                // Extract each of the elements in the other set, insert them
                // one by one into this set, and recover the list structure
                // at each step
                while (!other.set_.empty())
                {
                    auto node = other.set_.extract(set_.begin());
                    key_holder<Key>& kh = node.value();
                    const auto list_iter = list_.erase(list_.get_iterator(kh));
                    key_holder<Key>& new_kh = this->insert_to_set_get_iter(std::move(kh));
                    list_.insert(list_iter, new_kh);
                }
            }
        };
    } // namespace detail

    // clang-format off
    template <typename Key,
        typename KeyHash = std::hash<Key>,
        typename KeyEqual = std::equal_to<Key>,
        typename Alloc = std::allocator<Key>>
    class ordered_hash_set : public detail::ordered_hash_set_impl<
        Key, KeyHash, KeyEqual, Alloc, std::unordered_set,
        ordered_hash_set<Key, KeyHash, KeyEqual, Alloc>>
    // clang-format on
    {
    private:
        using base = detail::ordered_hash_set_impl<Key, KeyHash, KeyEqual, Alloc, //
            std::unordered_set, ordered_hash_set>;
        friend base;

    public:
        using base::base;
        using iterator = typename base::iterator;

    private:
        template <typename F, typename... Ts>
        std::pair<iterator, bool> emplace_with(const F& func, Ts&&... args)
        {
            const auto [iter, inserted] = this->set_.emplace(static_cast<Ts&&>(args)...);
            if (inserted)
            {
                auto& kh = const_cast<detail::key_holder<Key>&>(*iter);
                func(kh);
                this->list_.push_back(kh);
            }
            const iterator res_iter(base::list_type::get_iterator(*iter));
            return {res_iter, inserted};
        }
    };

    // clang-format off
    template <typename Key,
        typename KeyHash = std::hash<Key>,
        typename KeyEqual = std::equal_to<Key>,
        typename Alloc = std::allocator<Key>>
    class ordered_hash_multiset : public detail::ordered_hash_set_impl<
        Key, KeyHash, KeyEqual, Alloc, std::unordered_multiset,
        ordered_hash_multiset<Key, KeyHash, KeyEqual, Alloc>>
    // clang-format on
    {
    private:
        using base = detail::ordered_hash_set_impl<Key, KeyHash, KeyEqual, Alloc, //
            std::unordered_multiset, ordered_hash_multiset>;
        friend base;

    public:
        using base::base;

    private:
        template <typename F, typename... Ts>
        const Key& emplace_with(const F& func, Ts&&... args)
        {
            const auto iter = this->set_.emplace(static_cast<Ts&&>(args)...);
            auto& kh = const_cast<detail::key_holder<Key>&>(*iter);
            func(kh);
            return iter->key;
        }
    };
} // namespace clu
