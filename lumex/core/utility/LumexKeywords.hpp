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
#else
  #define LUMEX_NOEXCEPT_FUNCTION
#endif

#if defined(_MSC_VER)
  #define LUMEX_RESTRICT __restrict
#elif defined(__GNUC__) || defined(__clang__)
  #define LUMEX_RESTRICT __restrict__
#else
  #define LUMEX_RESTRICT
#endif

#endif // !LUMEX_KEYWORDS_HPP
