#ifndef LUMEX_TESTS_HAZARD_POINTER_STATIC_DESTRUCTION_HPP
#define LUMEX_TESTS_HAZARD_POINTER_STATIC_DESTRUCTION_HPP

#include <atomic>

#include "lumex/core/hazard_pointer/LumexHazardPointer"

namespace fixture
{
struct item : lumex::core::hazard_pointer::hazard_pointer_obj_base<item>
{
  explicit item (std::atomic<int> *the_counter) : counter (the_counter) {}
  ~item () { counter->fetch_add (1); }
  std::atomic<int> *counter;
};

// Counts deletions; defined in main.cpp.
std::atomic<int> &deleted ();

// Defined in early.cpp, which main.cpp's statics are initialized after or
// before depending on the link order: a static whose constructor retires.
void early_check ();
} // namespace fixture

#endif
