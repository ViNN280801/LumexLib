#ifndef LUMEX_EXPORT_HPP
#define LUMEX_EXPORT_HPP

#ifdef __cplusplus
  #define LUMEX_EXTERN_C_BEGIN extern "C" {
  #define LUMEX_EXTERN_C_END   }
#else
  #define LUMEX_EXTERN_C_BEGIN
  #define LUMEX_EXTERN_C_END
#endif

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
  #ifdef _MSC_VER // MSVC compiler
    #ifdef LUMEX_EXPORTS
      #define LUMEX_API __declspec(dllexport)
    #else
      #define LUMEX_API __declspec(dllimport)
    #endif
  #else
    // MinGW or other Windows compilers
    #ifdef LUMEX_EXPORTS
      #define LUMEX_API __attribute__((dllexport))
    #else
      #define LUMEX_API __attribute__((dllimport))
    #endif
  #endif
#else
  // UNIX
  #ifdef LUMEX_EXPORTS
    #define LUMEX_API __attribute__((visibility("default")))
  #else
    #define LUMEX_API
  #endif
#endif

#ifdef LUMEX_IMPLEMENTATION
  #if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__) && defined(_MSC_VER)
    // Force C linkage for compatibility with legacy applications
    #define LUMEX_PUBLIC_C_API extern "C" __declspec(dllexport)
    #define LUMEX_PUBLIC_API __declspec(dllexport)
  #elif _WIN32
    #define LUMEX_PUBLIC_C_API extern "C" __attribute__((dllexport))
    #define LUMEX_PUBLIC_API __attribute__((dllexport))
  #else
    #define LUMEX_PUBLIC_C_API extern "C" __attribute__((visibility("default")))
    #define LUMEX_PUBLIC_API __attribute__((visibility("default")))
  #endif
#else
  /// Macros for marking functions that should be available from outside.
  #define LUMEX_PUBLIC_C_API extern "C" LUMEX_API
  #define LUMEX_PUBLIC_API LUMEX_API
#endif

#endif // !LUMEX_EXPORT_HPP
