// bench_expected_impl.hpp
// Selects the result type under test: BENCH_EXPECTED_IMPL is 1 (the expected
// of this library), 2 (the std::expected of the standard library in use) or 3
// (boost::outcome_v2::result of Boost.Outcome). The scenarios of
// bench_expected.cpp and the translation unit of the compile-time table are
// written once against the names defined here, so every implementation runs
// the same source.
//
// The names:
//   bench::expected<T, E>   the type under test
//   bench::ok<X> (args...)  a value of type X holding a success built in place
//   bench::err<X> (args...) a value of type X holding an error built in place
//   bench::ok_void<X> ()    the success of an expected<void, E>
//   bench::deref (x)        the unchecked access to the value
//   BENCH_EXPECTED_HAS_MONADIC, BENCH_EXPECTED_HAS_VALUE_OR, ..._HAS_VOID
//                           what the type offers (scenarios that need a
//                           feature the type lacks are skipped, not emulated)
#ifndef LUMEX_BENCHMARKS_EXPECTED_IMPL_HPP
#define LUMEX_BENCHMARKS_EXPECTED_IMPL_HPP

#include <utility>

#define BENCH_EXPECTED_LUMEX 1
#define BENCH_EXPECTED_STD 2
#define BENCH_EXPECTED_OUTCOME 3

#ifndef BENCH_EXPECTED_IMPL
#error "define BENCH_EXPECTED_IMPL to 1 (lumex), 2 (std) or 3 (outcome)"
#endif

#if BENCH_EXPECTED_IMPL == BENCH_EXPECTED_LUMEX

#include "lumex/core/expected/Expected"

namespace bench
{
template <typename T, typename E>
using expected = lumex::core::expected::result::expected<T, E>;

template <typename X, typename... Args>
X
ok (Args &&...args)
{
  return X (lumex::core::expected::result::in_place,
            std::forward<Args> (args)...);
}

template <typename X, typename... Args>
X
err (Args &&...args)
{
  return X (lumex::core::expected::result::unexpect,
            std::forward<Args> (args)...);
}

template <typename X>
inline auto
deref (X &&x) -> decltype (*std::forward<X> (x))
{
  return *std::forward<X> (x);
}

/** The success of an expected<void, E>. */
template <typename X>
X
ok_void ()
{
  return X ();
}

#define BENCH_EXPECTED_IMPL_NAME "lumex"
#define BENCH_EXPECTED_HAS_MONADIC 1
#define BENCH_EXPECTED_HAS_VALUE_OR 1
#define BENCH_EXPECTED_HAS_VOID 1
} // namespace bench

#elif BENCH_EXPECTED_IMPL == BENCH_EXPECTED_STD

#include <expected>

namespace bench
{
template <typename T, typename E> using expected = std::expected<T, E>;

template <typename X, typename... Args>
X
ok (Args &&...args)
{
  return X (std::in_place, std::forward<Args> (args)...);
}

template <typename X, typename... Args>
X
err (Args &&...args)
{
  return X (std::unexpect, std::forward<Args> (args)...);
}

template <typename X>
inline auto
deref (X &&x) -> decltype (*std::forward<X> (x))
{
  return *std::forward<X> (x);
}

/** The success of an expected<void, E>. */
template <typename X>
X
ok_void ()
{
  return X ();
}

#define BENCH_EXPECTED_IMPL_NAME "std"
#define BENCH_EXPECTED_HAS_MONADIC 1
#define BENCH_EXPECTED_HAS_VALUE_OR 1
#define BENCH_EXPECTED_HAS_VOID 1
} // namespace bench

#elif BENCH_EXPECTED_IMPL == BENCH_EXPECTED_OUTCOME

// Boost.Outcome, header-only. `result<T, E, policy::terminate>`: value ()
// and error () on the wrong state check and terminate (the default policy of
// an arbitrary E would be the one that throws or is unchecked depending on
// E; terminate is the closest to the checks of this library's operator* and
// error ()). Outcome has no value_or, no monadic members and no
// `operator*`; `assume_value ()` is its unchecked access, used for deref.
// When the value type and the error type are the same type Outcome does not
// allow in_place_type, so ok () and err () then use success () and failure ()
// (one extra move of the argument).
#include <boost/outcome/result.hpp>
// terminate.hpp needs the declarations of result.hpp before it.
#include <boost/outcome/policy/terminate.hpp>

#include <type_traits>

namespace bench
{
template <typename T, typename E>
using expected
    = boost::outcome_v2::basic_result<T, E,
                                      boost::outcome_v2::policy::terminate>;

template <typename X, typename... Args>
X
ok_impl (std::false_type /* value and error types differ */, Args &&...args)
{
  return X (boost::outcome_v2::in_place_type<typename X::value_type>,
            std::forward<Args> (args)...);
}

template <typename X, typename Arg>
X
ok_impl (std::true_type /* same type */, Arg &&arg)
{
  return X (boost::outcome_v2::success (std::forward<Arg> (arg)));
}

template <typename X, typename... Args>
X
err_impl (std::false_type, Args &&...args)
{
  return X (boost::outcome_v2::in_place_type<typename X::error_type>,
            std::forward<Args> (args)...);
}

template <typename X, typename Arg>
X
err_impl (std::true_type, Arg &&arg)
{
  return X (boost::outcome_v2::failure (std::forward<Arg> (arg)));
}

template <typename X, typename... Args>
X
ok (Args &&...args)
{
  return ok_impl<X> (
      std::is_same<typename X::value_type, typename X::error_type> (),
      std::forward<Args> (args)...);
}

template <typename X, typename... Args>
X
err (Args &&...args)
{
  return err_impl<X> (
      std::is_same<typename X::value_type, typename X::error_type> (),
      std::forward<Args> (args)...);
}

template <typename X>
inline auto
deref (X &&x) -> decltype (std::forward<X> (x).assume_value ())
{
  return std::forward<X> (x).assume_value ();
}

/** The success of a result<void, E> (Outcome deletes the default
 * constructor). */
template <typename X>
X
ok_void ()
{
  return X (boost::outcome_v2::success ());
}

#define BENCH_EXPECTED_IMPL_NAME "outcome"
#define BENCH_EXPECTED_HAS_MONADIC 0
#define BENCH_EXPECTED_HAS_VALUE_OR 0
#define BENCH_EXPECTED_HAS_VOID 1
} // namespace bench

#else
#error "BENCH_EXPECTED_IMPL must be 1, 2 or 3"
#endif

#endif // LUMEX_BENCHMARKS_EXPECTED_IMPL_HPP
