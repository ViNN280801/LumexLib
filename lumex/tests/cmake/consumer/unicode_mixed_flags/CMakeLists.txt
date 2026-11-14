# Consumer fixture for cmake.consumer_unicode_mixed_flags_*.
#
# One program of four units: main and plain_unit.cpp with the options of the
# program, isa_unit.cpp with -mavx2 (/arch:AVX2: the compiler may encode every
# instruction of it, the ones it takes from the header too, with VEX) and
# no_simd_unit.cpp with LUMEX_UNICODE_UTF8_VALIDATE_NO_SIMD. All include the UTF-8
# validation. The entities of the header live in an inline namespace named after
# the mode and after the widest instruction set that the options of the unit
# enable, so the linker has no copy to replace by another. main checks, on any
# CPU and without an emulator, that the SSSE3 routine of the AVX2 unit and that
# of the plain unit are two functions (one copy for both would have one
# address), that every unit selected the routine it must, and that the results
# agree; the no_simd unit stays scalar when the SIMD unit is linked first.
# On Linux the cases `*_without_avx` also run the executable under
# qemu-x86_64 -cpu Conroe (RUN_PREFIX, which stands between `cmake -E env` and
# the executable, so that qemu emulates the fixture), and the test names the
# variant that this CPU must get in LUMEX_FIXTURE_EXPECT_VARIANT: a case that
# ran on the host would see AVX2 and fail. A plain unit that ran a copy with VEX
# encodings (a key without the instruction set) dies there with SIGILL.
# -DMIX_ORDER=ISA_FIRST|PLAIN_FIRST sets the link order, -O0 (/Od on MSVC,
# which ignores __forceinline there) keeps the compiler from inlining by itself.

cmake_minimum_required(VERSION 3.16)
project(LumexUnicodeMixedFlags LANGUAGES CXX)

if(NOT LUMEX_SOURCE_DIR)
    message(FATAL_ERROR "pass -DLUMEX_SOURCE_DIR=<LumexLib checkout>")
endif()
if(NOT MIX_ORDER)
    set(MIX_ORDER ISA_FIRST)
endif()
if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64|x86|i[3-6]86)$")
    message("LUMEX_CONSUMER_SKIP: the fixture is about the x86 routines")
    return()
endif()

if(MIX_ORDER STREQUAL "ISA_FIRST")
    set(_sources isa_unit.cpp plain_unit.cpp no_simd_unit.cpp main.cpp)
else()
    set(_sources main.cpp no_simd_unit.cpp plain_unit.cpp isa_unit.cpp)
endif()
add_executable(unicode_mixed_flags_consumer ${_sources})
target_include_directories(unicode_mixed_flags_consumer PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}" "${LUMEX_SOURCE_DIR}")
set_target_properties(unicode_mixed_flags_consumer PROPERTIES
    CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
if(MSVC)
    target_compile_options(unicode_mixed_flags_consumer PRIVATE /Od /Zc:__cplusplus)
    set_source_files_properties(isa_unit.cpp PROPERTIES COMPILE_OPTIONS /arch:AVX2)
else()
    target_compile_options(unicode_mixed_flags_consumer PRIVATE -O0)
    set_source_files_properties(isa_unit.cpp PROPERTIES COMPILE_OPTIONS -mavx2)
endif()
