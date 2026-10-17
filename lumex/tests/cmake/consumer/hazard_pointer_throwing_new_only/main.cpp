// A program that replaces only the throwing operator new and operator delete
// (cmake.consumer_hazard_pointer_throwing_new_only). Every block it hands out
// carries a header; operator delete checks the header and aborts on a block
// that did not come from this operator new. A libstdc++ before GCC 9 (PR
// libstdc++/68210) does not route new (std::nothrow) to a replaced operator
// new, so an engine that took its thread cache and its scan table with
// new (std::nothrow) and freed them with delete would pass a plain malloc
// block to the operator delete below at thread exit and in a reclamation
// pass. The engine takes that internal storage from malloc and free, so the
// program's allocator never sees it. Exit code 0 means every check passed.

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <thread>
#include <vector>

#include "lumex/core/hazard_pointer/LumexHazardPointer"

namespace engine = lumex::core::hazard_pointer::engine;

static int g_failures = 0;

#define CHECK(condition)                                                      \
  do                                                                          \
    {                                                                         \
      if (!(condition))                                                       \
        {                                                                     \
          std::fprintf (stderr, "%s:%d: CHECK failed: %s\n", __FILE__,        \
                        __LINE__, #condition);                                \
          ++g_failures;                                                       \
        }                                                                     \
    }                                                                         \
  while (false)

// --- the allocator: a header in front of every block -----------------------

namespace
{
constexpr std::size_t kHeader = 32;
constexpr std::uint64_t kMagic = 0x4C554D4558484452ULL;
std::atomic<long> g_news (0);
std::atomic<long> g_deletes (0);
} // namespace

void *
operator new (std::size_t size)
{
  void *const block = std::malloc (size + kHeader);
  if (block == nullptr)
    {
      throw std::bad_alloc ();
    }
  static_cast<std::uint64_t *> (block)[0] = kMagic;
  static_cast<std::uint64_t *> (block)[1] = size;
  g_news.fetch_add (1);
  return static_cast<char *> (block) + kHeader;
}

void
operator delete (void *memory) noexcept
{
  if (memory == nullptr)
    {
      return;
    }
  void *const block = static_cast<char *> (memory) - kHeader;
  if (static_cast<std::uint64_t *> (block)[0] != kMagic)
    {
      std::fprintf (stderr, "operator delete: a block without a header\n");
      std::abort ();
    }
  static_cast<std::uint64_t *> (block)[0] = 0;
  g_deletes.fetch_add (1);
  std::free (block);
}

void
operator delete (void *memory, std::size_t) noexcept
{
  operator delete (memory);
}

// --- the retired objects ---------------------------------------------------

namespace
{
struct object_t
{
  engine::node_t node;
  int value;
};

std::atomic<long> g_reclaimed (0);

void
reclaim_object (engine::node_t *node)
{
  g_reclaimed.fetch_add (1);
  delete reinterpret_cast<object_t *> (node);
}

void
retire_object (int value)
{
  object_t *const object = new object_t ();
  object->value = value;
  engine::init_node (object->node);
  object->node.reclaim = &reclaim_object;
  engine::retire_node (&object->node);
}

// A thread that takes slots (which creates its cache), retires many objects
// (passes build the scan table) and exits (the exit hook hands the cache
// back and frees it).
void
worker (int rounds, int per_round)
{
  for (int round = 0; round < rounds; ++round)
    {
      engine::slot_t *slots[4];
      engine::acquire_slots (slots, 4);
      for (int i = 0; i < 4; ++i)
        {
          slots[i]->value.store (nullptr);
        }
      for (int i = 0; i < per_round; ++i)
        {
          retire_object (i);
        }
      for (int i = 0; i < 4; ++i)
        {
          engine::release_slot (slots[i]);
        }
    }
}
} // namespace

int
main ()
{
  // Short-lived threads: each creates a cache and destroys it at exit.
  for (int wave = 0; wave < 8; ++wave)
    {
      std::vector<std::thread> threads;
      for (int t = 0; t < 4; ++t)
        {
          threads.emplace_back (worker, 3, 2000);
        }
      for (std::size_t t = 0; t < threads.size (); ++t)
        {
          threads[t].join ();
        }
    }

  // The main thread keeps one object protected while it retires many, so the
  // scan table holds a real address.
  object_t *const kept = new object_t ();
  engine::init_node (kept->node);
  kept->node.reclaim = &reclaim_object;
  engine::slot_t *const slot = engine::acquire_slot ();
  slot->value.store (&kept->node);
  engine::reader_fence ();
  engine::retire_node (&kept->node);
  for (int i = 0; i < 20000; ++i)
    {
      retire_object (i);
    }
  engine::clean_up ();
  engine::statistics_t const held = engine::statistics ();
  CHECK (held.passes > 0);
  CHECK (held.retired - held.reclaimed == 1);
  slot->value.store (nullptr);
  engine::release_slot (slot);
  engine::clean_up ();

  engine::statistics_t const done = engine::statistics ();
  CHECK (done.retired == done.reclaimed);
  CHECK (static_cast<long> (done.reclaimed) == g_reclaimed.load ());
  // The blocks of hazard slots live for the whole process, so not every block
  // is freed; every retired object is.
  CHECK (g_deletes.load () >= g_reclaimed.load ());

  if (g_failures != 0)
    {
      return 1;
    }
  std::printf ("hazard_pointer_throwing_new_only: all checks passed\n");
  return 0;
}
