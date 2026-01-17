#ifndef LUMEX_KEYWORDS_HPP
#define LUMEX_KEYWORDS_HPP

#if __cplusplus >= 202002L
  #define LUMEX_CONSTINIT constinit
#elif __cplusplus >= 201103L
  #define LUMEX_CONSTINIT constexpr
#else
  #define LUMEX_CONSTINIT
#endif

#if __cplusplus >= 201703L
  #define LUMEX_INLINE_VARIABLE inline
#else
  #define LUMEX_INLINE_VARIABLE
#endif

#if __cplusplus >= 201103L
  #define LUMEX_NOEXCEPT_FUNCTION noexcept
  #define LUMEX_NOEXCEPT_FUNCTION_CONDITIONAL(cond) noexcept(cond)
#else
  #define LUMEX_NOEXCEPT_FUNCTION
  #define LUMEX_NOEXCEPT_FUNCTION_CONDITIONAL(cond)
#endif

#if defined(_MSC_VER)
  #define LUMEX_RESTRICT __restrict
#elif defined(__GNUC__) || defined(__clang__)
  #define LUMEX_RESTRICT __restrict__
#else
  #define LUMEX_RESTRICT
#endif

#if __cplusplus >= 202303L
  // C++23 and newer has relaxed constraints for constexpr for all of these
  #define LUMEX_CONSTEXPR_FUNCTION constexpr
  #define LUMEX_CONSTEXPR_VIRTUAL_FUNCTION constexpr
  #define LUMEX_CONSTEXPR_CTOR constexpr
  #define LUMEX_CONSTEXPR_DEFAULTED_CTOR constexpr
  #define LUMEX_CONSTEXPR_DTOR constexpr
  #define LUMEX_CONSTEXPR_DEFAULTED_DTOR constexpr
  #define LUMEX_CONSTEXPR_VIRTUAL_DTOR constexpr
  #define LUMEX_CONSTEXPR_DEFAULTED_VIRTUAL_DTOR constexpr
#elif __cplusplus == 202002L
  // C++20: Virtual functions constexpr, but NOT virtual destructors
  #define LUMEX_CONSTEXPR_FUNCTION constexpr
  #define LUMEX_CONSTEXPR_VIRTUAL_FUNCTION constexpr
  #define LUMEX_CONSTEXPR_CTOR constexpr
  #define LUMEX_CONSTEXPR_DEFAULTED_CTOR
  #define LUMEX_CONSTEXPR_DTOR constexpr
  #define LUMEX_CONSTEXPR_DEFAULTED_DTOR
  #define LUMEX_CONSTEXPR_VIRTUAL_DTOR
  #define LUMEX_CONSTEXPR_DEFAULTED_VIRTUAL_DTOR
#elif __cplusplus == 201703L
  // C++17: Base constexpr capabilities
  #define LUMEX_CONSTEXPR_FUNCTION constexpr
  #define LUMEX_CONSTEXPR_VIRTUAL_FUNCTION
  #define LUMEX_CONSTEXPR_CTOR constexpr
  #define LUMEX_CONSTEXPR_DEFAULTED_CTOR
  #define LUMEX_CONSTEXPR_DTOR constexpr
  #define LUMEX_CONSTEXPR_DEFAULTED_DTOR
  #define LUMEX_CONSTEXPR_VIRTUAL_DTOR
  #define LUMEX_CONSTEXPR_DEFAULTED_VIRTUAL_DTOR
#elif __cplusplus == 201103L || __cplusplus == 201402L
  // C++11 and C++14: Very limited constexpr (only simple functions, no defaulted special members)
  #define LUMEX_CONSTEXPR_FUNCTION constexpr
  #define LUMEX_CONSTEXPR_VIRTUAL_FUNCTION
  #define LUMEX_CONSTEXPR_CTOR constexpr
  #define LUMEX_CONSTEXPR_DEFAULTED_CTOR
  #define LUMEX_CONSTEXPR_DTOR
  #define LUMEX_CONSTEXPR_DEFAULTED_DTOR
  #define LUMEX_CONSTEXPR_VIRTUAL_DTOR
  #define LUMEX_CONSTEXPR_DEFAULTED_VIRTUAL_DTOR
#else
  // All the lower standards are not supported `constexpr` for these features, because `constexpr` was introduced in
  // C++11
  #define LUMEX_CONSTEXPR_FUNCTION
  #define LUMEX_CONSTEXPR_VIRTUAL_FUNCTION
  #define LUMEX_CONSTEXPR_CTOR
  #define LUMEX_CONSTEXPR_DEFAULTED_CTOR
  // C++ doesn't support concept of virtual ctor, so, skipping it
  #define LUMEX_CONSTEXPR_DEFAULTED_VIRTUAL_DTOR
  #define LUMEX_CONSTEXPR_DTOR
  #define LUMEX_CONSTEXPR_DEFAULTED_DTOR
  #define LUMEX_CONSTEXPR_VIRTUAL_DTOR
  #define LUMEX_CONSTEXPR_DEFAULTED_VIRTUAL_DTOR
#endif

#endif // !LUMEX_KEYWORDS_HPP
