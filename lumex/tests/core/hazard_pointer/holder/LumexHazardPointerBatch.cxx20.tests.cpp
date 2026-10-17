// The batch functions over std::span: the module's span converts from it. The
// std::span tests need the standard library's <span> (LUMEX_HAS_STD_SPAN;
// libstdc++ 8 has none even with -std=c++2a), the range test does not.

#include <vector>
#if defined(__has_include)
#if __has_include(<span>)
#include <span>
#endif
#endif

#include <gtest/gtest.h>

#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"
#include "lumex/tests/core/hazard_pointer/LumexHazardPointerTestSupport.hpp"

namespace
{
namespace hp = lumex::core::hazard_pointer;
} // namespace

// Needs <span> (not in GCC 8 -std=c++2a)
#if LUMEX_HAS_STD_SPAN
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
#endif // LUMEX_HAS_STD_SPAN

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
