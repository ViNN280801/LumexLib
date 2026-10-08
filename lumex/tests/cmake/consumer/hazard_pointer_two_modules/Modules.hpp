// The interface between the executable and the two libraries of the fixture.
#ifndef LUMEX_TESTS_HAZARD_POINTER_TWO_MODULES_HPP
#define LUMEX_TESTS_HAZARD_POINTER_TWO_MODULES_HPP

#include <atomic>
#include <cstddef>

#if defined(_WIN32)
#if defined(HP_FIXTURE_BUILDING_holder_module)                                \
    || defined(HP_FIXTURE_BUILDING_retiring_module)
#define HP_FIXTURE_API __declspec (dllexport)
#else
#define HP_FIXTURE_API __declspec (dllimport)
#endif
#else
#define HP_FIXTURE_API __attribute__ ((visibility ("default")))
#endif

#include "lumex/core/hazard_pointer/LumexHazardPointer"

namespace fixture
{
// A protectable object that counts its deletions in a counter the test owns.
struct item : lumex::core::hazard_pointer::hazard_pointer_obj_base<item>
{
  explicit item (std::atomic<int> *the_counter) : counter (the_counter) {}
  ~item () { counter->fetch_add (1); }
  std::atomic<int> *counter;
};

// holder_module: owns a hazard pointer in its own static storage.
HP_FIXTURE_API void hold (std::atomic<item *> &source);
HP_FIXTURE_API void let_go ();
HP_FIXTURE_API std::size_t records_seen_by_holder ();

// retiring_module: retires objects and runs passes.
HP_FIXTURE_API void retire_item (item *object);
HP_FIXTURE_API void clean_up_here ();
HP_FIXTURE_API std::size_t records_seen_by_retirer ();
} // namespace fixture

#endif
