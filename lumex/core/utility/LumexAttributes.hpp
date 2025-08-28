#ifndef LUMEX_ATTRIBUTES_HPP
#define LUMEX_ATTRIBUTES_HPP

// @link https://en.cppreference.com/w/cpp/language/attributes.html

// [[nodiscard]] and [[nodiscard("reason")]]
#if __cplusplus < 201703L
  #if defined(__GNUC__) || defined(__clang__)
    #define LUMEX_ATTRIBUTE_NODISCARD(msg) __attribute__((warn_unused_result)) // https://stackoverflow.com/questions/53169938/attribute-warn-unused-result-vs-attribute-warn-unused-result
  #elif defined(_MSC_VER)
    #define LUMEX_ATTRIBUTE_NODISCARD(msg) _Check_return_ // https://learn.microsoft.com/en-us/cpp/code-quality/annotating-function-behavior?view=msvc-170
  #else
    #define LUMEX_ATTRIBUTE_NODISCARD(msg)
  #endif
#elif __cplusplus == 201703L
  #define LUMEX_ATTRIBUTE_NODISCARD(msg) [[nodiscard]] // __cplusplus >= 201703L: https://en.cppreference.com/w/cpp/language/attributes/nodiscard
#else // __cplusplus >= 202002UL
  #define LUMEX_ATTRIBUTE_NODISCARD(msg) [[nodiscard(msg)]]
#endif

// --- Lumex Standard Attribute Macros ---

// [[noreturn]]
#if __cplusplus >= 201103L
  #define LUMEX_ATTRIBUTE_NORETURN [[noreturn]]
#elif defined(__GNUC__) || defined(__clang__)
  #define LUMEX_ATTRIBUTE_NORETURN __attribute__((noreturn))
#elif defined(_MSC_VER)
  #define LUMEX_ATTRIBUTE_NORETURN __declspec(noreturn)
#else
  #define LUMEX_ATTRIBUTE_NORETURN
#endif

// [[noinline]]
#if defined(__GNUC__) || defined(__clang__)
  #define LUMEX_ATTRIBUTE_NOINLINE __attribute__((noinline))
#elif defined(_MSC_VER)
  #define LUMEX_ATTRIBUTE_NOINLINE __declspec(noinline)
#else
  #define LUMEX_ATTRIBUTE_NOINLINE
#endif

// [[carries_dependency]]
#if __cplusplus >= 201103L
  #define LUMEX_ATTRIBUTE_CARRIES_DEPENDENCY [[carries_dependency]]
#elif defined(__GNUC__) || defined(__clang__)
  #define LUMEX_ATTRIBUTE_CARRIES_DEPENDENCY __attribute__((carries_dependency))
#elif defined(_MSC_VER)
  #define LUMEX_ATTRIBUTE_CARRIES_DEPENDENCY _CARRIES_DEPENDENCY_
#else
  #define LUMEX_ATTRIBUTE_CARRIES_DEPENDENCY
#endif

// [[deprecated]] and [[deprecated("reason")]]
#if __cplusplus >= 201402L
  #define LUMEX_ATTRIBUTE_DEPRECATED [[deprecated]]
  #define LUMEX_ATTRIBUTE_DEPRECATED_MSG(msg) [[deprecated(msg)]]
#elif defined(__GNUC__) || defined(__clang__)
  #define LUMEX_ATTRIBUTE_DEPRECATED __attribute__((deprecated))
  #define LUMEX_ATTRIBUTE_DEPRECATED_MSG(msg) __attribute__((deprecated(msg)))
#elif defined(_MSC_VER)
  #define LUMEX_ATTRIBUTE_DEPRECATED __declspec(deprecated)
  #define LUMEX_ATTRIBUTE_DEPRECATED_MSG(msg) __declspec(deprecated(msg))
#else
  #define LUMEX_ATTRIBUTE_DEPRECATED
  #define LUMEX_ATTRIBUTE_DEPRECATED_MSG(msg)
#endif

// [[fallthrough]]
#if __cplusplus >= 201703L
  #define LUMEX_ATTRIBUTE_FALLTHROUGH [[fallthrough]]
#elif (defined(__clang__) && __has_cpp_attribute(fallthrough)) || (defined(__GNUC__) && (__GNUC__ >= 7))
  #define LUMEX_ATTRIBUTE_FALLTHROUGH __attribute__((fallthrough))
#else
  #define LUMEX_ATTRIBUTE_FALLTHROUGH
#endif

// [[maybe_unused]]
#define LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR(var) (void)(var)
#if __cplusplus >= 201703L
  #define LUMEX_ATTRIBUTE_MAYBE_UNUSED [[maybe_unused]]
#elif defined(__GNUC__) || defined(__clang__)
  #define LUMEX_ATTRIBUTE_MAYBE_UNUSED __attribute__((unused))
#else
  #define LUMEX_ATTRIBUTE_MAYBE_UNUSED
#endif

// [[likely]] / [[unlikely]]
#if __cplusplus >= 202002L
  #define LUMEX_ATTRIBUTE_LIKELY [[likely]]
  #define LUMEX_ATTRIBUTE_UNLIKELY [[unlikely]]
  #define LUMEX_ATTRIBUTE_LIKELY_COND(cond) (cond)
  #define LUMEX_ATTRIBUTE_UNLIKELY_COND(cond) (cond)
#elif (defined(__GNUC__) && (__GNUC__ >= 9)) || (defined(__clang__) && __has_cpp_attribute(likely))
  #define LUMEX_ATTRIBUTE_LIKELY __attribute__((likely))
  #define LUMEX_ATTRIBUTE_UNLIKELY __attribute__((unlikely))

  #if (defined(__GNUC__))
    #define LUMEX_ATTRIBUTE_UNLIKELY_COND(cond) __builtin_expect(cond, 0)
  #else
    #define LUMEX_ATTRIBUTE_UNLIKELY_COND(cond) (cond)
  #endif
#else
  #define LUMEX_ATTRIBUTE_LIKELY
  #define LUMEX_ATTRIBUTE_UNLIKELY
  #define LUMEX_ATTRIBUTE_UNLIKELY_COND(cond) (cond)
#endif

// [[no_unique_address]] -> C++20
#if __cplusplus >= 202002L
  #define LUMEX_ATTRIBUTE_NO_UNIQUE_ADDRESS [[no_unique_address]]
#else
  #define LUMEX_ATTRIBUTE_NO_UNIQUE_ADDRESS
#endif

// [[assume(expr)]] -> C++23
#if __cplusplus >= 202302L
  #define LUMEX_ATTRIBUTE_ASSUME(expr) [[assume(expr)]]
#else
  #define LUMEX_ATTRIBUTE_ASSUME(expr)
#endif

// [[indeterminate]] -> C++26
#if __cplusplus >= 202602L
  #define LUMEX_ATTRIBUTE_INDETERMINATE [[indeterminate]]
#else
  #define LUMEX_ATTRIBUTE_INDETERMINATE
#endif

// [[optimize_for_synchronized]] -> TM TS
#define LUMEX_ATTRIBUTE_OPTIMIZE_FOR_SYNCHRONIZED

#endif // !LUMEX_ATTRIBUTES_HPP
