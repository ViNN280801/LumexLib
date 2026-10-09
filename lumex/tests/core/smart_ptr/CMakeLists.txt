# lumex/tests/core/smart_ptr/CMakeLists.txt

# The tests follow the source tree (lumex/core/smart_ptr):
#   ctl/      split_counter, the control block ctl_base and its kinds
#   detail/   the traits, the allocator, detail::access, the
#             enable_shared_from_this probes
#   shared/   shared_ptr: construction, modifiers, observers, deleters,
#             casts, enable_shared_from_this, arrays (C++17), the three-way
#             comparison (C++20), the multithreaded and ABA tests
#   weak/     weak_ptr and owner_less
#   make/     make_shared and allocate_shared
#   interop/  from_std and to_std
#   this one  LumexSmartPtrGlobalNames.cxx11.tests.cpp (the module declares
#             nothing at global scope), and the headers the directories
#             share: LumexSmartPtrTestSupport.hpp (the two "families" of the
#             differential scenarios, the test types, the counting
#             allocator and deleters) and LumexSmartPtrTestScenarios.hpp (the
#             concurrent checkers of the stress, ABA and soak tests)
# Every directory has one suite per standard of the module (C++11, 14, 17, 20;
# lumex/tests/LumexTestStandards.cmake). The module is header-only, so a
# suite compiles it at its own standard. Every behavior the standard fixes is
# also run on std::shared_ptr / std::weak_ptr and the observations are
# compared (the differential oracle). Every test carries the label `tsan`: the
# ThreadSanitizer run is a separate build tree
#   cmake -S . -B build-tsan -G Ninja -DCMAKE_BUILD_TYPE=Debug \
#         -DCMAKE_CXX_COMPILER=/opt/llvm-23.1.0/bin/clang++ \
#         -DLUMEX_BUILD_TESTS=ON -DLUMEX_USE_TSAN=ON
#   ctest --test-dir build-tsan -L tsan -j4
# (GCC's ThreadSanitizer needs `setarch x86_64 -R ctest ...` on a kernel that
# randomizes mappings above its shadow range). The soak run is the test named
# *Soak*: left out of the default registration, registered under the label
# `soak` by -DLUMEX_BUILD_SOAK_TESTS=ON, or run directly with
# LUMEX_TEST_SOAK=1 and LUMEX_TEST_SOAK_SECONDS. The rejected uses
# (unique_ptr-like misuse, std::enable_shared_from_this classes, arrays of
# make_shared) are the compile checks of
# lumex/tests/cmake/consumer/smart_ptr_compile_checks.
lumex_add_standard_suites(SmartPtr
    MODULE smart_ptr
    LINK lumex::smart_ptr
    SOURCES LumexSmartPtrTestSupport.hpp LumexSmartPtrTestScenarios.hpp
    DISCOVER_ARGS PROPERTIES TIMEOUT 300 LABELS tsan
)

add_subdirectory(ctl)
add_subdirectory(detail)
add_subdirectory(shared)
add_subdirectory(weak)
add_subdirectory(make)
add_subdirectory(interop)
