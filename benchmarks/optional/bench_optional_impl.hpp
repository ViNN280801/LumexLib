// bench_optional_impl.hpp
// Selects the optional under test: BENCH_OPTIONAL_IMPL is 1 (the optional of
// this library), 2 (std::optional of the standard library in use) or 3
// (boost::optional of Boost.Optional). The scenarios of bench_optional.cpp and
// the translation unit of the compile-time table are written once against the
// names defined here, so every implementation runs the same source.
#ifndef LUMEX_BENCHMARKS_OPTIONAL_IMPL_HPP
#define LUMEX_BENCHMARKS_OPTIONAL_IMPL_HPP

#define BENCH_OPTIONAL_LUMEX 1
#define BENCH_OPTIONAL_STD 2
#define BENCH_OPTIONAL_BOOST 3

#ifndef BENCH_OPTIONAL_IMPL
#error "define BENCH_OPTIONAL_IMPL to 1 (lumex), 2 (std) or 3 (boost)"
#endif

#if BENCH_OPTIONAL_IMPL == BENCH_OPTIONAL_LUMEX

#include "lumex/core/optional/LumexOptional"

namespace bench
{
template <typename T> using optional = lumex::core::optional::opt::optional<T>;
#define BENCH_OPTIONAL_IMPL_NAME "lumex"
#define BENCH_NULLOPT lumex::core::optional::opt::nullopt
#define BENCH_HAS_HASH 1
} // namespace bench

#elif BENCH_OPTIONAL_IMPL == BENCH_OPTIONAL_STD

#include <optional>

namespace bench
{
template <typename T> using optional = std::optional<T>;
#define BENCH_OPTIONAL_IMPL_NAME "std"
#define BENCH_NULLOPT std::nullopt
#define BENCH_HAS_HASH 1
} // namespace bench

#elif BENCH_OPTIONAL_IMPL == BENCH_OPTIONAL_BOOST

#include <boost/optional.hpp>

namespace bench
{
template <typename T> using optional = boost::optional<T>;
#define BENCH_OPTIONAL_IMPL_NAME "boost"
#define BENCH_NULLOPT boost::none
#define BENCH_HAS_HASH 0
} // namespace bench

#else
#error "BENCH_OPTIONAL_IMPL must be 1, 2 or 3"
#endif

#endif // !LUMEX_BENCHMARKS_OPTIONAL_IMPL_HPP
