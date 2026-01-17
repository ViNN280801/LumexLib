#ifndef LUMEX_LOGGING_MACRO_HPP
#define LUMEX_LOGGING_MACRO_HPP

#include "LumexLogging.hpp"

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

#define lumDebug(moduleName, ...) LumexLogging::debug(moduleName,  __VA_ARGS__)
#define lumInfo(moduleName, ...) LumexLogging::info(moduleName, __VA_ARGS__)
#define lumSuccess(moduleName, ...) LumexLogging::success(moduleName, __VA_ARGS__)
#define lumWarning(moduleName, ...) LumexLogging::warning(moduleName, __VA_ARGS__)
#define lumError(moduleName, ...) LumexLogging::error(moduleName, __VA_ARGS__)
#define lumCritical(moduleName, ...) LumexLogging::critical(moduleName, __VA_ARGS__)

#define lumDebugFL(moduleName, ...) LumexLogging::debug(moduleName, "[", __FILE__, ":", __LINE__, "] ", __VA_ARGS__)
#define lumInfoFL(moduleName, ...) LumexLogging::info(moduleName, "[", __FILE__, ":", __LINE__, "] ", __VA_ARGS__)
#define lumSuccessFL(moduleName, ...) LumexLogging::success(moduleName, "[", __FILE__, ":", __LINE__, "] ", __VA_ARGS__)
#define lumWarningFL(moduleName, ...) LumexLogging::warning(moduleName, "[", __FILE__, ":", __LINE__, "] ", __VA_ARGS__)
#define lumErrorFL(moduleName, ...) LumexLogging::error(moduleName, "[", __FILE__, ":", __LINE__, "] ", __VA_ARGS__)
#define lumCriticalFL(moduleName, ...) LumexLogging::critical(moduleName, "[", __FILE__, ":", __LINE__, "] ", __VA_ARGS__)

#endif // !LUMEX_LOGGING_MACRO_HPP
