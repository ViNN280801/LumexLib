// Five includes of newer standard headers that MSVC reports with STL4038 at
// an older standard: guarded by __has_include only (LumexSharedPtr.hpp before
// 2.0.3.0), not guarded, guarded by a standard that is too old, in the #else
// branch of the standard check, and in a condition with ||.
#if defined(__has_include)
#if __has_include(<compare>)
#include <compare>
#endif
#endif

#include <optional>

#if __cplusplus >= 201703L
#include <span>
#endif

#if __cplusplus >= 202302L
#else
#include <expected>
#endif

#if __cplusplus > 201703L || defined(_MSC_VER)
#include <bit>
#endif
