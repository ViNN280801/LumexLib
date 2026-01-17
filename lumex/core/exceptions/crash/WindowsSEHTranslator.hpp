#ifndef WINDOWS_SEH_TRANSLATOR_HPP
#define WINDOWS_SEH_TRANSLATOR_HPP

#include "lumex/LumexExport.hpp"
#include "lumex/core/utility/LumexUtility"

#if LUMEX_OS_WINDOWS
  #include <Windows.h>
  #include <eh.h>

/**
 * @brief SEH translator function for Windows.
 * @param code The exception code.
 * @param info The exception information.
 */
LUMEX_PUBLIC_API
inline void seh_translator(LUMEX_ATTRIBUTE_MAYBE_UNUSED unsigned int code, _EXCEPTION_POINTERS *info);
#endif

#if LUMEX_OS_WINDOWS
  #define SET_SEH_TRANSLATOR _set_se_translator(seh_translator);
#else
  #define SET_SEH_TRANSLATOR
#endif

#endif // !WINDOWS_SEH_TRANSLATOR_HPP
