// A static object of another translation unit: its constructor makes a hazard
// pointer and retires an object while the program is still initializing, and
// its destructor does the same while it shuts down.

#include <cstdio>
#include <cstdlib>

#include "Shared.hpp"

namespace
{
struct early_user
{
  lumex::core::hazard_pointer::hazard_pointer holder;

  early_user () : holder (lumex::core::hazard_pointer::make_hazard_pointer ())
  {
    std::atomic<fixture::item *> source (
        new fixture::item (&fixture::deleted ()));
    fixture::item *const protected_item = holder.protect (source);
    source.store (nullptr);
    protected_item->retire ();
    holder.reset_protection ();
    lumex::core::hazard_pointer::clean_up ();
  }

  ~early_user ()
  {
    // Shutdown: use the domain again and leave a retired object behind.
    lumex::core::hazard_pointer::hazard_pointer late
        = lumex::core::hazard_pointer::make_hazard_pointer ();
    std::atomic<fixture::item *> source (
        new fixture::item (&fixture::deleted ()));
    fixture::item *const item = late.protect (source);
    source.store (nullptr);
    item->retire ();
    late.reset_protection ();
    lumex::core::hazard_pointer::clean_up ();
    if (fixture::deleted ().load () < 1)
      {
        std::fprintf (stderr, "static destruction: nothing was reclaimed\n");
        std::_Exit (3);
      }
  }
};

early_user g_early;
} // namespace

namespace fixture
{
void
early_check ()
{
  // g_early's constructor ran before main: it reclaimed its object.
  if (deleted ().load () < 1)
    {
      std::fprintf (stderr, "static initialization: nothing was reclaimed\n");
      std::_Exit (4);
    }
}
} // namespace fixture
