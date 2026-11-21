// LumexBase64Literal.cxx11.tests.cpp
//
// encoder, decoder and validator are final classes of static functions with
// no declared special member (todo 108): at every standard of the build they
// are empty, trivially destructible literal types, and a constexpr object of
// each exists. A user-declared constructor or destructor on one of them would
// make it a non-trivial type and show here. This file stands in the decode
// directory for the three classes, which share the suite of the module.
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/core/base64/LumexBase64"

namespace
{
namespace b64 = lumex::core::base64;

template <typename T> struct is_stateless_literal
{
  static constexpr bool value
      = std::is_empty<T>::value && std::is_trivially_destructible<T>::value
        && std::is_trivially_default_constructible<T>::value
        && std::is_trivially_copyable<T>::value;
};

constexpr b64::encode::encoder k_encoder{};
constexpr b64::decode::decoder k_decoder{};
constexpr b64::validate::validator k_validator{};

static_assert (is_stateless_literal<b64::encode::encoder>::value, "encoder");
static_assert (is_stateless_literal<b64::decode::decoder>::value, "decoder");
static_assert (is_stateless_literal<b64::validate::validator>::value,
               "validator");
} // namespace

TEST (LumexBase64LiteralTest, ConstantObjects_AreUsableAtRunTime)
{
  static_cast<void> (k_encoder);
  static_cast<void> (k_decoder);
  static_cast<void> (k_validator);
  EXPECT_TRUE (b64::validate::validator::is_valid_base64 ("SGk=", 4));
}
