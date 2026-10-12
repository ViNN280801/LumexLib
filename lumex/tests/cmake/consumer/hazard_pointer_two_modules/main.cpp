// Two shared libraries, one hazard pointer domain: the object that the holder
// library protects is not reclaimed by a pass that the retiring library runs,
// and both libraries (and this executable) see the same slot count.

#include <atomic>
#include <cstdio>

#include "Modules.hpp"

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

int
main ()
{
  namespace hp = lumex::core::hazard_pointer;
  std::atomic<int> deleted (0);
  std::atomic<fixture::item *> source (new fixture::item (&deleted));
  fixture::item *const object = source.load ();

  fixture::hold (source); // protected by a holder inside one library
  source.store (nullptr);
  fixture::retire_item (object); // retired from the other library
  fixture::clean_up_here ();     // and a pass runs there
  CHECK (deleted.load () == 0);
  hp::clean_up (); // and one here
  CHECK (deleted.load () == 0);

  std::size_t const here = hp::engine::statistics ().records;
  CHECK (fixture::records_seen_by_holder () == here);
  CHECK (fixture::records_seen_by_retirer () == here);
  CHECK (here >= 1);

  fixture::let_go ();        // the holder library ends the protection
  fixture::clean_up_here (); // the retiring library reclaims
  CHECK (deleted.load () == 1);

  if (g_failures != 0)
    {
      return 1;
    }
  std::printf ("hazard_pointer_two_modules: one domain for all\n");
  return 0;
}
