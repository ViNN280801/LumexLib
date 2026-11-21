// LumexCrcCatalogLiteral.cxx11.tests.cpp
//
// The parameter record of the catalogue and the parametric engine declare no
// constructor or destructor by hand (todo 108): crc_params_t is a literal type
// at every standard of the build, so a constexpr record exists, and
// crc_parametric is a class of static functions that can neither be created,
// copied nor moved. A user-declared special member on either would show here.
// The record has default member initializers, so it is an aggregate from
// C++14 only and is built by value-initialization at C++11.
#include <cstdint>
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/core/crc/LumexCrc"

namespace
{
using lumex::core::crc::catalog::crc_params_t;
using lumex::core::crc::parametric::Crc16Arc;

constexpr crc_params_t k_zero{};
constexpr crc_params_t k_copy (k_zero);

static_assert (std::is_trivially_destructible<crc_params_t>::value,
               "crc_params_t is trivially destructible");
static_assert (std::is_trivially_copyable<crc_params_t>::value,
               "crc_params_t stays trivially copyable");
static_assert (k_zero.widthBits == 0 && k_zero.poly == 0u && k_zero.init == 0u
                   && !k_zero.refIn && !k_zero.refOut && k_zero.xorOut == 0u,
               "value-initialization zeroes every field");
static_assert (k_copy.widthBits == 0, "copy");

static_assert (!std::is_default_constructible<Crc16Arc>::value,
               "crc_parametric cannot be created");
static_assert (!std::is_copy_constructible<Crc16Arc>::value,
               "crc_parametric cannot be copied");
static_assert (!std::is_move_constructible<Crc16Arc>::value,
               "crc_parametric cannot be moved");
static_assert (std::is_trivially_destructible<Crc16Arc>::value,
               "crc_parametric is trivially destructible");
} // namespace

TEST (LumexCrcCatalogLiteralTest, ConstantRecord_AgreesWithTheRunTimeOne)
{
  crc_params_t const run_time{};
  EXPECT_EQ (run_time.widthBits, k_zero.widthBits);
  EXPECT_EQ (run_time.poly, k_zero.poly);
  EXPECT_EQ (run_time.xorOut, k_zero.xorOut);
}
