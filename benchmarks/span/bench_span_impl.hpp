// bench_span_impl.hpp
// Selects the span under test: BENCH_SPAN_IMPL is 1 (the span of this
// library), 2 (the std::span of the standard library in use) or 3
// (boost::span of Boost.Core). The scenarios of bench_span.cpp and the
// translation unit of the compile-time table are written once against the
// names defined here, so every implementation runs the same source.
#ifndef LUMEX_BENCHMARKS_SPAN_IMPL_HPP
#define LUMEX_BENCHMARKS_SPAN_IMPL_HPP

#include <cstddef>

#define BENCH_SPAN_LUMEX 1
#define BENCH_SPAN_STD 2
#define BENCH_SPAN_BOOST 3

#ifndef BENCH_SPAN_IMPL
#error "define BENCH_SPAN_IMPL to 1 (lumex), 2 (std) or 3 (boost)"
#endif

#if BENCH_SPAN_IMPL == BENCH_SPAN_LUMEX

#include "lumex/core/span/LumexSpan"

namespace bench
{
using lumex::core::span::view::as_bytes;
using lumex::core::span::view::byte;
using lumex::core::span::view::span;
constexpr std::size_t dynamic_extent = lumex::core::span::view::dynamic_extent;
#define BENCH_SPAN_IMPL_NAME "lumex"
#define BENCH_SPAN_HAS_AS_BYTES 1
} // namespace bench

#elif BENCH_SPAN_IMPL == BENCH_SPAN_STD

#include <span>

namespace bench
{
using std::as_bytes;
using std::byte;
using std::span;
constexpr std::size_t dynamic_extent = std::dynamic_extent;
#define BENCH_SPAN_IMPL_NAME "std"
#define BENCH_SPAN_HAS_AS_BYTES 1
} // namespace bench

#elif BENCH_SPAN_IMPL == BENCH_SPAN_BOOST

#include <boost/core/span.hpp>

namespace bench
{
using boost::span;
constexpr std::size_t dynamic_extent = boost::dynamic_extent;
#define BENCH_SPAN_IMPL_NAME "boost"
// boost::as_bytes exists only where std::byte does (C++17 and later).
#if defined(__cpp_lib_byte)
using boost::as_bytes;
using byte = std::byte;
#define BENCH_SPAN_HAS_AS_BYTES 1
#else
#define BENCH_SPAN_HAS_AS_BYTES 0
#endif
} // namespace bench

#else
#error "BENCH_SPAN_IMPL must be 1, 2 or 3"
#endif

#endif // !LUMEX_BENCHMARKS_SPAN_IMPL_HPP
