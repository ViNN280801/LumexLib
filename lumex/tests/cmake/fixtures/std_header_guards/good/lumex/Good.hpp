// Includes of newer standard headers, each under a guard that requires its
// standard: the forms the library uses.
#include <memory>
#include <vector>

#if __cplusplus > 201703L && defined(__has_include)
#if __has_include(<compare>)
#include <compare>
#endif
#endif

#if __cplusplus >= 201703L
#include <optional>
#include <string_view>
#endif

#if __cplusplus >= 202002L && defined(__cpp_lib_span)                         \
    && __cpp_lib_span >= 202002L
#include <span>
#endif

#if defined(__cpp_lib_source_location)
#include <source_location>
#endif

#if LUMEX_HAS_STD_EXPECTED
#include <expected>
#endif

#if _MSVC_LANG > 201402L
#include <variant>
#endif

#if defined(__has_include)
#if __cplusplus > 201703L
#include <bit>
#endif
#endif
