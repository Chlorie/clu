#include "clu/execution_contexts/threads.h"

#include <thread>
#include <mutex>
#include <optional>

#include "clu/scope.h"
#include "clu/new.h"

namespace clu::detail::static_tp
{
    class pool::thread_res
    {
    public:
        // clang-format off
        template <typename Fn>
        explicit thread_res(Fn&& func): thread_(static_cast<Fn&&>(func)) {}
        // clang-format on

        ~thread_res() noexcept
        {
            CLU_ASSERT(!thread_.joinable(), //
                "finish() should be called before the destruction of a static thread pool");
        }

        void finish()
        {
            std::unique_lock lock(mutex_);
            finishing_ = true;
            cv_.notify_all();
        }

        void join()
        {
            if (thread_.joinable())
                thread_.join();
        }

        void enqueue(ops_base* task)
        {
            std::unique_lock lock(mutex_);
            enqueue_with_lock(task);
        }

        bool try_enqueue(ops_base* task)
        {
            const std::unique_lock lock(mutex_, std::try_to_lock);
            if (lock.owns_lock())
                enqueue_with_lock(task);
            return lock.owns_lock();
        }

        ops_base* dequeue()
        {
            std::unique_lock lock(mutex_);
            return dequeue_with_lock(lock);
        }

        std::optional<ops_base*> try_dequeue()
        {
            std::unique_lock lock(mutex_, std::try_to_lock);
            if (lock.owns_lock())
                return dequeue_with_lock(lock);
            return std::nullopt;
        }

    private:
        std::mutex mutex_;
        std::condition_variable cv_;
        bool finishing_ = false;
        ops_base* head_ = nullptr;
        ops_base* tail_ = nullptr;
        std::thread thread_;

        void enqueue_with_lock(ops_base* task) noexcept
        {
            tail_ = (head_ ? tail_->state.next : head_) = task;
            cv_.notify_one();
        }

        ops_base* dequeue_with_lock(std::unique_lock<std::mutex>& lock)
        {
            cv_.wait(lock, [this] { return head_ != nullptr || finishing_; });
            if (!head_)
                return nullptr;
            ops_base* ptr = head_;
            head_ = head_->state.next;
            if (!head_)
                tail_ = nullptr;
            return ptr;
        }
    };

    pool::pool(const std::size_t size): size_(size)
    {
        res_ = static_cast<thread_res*>(aligned_alloc_for<thread_res>(size));
        scope_fail _1([&] { aligned_free_for<thread_res>(res_, size); });
        std::size_t i = 0;
        scope_fail _2([&] { std::destroy_n(res_, i); });
        for (i = 0; i < size; i++)
            std::construct_at(res_ + i, [this, i] { work(i); });
    }

    pool::~pool() noexcept
    {
        finish();
        std::destroy_n(res_, size_);
        aligned_free_for<thread_res>(res_, size_);
    }

    void pool::finish()
    {
        for (std::size_t i = 0; i < size_; i++)
            res_[i].finish();
        for (std::size_t i = 0; i < size_; i++)
            res_[i].join();
    }

    void pool::enqueue(ops_base* task)
    {
        constexpr std::size_t spin_round = 2;
        const auto index = index_.fetch_add(1, std::memory_order::relaxed);
        for (std::size_t i = index; i < index + spin_round * size_; i++)
            if (res_[i % size_].try_enqueue(task))
                return;
        res_[index % size_].enqueue(task);
    }

    void pool::work(const std::size_t index)
    {
        const auto get_task = [=, this]
        {
            for (std::size_t i = index; i < index + size_; i++)
                if (const auto task = res_[i % size_].try_dequeue()) // acquired the lock
                    return *task;
            return res_[index].dequeue();
        };
        while (ops_base* task = get_task())
            task->set();
    }
} // namespace clu::detail::static_tp
