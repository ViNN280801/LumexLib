// A program that still compiles below C++14 often declares std::make_unique
// and std::exchange itself, because the standard library has neither. The
// logger header used to declare both in namespace std below C++14 (undefined
// behavior), so such a program could not include the header: the two
// declarations were a redefinition of each other. The header declares nothing
// in namespace std now; this translation unit declares the two functions the
// way an old code base does, after the logger header, and calls them. Below
// C++14 the translation unit fails to compile when the header declares them
// again. From C++14 the standard library has the two functions and the tests
// call those.

#include <cstddef>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#include "lumex/applied/logger/logger/LumexLogger.hpp"

#if __cplusplus < 201402L
namespace
{
int polyfill_make_unique_calls = 0;
int polyfill_exchange_calls = 0;
} // namespace

namespace std
{
template <typename T, typename... Args>
typename enable_if<!is_array<T>::value, unique_ptr<T>>::type
make_unique (Args &&...args)
{
  ++polyfill_make_unique_calls;
  return unique_ptr<T> (new T (forward<Args> (args)...));
}

template <typename T, typename U = T>
T
exchange (T &object, U &&value)
{
  ++polyfill_exchange_calls;
  T old_value = move (object);
  object = forward<U> (value);
  return old_value;
}
} // namespace std
#endif

TEST (LumexLoggerStdPolyfill, ConsumerMakeUniqueAndExchangeCompileNextToLogger)
{
#if __cplusplus < 201402L
  polyfill_make_unique_calls = 0;
  polyfill_exchange_calls = 0;
#endif

  std::unique_ptr<std::string> text = std::make_unique<std::string> (3, 'x');
  ASSERT_NE (text, nullptr);
  EXPECT_EQ (*text, "xxx");

  int counter = 5;
  int const previous = std::exchange (counter, 9);
  EXPECT_EQ (previous, 5);
  EXPECT_EQ (counter, 9);

#if __cplusplus < 201402L
  // The consumer's functions were the ones called: the header declares no
  // std::make_unique and no std::exchange of its own.
  EXPECT_EQ (polyfill_make_unique_calls, 1);
  EXPECT_EQ (polyfill_exchange_calls, 1);
#endif
}
