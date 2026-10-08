// hazard_pointer_obj_base with a captureless lambda as the deleter: its type
// is default constructible from C++20, which the deleter requirement needs.

#include <atomic>
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/tests/core/hazard_pointer/LumexHazardPointerTestSupport.hpp"

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
