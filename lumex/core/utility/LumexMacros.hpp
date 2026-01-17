#ifndef LUMEX_MACROS_HPP
#define LUMEX_MACROS_HPP

#if defined(_WIN32) || defined(__WIN32__) || defined(__WIN64__) || defined(__MINGW32__) || defined(__MINGW64__)
  #define LUMEX_FUNCTION_NAME __FUNCSIG__
#elif defined(__linux__) || defined(__APPLE__) || defined(__MACH__) || defined(__MACOS__) || defined(__GNUC__)         \
  || defined(__clang__)
  #define LUMEX_FUNCTION_NAME __PRETTY_FUNCTION__
#elif defined(__ICC) || defined(__INTEL_COMPILER)
  #define LUMEX_FUNCTION_NAME __FUNCTION__
#else
  #define LUMEX_FUNCTION_NAME __FUNCTION__
#endif

#define LUMEX_STRINGIZE_DETAIL(x) #x
#define LUMEX_STRINGIZE(x) LUMEX_STRINGIZE_DETAIL(x)

#define LUMEX_CONCAT_DETAIL(x, y) x##y
#define LUMEX_CONCAT(x, y) LUMEX_CONCAT_DETAIL(x, y)

#endif // !LUMEX_MACROS_HPP
