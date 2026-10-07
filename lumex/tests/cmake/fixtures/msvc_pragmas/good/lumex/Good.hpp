// The same pragmas, each under a condition that needs _MSC_VER.
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4251)
#pragma warning(pop)
#endif

#ifdef _MSC_VER
#pragma comment(lib, "x.lib")
#endif

#if defined(_WIN32) && defined(_MSC_VER)
#pragma comment(lib, "y.lib")
#endif

#if defined(_WIN32)
#include <windows.h>
#if defined(_MSC_VER)
#pragma comment(lib, "z.lib")
#endif
#endif
