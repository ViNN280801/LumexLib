#include "Modules.hpp"

namespace fixture
{
void
retire_item (item *object)
{
  object->retire ();
}

void
clean_up_here ()
{
  lumex::core::hazard_pointer::clean_up ();
}

std::size_t
records_seen_by_retirer ()
{
  return lumex::core::hazard_pointer::engine::statistics ().records;
}
} // namespace fixture
