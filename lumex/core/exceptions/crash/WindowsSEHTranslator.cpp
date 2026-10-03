#define LUMEX_IMPLEMENTATION
#include "WindowsSEHTranslator.hpp"
#include "lumex/core/utility/attr/LumexAttributes.hpp"

#if defined(LUMEX_OS_WINDOWS)
#include "LumexCrashHandler.hpp"

LUMEX_PUBLIC_API
void
seh_translator (LUMEX_ATTRIBUTE_MAYBE_UNUSED unsigned int code,
                _EXCEPTION_POINTERS *info)
{
  LumexCrashHandler::instance ()._handleSEHException (info);
  TerminateProcess (GetCurrentProcess (), code);
}
#endif // LUMEX_OS_WINDOWS
