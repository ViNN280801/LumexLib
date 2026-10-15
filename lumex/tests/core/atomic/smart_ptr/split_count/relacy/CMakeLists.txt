# lumex/tests/core/atomic/smart_ptr/split_count/relacy/CMakeLists.txt
#
# Relacy models of the memory orders of the split-count engine. Optional: the
# directory is added by ../CMakeLists.txt only when LUMEX_BUILD_RELACY is ON
# or LUMEX_RELACY_DIR is given. Relacy (https://github.com/dvyukov/relacy,
# Dmitry Vyukov's race detector for the C++11 memory model; header-only) is
# vendored in 3rdparty/relacy and used by default; LUMEX_RELACY_DIR names
# another checkout instead. With both unset the default build and the default
# test run do not change.
#
# With the models enabled they are ordinary CTest cases, so a plain
# `ctest` of that tree runs them too (about 10 minutes); they carry the labels
# relacy and relacy_mutant, and `ctest -LE relacy` leaves all of them out.
#
# How to run:
#   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
#         -DLUMEX_BUILD_TESTS=ON -DLUMEX_BUILD_RELACY=ON
#   ninja -C build LumexSplitCountRelacyBlock LumexSplitCountRelacyHolder \
#                  LumexSplitCountRelacyLitmus
#   ctest --test-dir build -L '^relacy$' --output-on-failure
# or run a program by hand, with a filter and another search:
#   build/bin/LumexSplitCountRelacyBlock --only load_store/k0 \
#         --search bound --bound 6
#
# Every program is built twice: with the counter of the library (a release
# drop and an acquire fence on zero) and with the counter that the library uses
# under ThreadSanitizer (an acq_rel drop and no fence), the *_tsan_counter
# cases. The fence of the first counter masks the success order of the pinned
# swap (mutation 9); the second does not.
#
# Mutants (an order of the engine weakened): configure with
# -DLUMEX_RELACY_MUTANTS=ON; every row of the table below becomes a program
# built with LUMEX_RELACY_MUTANT=<number> and a CTest case with the label
# relacy_mutant. A case marked "caught" passes only when the program prints a
# data race at one of the expected sites followed by the failure of the named
# model: a crash, a filter that matches nothing or another kind of failure does
# not count. A case marked "survives" passes only when the program ran and
# every model passed.
#
# The models are plain programs, not GoogleTest suites, and use no library
# header. Relacy builds at C++11 to C++20 (checked with GCC 13 and Clang 23);
# the models pass its debug-info argument explicitly, so they do not depend on
# the C++20 defaulting, and are pinned to C++17. Relacy runs the threads of a
# model as fibers of its own, so the sanitizer trees do not build them.

if(LUMEX_USE_ASAN OR LUMEX_USE_UBSAN OR LUMEX_USE_TSAN)
    message(STATUS "Skipping the Relacy models: they switch fibers themselves")
    return()
endif()

# Relacy includes <source_location> and needs a conforming atomics library:
# compile one of its headers with the include path and the standard that the
# models use, and skip the models if that fails.
include(CheckCXXSourceCompiles)
set(CMAKE_REQUIRED_INCLUDES "${LUMEX_RELACY_DIR}")
if(MSVC)
    set(CMAKE_REQUIRED_FLAGS "/std:c++17")
else()
    set(CMAKE_REQUIRED_FLAGS "-std=c++17")
endif()
check_cxx_source_compiles("
#include <source_location>
#include \"relacy/relacy.hpp\"
struct probe_t : rl::test_suite<probe_t, 1>
{
  void thread (unsigned) { rl::atomic<int> a; a.store (0, rl::mo_relaxed, $); }
};
int main () { return 0; }
" LUMEX_RELACY_COMPILES)
unset(CMAKE_REQUIRED_INCLUDES)
unset(CMAKE_REQUIRED_FLAGS)
if(NOT LUMEX_RELACY_COMPILES)
    message(STATUS "Skipping the Relacy models: the compiler cannot build Relacy at C++17")
    return()
endif()

set(_relacy_models Block Holder Litmus)

# One CTest case per model program and counter. The search of each program is
# in its main (the full and bounded searches are the long ones); the limit is
# in seconds.
set(_relacy_timeout 1800)

# The mutants: <number>|<program>|<filter>|<verdict>|<counter>|<sites>|<model>.
# The filter selects the models of the program by a substring of
# "<model>/k<reserve>"; the counter is "default" or "tsan"; the sites are the
# functions in which Relacy may report the race (comma separated), or "-" for a
# mutant of the logic of the reserve, which no race reveals: the program then
# must end with the failure of the named model (a violated check of the
# balance); the model is the first model that must fail.
#   8    L3 tick relaxed                           data race on the object
#   10   L7 un-tick relaxed                        data race on the holder
#   33   decision reads relaxed (load and swap)    ghost race: payment unordered
#   331  decision read of a load relaxed           ghost race
#   332  failure order of the swap relaxed         ghost race
#   101  no acquire fence after a drop to zero     data race in the disposal
#   102  drop relaxed (no release)                 data race in the disposal
#   103  S1 swap of a tick-free word relaxed       data race on the object
#   104  S2 writer pin relaxed                     data race on the holder
#   105  S10 and S12 swap relaxed                  data race
#   9    S4 success without acquire. With the counter of the library it
#        survives: the acquire fence of the disposing drop masks it. With the
#        counter of ThreadSanitizer (acq_rel drop, no fence) the second un-tick
#        of a loader that loads twice reaches the writer's disposal of the
#        holder only through the success order of the swap: data race.
#   25   rmw_order ignores seq_cst                 survives: see the litmus tests
#   201  compare-exchange: the surplus of the swap is one too big
#   202  compare-exchange: the retained deposit is not topped up
#   203  holder compare-exchange: the deposit is one short
set(_relacy_mutants
    "8|Block|block.publish_load|caught|default|load|block.publish_load"
    "10|Holder|holder.load_store|caught|default|finish|holder.load_store"
    "33|Block|block.load_store|caught|default|witness_read|block.load_store"
    "331|Block|block.load_store|caught|default|witness_read|block.load_store"
    "332|Block|two_writers|caught|default|witness_read|block.two_writers_load"
    "33|Block|block.load_reset|caught|default|witness_read|block.load_reset"
    "331|Block|block.load_reset|caught|default|witness_read|block.load_reset"
    "101|Block|block.load_store|caught|default|finish|block.load_store"
    "102|Block|block.load_store|caught|default|finish|block.load_store"
    "103|Block|block.publish_load|caught|default|load|block.publish_load"
    "104|Holder|holder.publish_load|caught|default|deposit|holder.publish_load"
    "105|Block|cas_load_store|caught|default|finish|block.cas_load_store"
    "105|Holder|holder.cas_load|caught|default|witness_read,finish|holder.cas_load"
    "201|Block|cas_load_store/k2|caught|default|-|block.cas_load_store"
    "202|Block|cas_load_store/k2|caught|default|-|block.cas_load_store"
    "203|Holder|cas_load/k2|caught|default|-|holder.cas_load"
    "9|Holder|two_loads_in_one_thread|caught|tsan|finish|holder.two_loads_in_one_thread"
    "9|Holder|two_loads_in_one_thread|survives|default|-|-"
    "25|Litmus|litmus|survives|default|-|-")

function(_lumex_relacy_executable target source definitions)
    add_executable(${target} ${source} LumexSplitCountRelacyModel.hpp)
    target_include_directories(${target} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}" "${LUMEX_RELACY_DIR}")
    if(definitions)
        target_compile_definitions(${target} PRIVATE ${definitions})
    endif()
    set_target_properties(${target} PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    # The models are tests: no warnings, like every other test translation
    # unit (Relacy itself is not warning-clean under the strict profile).
    if(COMMAND suppress_warnings_for_sources)
        suppress_warnings_for_sources(${target} MATCH ".*" ALL)
    endif()
    set_property(TARGET ${target} PROPERTY LUMEX_SUPPRESS_ALL_WARNINGS TRUE)
endfunction()

foreach(_model IN LISTS _relacy_models)
    string(TOLOWER "${_model}" _lower)
    foreach(_counter default tsan_counter)
        set(_target LumexSplitCountRelacy${_model})
        set(_name "${_lower}")
        set(_definitions "")
        if(_counter STREQUAL "tsan_counter")
            set(_target ${_target}_TsanCounter)
            set(_name "${_lower}_tsan_counter")
            set(_definitions LUMEX_RELACY_TSAN_COUNTER=1)
        endif()
        _lumex_relacy_executable(${_target}
            LumexSplitCountRelacy${_model}.relacy.cpp "${_definitions}")
        lumex_test_name(_ctest_name "${_name}")
        add_test(NAME ${_ctest_name} COMMAND ${_target})
        set_tests_properties(${_ctest_name} PROPERTIES
            LABELS "relacy" TIMEOUT ${_relacy_timeout}
            FAIL_REGULAR_EXPRESSION "RESULT FAIL")
    endforeach()
endforeach()

if(LUMEX_RELACY_MUTANTS)
    foreach(_row IN LISTS _relacy_mutants)
        string(REPLACE "|" ";" _fields "${_row}")
        list(GET _fields 0 _number)
        list(GET _fields 1 _model)
        list(GET _fields 2 _filter)
        list(GET _fields 3 _verdict)
        list(GET _fields 4 _counter)
        list(GET _fields 5 _sites)
        string(REPLACE "," "|" _sites "${_sites}")
        list(GET _fields 6 _expected)
        string(TOLOWER "${_model}" _lower)
        set(_definitions LUMEX_RELACY_MUTANT=${_number})
        set(_suffix "M${_number}")
        set(_cname "mutant_m${_number}_${_lower}")
        if(_counter STREQUAL "tsan")
            list(APPEND _definitions LUMEX_RELACY_TSAN_COUNTER=1)
            set(_suffix "${_suffix}_Tsan")
            set(_cname "${_cname}_tsan_counter")
        endif()
        set(_cname "${_cname}_${_verdict}")
        set(_target LumexSplitCountRelacy${_model}_${_suffix})
        if(NOT TARGET ${_target})
            _lumex_relacy_executable(${_target}
                LumexSplitCountRelacy${_model}.relacy.cpp "${_definitions}")
        endif()
        # One case per row; rows with the same mutant and program but another
        # filter get their own name from the filter.
        string(REGEX REPLACE "[^A-Za-z0-9]+" "_" _tag "${_filter}")
        lumex_test_name(_ctest_name "${_cname}_${_tag}")
        add_test(NAME ${_ctest_name}
            COMMAND ${_target} --only ${_filter})
        if(_verdict STREQUAL "caught" AND _sites STREQUAL "-")
            set_tests_properties(${_ctest_name} PROPERTIES
                PASS_REGULAR_EXPRESSION "RESULT FAIL ${_expected}")
        elseif(_verdict STREQUAL "caught")
            set_tests_properties(${_ctest_name} PROPERTIES
                PASS_REGULAR_EXPRESSION
                "DATA RACE \\(data race detected\\), in (${_sites}),.*RESULT FAIL ${_expected}")
        else()
            set_tests_properties(${_ctest_name} PROPERTIES
                PASS_REGULAR_EXPRESSION "RESULT pass"
                FAIL_REGULAR_EXPRESSION "RESULT FAIL")
        endif()
        set_tests_properties(${_ctest_name} PROPERTIES
            LABELS "relacy_mutant" TIMEOUT ${_relacy_timeout})
    endforeach()
endif()
