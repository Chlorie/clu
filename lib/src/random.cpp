#include "clu/random.h"

namespace clu
{
    namespace
    {
        class seed_generator
        {
        public:
            using result_type = std::random_device::result_type;

            template <typename It>
            void generate(It begin, It end)
            {
                for (; begin != end; ++begin)
                    *begin = dev_();
            }

        private:
            std::random_device dev_;
        };
    } // namespace

    std::mt19937& random_engine() { return thread_rng(); }

    std::mt19937& thread_rng()
    {
        thread_local std::mt19937 engine = []
        {
            seed_generator seed_gen;
            return std::mt19937(seed_gen);
        }();
        return engine;
    }

    void reseed()
    {
        seed_generator seed_gen;
        thread_rng().seed(seed_gen);
    }

    void reseed(const std::mt19937::result_type seed) { thread_rng().seed(seed); }
} // namespace clu
