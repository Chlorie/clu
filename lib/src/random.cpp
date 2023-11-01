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

    thread_rng_t& thread_rng()
    {
        thread_local thread_rng_t engine = []
        {
            seed_generator seed_gen;
            return thread_rng_t(seed_gen);
        }();
        return engine;
    }

    void reseed()
    {
        seed_generator seed_gen;
        thread_rng().seed(seed_gen);
    }

    void reseed(const thread_rng_t::result_type seed) { thread_rng().seed(seed); }
} // namespace clu
