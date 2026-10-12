#include <atomic>
#include <cstdint>
#include <iostream>
#include <thread>
#include <vector>

#include "lumex/core/hazard_pointer/LumexHazardPointer"

namespace hp = lumex::core::hazard_pointer;

// A workflow: a lock-free stack whose readers and writers run on several
// threads. A popped node is retired, not deleted: another thread may still be
// looking at it. The hazard pointer is what makes the compare-and-swap of pop
// safe, too: while a thread holds the top node, its address cannot be freed
// and allocated again for another node, so the head cannot be mistaken for the
// same node (the ABA problem).

namespace
{
struct Node : hp::hazard_pointer_obj_base<Node>
{
  explicit Node (std::int64_t the_value) : next (nullptr), value (the_value) {}

  std::atomic<Node *> next;
  std::int64_t value;
};

class Stack
{
public:
  Stack () : head_ (nullptr) {}

  ~Stack ()
  {
    hp::hazard_pointer holder = hp::make_hazard_pointer ();
    std::int64_t value = 0;
    while (pop (holder, value))
      {
      }
    hp::clean_up ();
  }

  void
  push (std::int64_t value)
  {
    Node *node = new Node (value);
    Node *top = head_.load (std::memory_order_relaxed);
    do
      {
        node->next.store (top, std::memory_order_relaxed);
      }
    while (!head_.compare_exchange_weak (top, node, std::memory_order_release,
                                         std::memory_order_relaxed));
  }

  bool
  pop (hp::hazard_pointer &holder, std::int64_t &value)
  {
    Node *top = head_.load (std::memory_order_acquire);
    for (;;)
      {
        // Announce the top node and check that it is still the top.
        if (!holder.try_protect (top, head_))
          {
            continue;
          }
        if (top == nullptr)
          {
            return false;
          }
        Node *next = top->next.load (std::memory_order_relaxed);
        if (head_.compare_exchange_strong (top, next,
                                           std::memory_order_acq_rel,
                                           std::memory_order_acquire))
          {
            value = top->value;
            holder.reset_protection ();
            top->retire (); // deleted when no hazard pointer names it
            return true;
          }
      }
  }

private:
  std::atomic<Node *> head_;
};
} // namespace

int
main ()
{
  std::cout
      << "=== hazard_pointer: a lock-free stack on several threads ===\n\n";

  Stack stack;
  unsigned const threads = 4;
  std::int64_t const per_thread = 20000;
  std::atomic<std::int64_t> popped_sum (0);
  std::atomic<std::int64_t> popped_count (0);

  std::vector<std::thread> workers;
  for (unsigned t = 0; t < threads; ++t)
    {
      workers.emplace_back (
          [&, t]
            {
              hp::hazard_pointer holder = hp::make_hazard_pointer ();
              for (std::int64_t i = 0; i < per_thread; ++i)
                {
                  stack.push (static_cast<std::int64_t> (t) * per_thread + i
                              + 1);
                  std::int64_t value = 0;
                  if (stack.pop (holder, value))
                    {
                      popped_sum.fetch_add (value);
                      popped_count.fetch_add (1);
                    }
                }
            });
    }
  for (std::thread &worker : workers)
    {
      worker.join ();
    }

  // Whatever is left comes out single-threaded.
  hp::hazard_pointer holder = hp::make_hazard_pointer ();
  std::int64_t value = 0;
  while (stack.pop (holder, value))
    {
      popped_sum.fetch_add (value);
      popped_count.fetch_add (1);
    }
  holder.reset_protection ();
  hp::clean_up ();

  std::int64_t const total = static_cast<std::int64_t> (threads) * per_thread;
  std::int64_t const expected_sum = total * (total + 1) / 2;
  std::cout << "pushed " << total << " values, popped " << popped_count.load ()
            << ", sum " << popped_sum.load () << " (expected " << expected_sum
            << ")\n";
  bool const ok
      = popped_count.load () == total && popped_sum.load () == expected_sum;
  std::cout << (ok ? "every value came out exactly once\n" : "MISMATCH\n");

  hp::engine::statistics_t const stats = hp::engine::statistics ();
  std::cout << "hazard slots ever created: " << stats.records
            << ", nodes retired " << stats.retired << ", reclaimed "
            << stats.reclaimed << '\n';
  return ok ? 0 : 1;
}
