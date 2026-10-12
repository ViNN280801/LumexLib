#include <memory>

#include "Modules.hpp"

namespace
{
std::unique_ptr<lumex::core::hazard_pointer::hazard_pointer> g_holder;
}

namespace fixture
{
void
hold (std::atomic<item *> &source)
{
  g_holder.reset (new lumex::core::hazard_pointer::hazard_pointer (
      lumex::core::hazard_pointer::make_hazard_pointer ()));
  item *const held = g_holder->protect (source);
  static_cast<void> (held);
}

void
let_go ()
{
  g_holder.reset ();
}

std::size_t
records_seen_by_holder ()
{
  return lumex::core::hazard_pointer::engine::statistics ().records;
}
} // namespace fixture
