// hazard_pointer_obj_base with a captureless lambda as the deleter: its type
// is default constructible from C++20 (P0624), which the deleter requirement
// needs. GCC 8 at -std=c++2a (the C++20 draft) still deletes the default
// constructor of a closure type and has no feature-test macro for this change,
// so there the file is empty.

#include <atomic>
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/core/utility/compiler/LumexCheckCompiler.hpp"
#include "lumex/tests/core/hazard_pointer/LumexHazardPointerTestSupport.hpp"

// Needs default constructible captureless lambdas (not in GCC 8 -std=c++2a)
#if !LUMEX_GCC_BEFORE(9, 0)

namespace
{
namespace hp = lumex::core::hazard_pointer;

int g_lambda_deleted = 0;

constexpr auto lambda_deleter = [] (auto *object)
  {
    ++g_lambda_deleted;
    delete object;
  };

using lambda_deleter_t = std::remove_const_t<decltype (lambda_deleter)>;

struct lambda_node : hp::hazard_pointer_obj_base<lambda_node, lambda_deleter_t>
{
};
} // namespace

TEST (LumexHazardPointerObjBaseCxx20Test,
      GivenACaptureLessLambdaDeleter_WhenRetired_ThenItRuns)
{
  static_assert (std::is_default_constructible_v<lambda_deleter_t>);
  static_assert (std::is_empty_v<lambda_deleter_t>);
  static_assert (sizeof (lambda_node) == 2 * sizeof (void *),
                 "an empty lambda costs nothing");
  g_lambda_deleted = 0;
  (new lambda_node)->retire ();
  hp::clean_up ();
  EXPECT_EQ (g_lambda_deleted, 1);
}

#endif // !LUMEX_GCC_BEFORE(9, 0)
