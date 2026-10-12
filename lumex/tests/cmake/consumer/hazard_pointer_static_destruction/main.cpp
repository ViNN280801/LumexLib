// Hazard pointers from static constructors and destructors and from
// thread-local destructors. Exit code 0 means nothing crashed and the objects
// retired during shutdown were handled.

#include <atomic>
#include <cstdio>
#include <thread>

#include "Shared.hpp"

namespace fixture
{
std::atomic<int> &
deleted ()
{
  // Constructed on first use; a static of another unit may call it first.
  static std::atomic<int> *count = new std::atomic<int> (0);
  return *count;
}
} // namespace fixture

namespace
{
namespace hp = lumex::core::hazard_pointer;

// A static hazard pointer that outlives main.
hp::hazard_pointer g_static_holder = hp::make_hazard_pointer ();

// A thread-local object whose destructor uses the domain.
struct thread_user
{
  void
  touch ()
  {
  }

  ~thread_user ()
  {
    hp::hazard_pointer holder = hp::make_hazard_pointer ();
    std::atomic<fixture::item *> source (
        new fixture::item (&fixture::deleted ()));
    fixture::item *const item = holder.protect (source);
    source.store (nullptr);
    item->retire ();
    holder.reset_protection ();
  }
};

thread_local thread_user t_user;

// A static destroyed after the engine's own statics, if it had any.
struct late_user
{
  ~late_user ()
  {
    hp::hazard_pointer holder = hp::make_hazard_pointer ();
    std::atomic<fixture::item *> source (
        new fixture::item (&fixture::deleted ()));
    fixture::item *const item = holder.protect (source);
    source.store (nullptr);
    item->retire ();
  }
};

late_user g_late;
} // namespace

int
main ()
{
  fixture::early_check ();
  // The static holder works.
  std::atomic<fixture::item *> source (
      new fixture::item (&fixture::deleted ()));
  fixture::item *const item = g_static_holder.protect (source);
  source.store (nullptr);
  item->retire ();
  hp::clean_up ();
  int const before = fixture::deleted ().load ();
  if (before < 1)
    {
      std::fprintf (stderr,
                    "the protected object was reclaimed early or never\n");
    }
  g_static_holder.reset_protection ();
  hp::clean_up ();
  if (fixture::deleted ().load () != before + 1)
    {
      std::fprintf (
          stderr, "the object was not reclaimed after the protection ended\n");
      return 1;
    }
  // Threads whose thread-local destructors retire objects.
  for (int i = 0; i < 4; ++i)
    {
      std::thread worker ([] { t_user.touch (); });
      worker.join ();
    }
  std::printf ("hazard_pointer_static_destruction: main done\n");
  return 0;
}
