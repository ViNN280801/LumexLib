// Three MSVC-only pragmas that GCC reports as unknown: guarded by _WIN32
// (MinGW defines it too), in the #else branch of _MSC_VER, and in a
// condition that does not need _MSC_VER.
#ifdef _WIN32
#pragma warning(push)
#endif

#if defined(_MSC_VER)
#else
#pragma comment(lib, "x.lib")
#endif

#if defined(_MSC_VER) || defined(_WIN32)
#pragma warning(pop)
#endif
