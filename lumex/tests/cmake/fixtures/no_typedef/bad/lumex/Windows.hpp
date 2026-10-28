// A typedef in a branch the preprocessor does not take on this machine, and
// one whose keyword ends its line.
#if defined(_WIN32)
typedef unsigned long dword_t;
#endif

#ifdef LUMEX_FIXTURE_NEVER_DEFINED
// clang-format off
typedef
    unsigned long long split_t;
// clang-format on
#endif
