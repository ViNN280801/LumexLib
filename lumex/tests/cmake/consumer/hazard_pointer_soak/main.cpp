// Soak run of the hazard pointer structures. LUMEX_HP_SOAK_SECONDS (default
// 10) is the duration, LUMEX_HP_SEED the seed of the first round (printed;
// every later round adds one), LUMEX_HP_MAX_THREADS the largest thread count
// and LUMEX_HP_SCALE the work of one round in percent. Exit code 0 means no
// invariant was violated in any round.

#include <chrono>
#include <cstdio>
#include <cstdlib>

#include "lumex/tests/core/hazard_pointer/LumexHazardPointerTestStress.hpp"

using namespace lumex_hp_structures;

int
main ()
{
  unsigned const seconds = env_number ("LUMEX_HP_SOAK_SECONDS", 10);
  unsigned const scale = env_number ("LUMEX_HP_SCALE", 100);
  std::uint64_t const first_seed = run_seed ();
  std::chrono::steady_clock::time_point const end
      = std::chrono::steady_clock::now () + std::chrono::seconds (seconds);
  unsigned rounds = 0;
  int failures = 0;
  do
    {
      std::uint64_t const seed = first_seed + rounds;
      std::printf ("round %u seed %llu\n", rounds,
                   static_cast<unsigned long long> (seed));
      std::fflush (stdout);
      for (unsigned threads : thread_counts ())
        {
          unsigned const producers = threads / 2 > 0 ? threads / 2 : 1;
          unsigned const consumers
              = threads - threads / 2 > 0 ? threads - threads / 2 : 1;
          long const per_producer
              = static_cast<long> (2000 * scale / 100 + 10);
          stack_outcome const stack = run_stack_stress<hazard_policy> (
              producers, consumers, per_producer, seed);
          if (stack.duplicate || stack.popped != stack.expected_count
              || stack.sum != stack.expected_sum)
            {
              std::fprintf (stderr, "stack violated: %u threads, seed %llu\n",
                            threads, static_cast<unsigned long long> (seed));
              ++failures;
            }
          queue_outcome const queue = run_queue_stress<hazard_policy> (
              producers, consumers, per_producer, seed);
          if (queue.duplicate || queue.order_broken
              || queue.dequeued != queue.expected)
            {
              std::fprintf (stderr, "queue violated: %u threads, seed %llu\n",
                            threads, static_cast<unsigned long long> (seed));
              ++failures;
            }
          list_outcome const list = run_list_stress<hazard_policy> (
              threads, 4000 * scale / 100 + 10, seed);
          if (list.unsorted || list.membership_wrong)
            {
              std::fprintf (stderr, "list violated: %u threads, seed %llu\n",
                            threads, static_cast<unsigned long long> (seed));
              ++failures;
            }
        }
      ++rounds;
    }
  while (failures == 0 && std::chrono::steady_clock::now () < end);
  std::printf ("hazard_pointer_soak: %u rounds, %d violations\n", rounds,
               failures);
  return failures == 0 ? 0 : 1;
}
