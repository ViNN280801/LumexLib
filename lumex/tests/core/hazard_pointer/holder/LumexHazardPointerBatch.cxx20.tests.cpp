// The batch functions over std::span: the module's span converts from it.

#include <span>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/core/hazard_pointer/LumexHazardPointerTestSupport.hpp"

namespace
{
namespace hp = lumex::core::hazard_pointer;
} // namespace

TEST (LumexHazardPointerBatchCxx20Test,
      GivenAStdSpan_WhenCallingTheBatchFunctions_ThenItConvertsAndWorks)
{
  std::vector<hp::hazard_pointer> holders (6);
  std::span<hp::hazard_pointer> view (holders);
  hp::make_hazard_pointer_batch (view);
  for (hp::hazard_pointer const &holder : holders)
    {
      EXPECT_FALSE (holder.empty ());
    }
  hp::clear_hazard_pointer_batch (view);
  for (hp::hazard_pointer const &holder : holders)
    {
      EXPECT_TRUE (holder.empty ());
    }
}

TEST (
    LumexHazardPointerBatchCxx20Test,
    GivenAStdVector_WhenCallingTheBatchFunctions_ThenTheRangeConstructorsApply)
{
  std::vector<hp::hazard_pointer> holders (3);
  hp::make_hazard_pointer_batch (holders);
  EXPECT_FALSE (holders[0].empty ());
  hp::clear_hazard_pointer_batch (holders);
  EXPECT_TRUE (holders[2].empty ());
}
