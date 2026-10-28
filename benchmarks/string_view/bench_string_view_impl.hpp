// bench_string_view_impl.hpp
// Selects the string view under test: BENCH_STRING_VIEW_IMPL is 1 (the view of
// this library, lumex_string_view), 2 (std::string_view of the standard
// library in use) or 3 (boost::string_view of Boost.Utility). The scenarios of
// bench_string_view.cpp and the translation unit of the compile-time table
// are written once against the names defined here, so every implementation
// runs the same source.
#ifndef LUMEX_BENCHMARKS_STRING_VIEW_IMPL_HPP
#define LUMEX_BENCHMARKS_STRING_VIEW_IMPL_HPP

#include <cstddef>
#include <functional>
#include <string>

#define BENCH_STRING_VIEW_LUMEX 1
#define BENCH_STRING_VIEW_STD 2
#define BENCH_STRING_VIEW_BOOST 3

#ifndef BENCH_STRING_VIEW_IMPL
#error "define BENCH_STRING_VIEW_IMPL to 1 (lumex), 2 (std) or 3 (boost)"
#endif

#if __cplusplus >= 201703L
#include <string_view>
#define BENCH_HAS_STD_VIEW 1
#else
#define BENCH_HAS_STD_VIEW 0
#endif

#if BENCH_STRING_VIEW_IMPL == BENCH_STRING_VIEW_LUMEX

#include "lumex/core/string_view/LumexStringView"

namespace bench
{
using view = lumex_string_view;
using wview = lumex_wstring_view;
#define BENCH_STRING_VIEW_IMPL_NAME "lumex"
// The view has std::hash on every standard (a deterministic FNV-1a below
// C++17, the hash of std::string_view from C++17).
#define BENCH_HAS_HASH 1
inline std::size_t
hash_of (view const &v)
{
  return std::hash<view> () (v);
}
#if BENCH_HAS_STD_VIEW
#define BENCH_HAS_STD_CONVERSION 1
#else
#define BENCH_HAS_STD_CONVERSION 0
#endif
} // namespace bench

#elif BENCH_STRING_VIEW_IMPL == BENCH_STRING_VIEW_STD

#include <string_view>

namespace bench
{
using view = std::string_view;
using wview = std::wstring_view;
#define BENCH_STRING_VIEW_IMPL_NAME "std"
#define BENCH_HAS_HASH 1
#define BENCH_HAS_STD_CONVERSION 1
inline std::size_t
hash_of (view const &v)
{
  return std::hash<std::string_view> () (v);
}
} // namespace bench

#elif BENCH_STRING_VIEW_IMPL == BENCH_STRING_VIEW_BOOST

#include <boost/container_hash/hash.hpp>
#include <boost/utility/string_view.hpp>

namespace bench
{
using view = boost::string_view;
using wview = boost::wstring_view;
#define BENCH_STRING_VIEW_IMPL_NAME "boost"
#define BENCH_HAS_HASH 1
// boost::string_view neither converts to nor is built from std::string_view.
#define BENCH_HAS_STD_CONVERSION 0
inline std::size_t
hash_of (view const &v)
{
  return boost::hash<boost::string_view> () (v);
}
} // namespace bench

#else
#error "BENCH_STRING_VIEW_IMPL must be 1, 2 or 3"
#endif

#endif // !LUMEX_BENCHMARKS_STRING_VIEW_IMPL_HPP
