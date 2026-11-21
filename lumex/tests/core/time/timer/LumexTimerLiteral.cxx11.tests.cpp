// LumexTimerLiteral.cxx11.tests.cpp
//
// lumex_timer has a trivial destructor at every standard of the build (todo
// 108). Its defaulted constructor is constexpr because the members have
// constexpr default initializers (time points, a flag), so it is a literal
// type from C++14: the standard makes the constructor of
// std::chrono::time_point constexpr from C++14 (libc++ follows it), only
// libstdc++ marks it constexpr already in C++11. The constexpr object is
// therefore built from C++14 and with libstdc++. A static_assert here stops
// the build when the constructor becomes user-provided and not constexpr.
// lumex_time has only static functions and no special member.
#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/core/time/LumexTime"

namespace
{
using lumex::core::time::timer::lumex_timer;

static_assert (std::is_trivially_destructible<lumex_timer>::value,
               "lumex_timer");

#if __cplusplus >= 201402L || defined(__GLIBCXX__)
#define LUMEX_TIMER_LITERAL_TEST_CONSTEXPR 1
#elif defined(_MSVC_LANG) && _MSVC_LANG >= 201402L
#define LUMEX_TIMER_LITERAL_TEST_CONSTEXPR 1
#endif

#ifdef LUMEX_TIMER_LITERAL_TEST_CONSTEXPR
constexpr lumex_timer k_timer{};
#endif
} // namespace

TEST (LumexTimerLiteralTest, ConstantTimer_IsNotStarted)
{
#ifdef LUMEX_TIMER_LITERAL_TEST_CONSTEXPR
  lumex_timer copy = k_timer;
#else
  lumex_timer copy;
#endif
  EXPECT_EQ (copy.elapsed_time_ms (), 0);
}
