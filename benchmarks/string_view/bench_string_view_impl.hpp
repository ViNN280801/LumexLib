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

#if defined(BENCH_LUMEX_UNITY)
// The compiled part of the view (what LumexCore_string_view contains) is built
// into this translation unit: the same code, but without the call across the
// library boundary and visible to the optimizer. It shows what the compiled
// part costs; the executable does not link the library.
#include "lumex/core/string_view/view/LumexStringView.cpp"
#include "lumex/core/string_view/view/LumexWStringView.cpp"
#endif

namespace bench
{
typedef lumex_string_view view;
typedef lumex_wstring_view wview;
#define BENCH_STRING_VIEW_IMPL_NAME "lumex"
// The view has no std::hash: from C++17 it is hashed by converting it.
#if BENCH_HAS_STD_VIEW
#define BENCH_HAS_HASH 1
#define BENCH_HAS_STD_CONVERSION 1
inline std::size_t
hash_of (view const &v)
{
  return std::hash<std::string_view> () (static_cast<std::string_view> (v));
}
#else
#define BENCH_HAS_HASH 0
#define BENCH_HAS_STD_CONVERSION 0
#endif
} // namespace bench

#elif BENCH_STRING_VIEW_IMPL == BENCH_STRING_VIEW_STD

#include <string_view>

namespace bench
{
typedef std::string_view view;
typedef std::wstring_view wview;
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
typedef boost::string_view view;
typedef boost::wstring_view wview;
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
