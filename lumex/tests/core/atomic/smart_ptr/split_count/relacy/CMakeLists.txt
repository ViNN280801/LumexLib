# lumex/tests/core/atomic/smart_ptr/split_count/relacy/CMakeLists.txt
#
# Relacy models of the memory orders of the split-count engine. Optional: the
# directory is added by ../CMakeLists.txt only when LUMEX_RELACY_DIR names a
# checkout of Relacy (https://github.com/dvyukov/relacy, Dmitry Vyukov's race
# detector for the C++11 memory model; header-only, not vendored here). The
# default build and the default test run do not change.
#
# How to run:
#   git clone https://github.com/dvyukov/relacy ~/relacy
#   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
#         -DLUMEX_BUILD_TESTS=ON -DLUMEX_RELACY_DIR=$HOME/relacy
#   ninja -C build LumexSplitCountRelacyBlock LumexSplitCountRelacyHolder \
#                  LumexSplitCountRelacyLitmus
#   ctest --test-dir build -L relacy --output-on-failure
# or run a program by hand, with a filter and another search:
#   build/bin/LumexSplitCountRelacyBlock --only load_store/k0 \
#         --search bound --bound 6
#
# Mutants (an order of the engine weakened): configure with
# -DLUMEX_RELACY_MUTANTS=ON; every row of the table below becomes a program
# built with LUMEX_RELACY_MUTANT=<number> and a CTest case with the label
# relacy_mutant. The case passes when the program reports a failure (the
# mutation is caught) or, for the two orders found redundant, when it still
# passes.
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

# Relacy includes <source_location> whatever the standard is (GCC 11 and
# later, Clang with such a library, MSVC); an older toolchain cannot build it.
include(CheckIncludeFileCXX)
check_include_file_cxx(source_location LUMEX_RELACY_HAS_SOURCE_LOCATION)
if(NOT LUMEX_RELACY_HAS_SOURCE_LOCATION)
    message(STATUS "Skipping the Relacy models: the compiler has no <source_location>")
    return()
endif()

set(_relacy_models Block Holder Litmus)

# One CTest case per model program. The search of each program is in its main
# (the full and bounded searches are the long ones); the limit is in seconds.
set(_relacy_timeout 1800)

# The mutants: <number>|<model program>|<filter>|<verdict>. The filter selects
# the models of the program by a substring of "<program>.<model>/k<reserve>";
# the verdict is "caught" (the program must report a failure: the engine order
# the mutation weakens is needed) or "survives" (the program must still pass:
# the order is redundant in the C++11 memory model, see the notes below).
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
#   9    S4 success without acquire                survives: the pin (S2) and the
#        failure order of S4 already acquire every release that could precede
#        a successful swap, and the word is only ever changed by
#        read-modify-writes, which continue every release sequence
#   25   rmw_order ignores seq_cst                 survives: every access to the
#        word is a read-modify-write, and two acq_rel read-modify-writes of one
#        atomic synchronize, which closes the cycle of the store-buffering and
#        independent-reads outcomes without seq_cst
set(_relacy_mutants
    "8|Block|block.publish_load|caught"
    "10|Holder|holder.load_store|caught"
    "33|Block|block.load_store|caught"
    "331|Block|block.load_store|caught"
    "332|Block|two_writers|caught"
    "101|Block|block.load_store|caught"
    "102|Block|block.load_store|caught"
    "103|Block|block.publish_load|caught"
    "104|Holder|holder.publish_load|caught"
    "105|Block|cas_load_store|caught"
    "105|Holder|holder.cas_load/|caught"
    "9|Holder|holder.t|survives"
    "25|Litmus|litmus|survives")

function(_lumex_relacy_executable target source definition)
    add_executable(${target} ${source} LumexSplitCountRelacyModel.hpp)
    target_include_directories(${target} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}" "${LUMEX_RELACY_DIR}")
    if(definition)
        target_compile_definitions(${target} PRIVATE ${definition})
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
    set(_target LumexSplitCountRelacy${_model})
    _lumex_relacy_executable(${_target}
        ${_target}.relacy.cpp "")
    string(TOLOWER "${_model}" _lower)
    lumex_test_name(_ctest_name "${_lower}")
    add_test(NAME ${_ctest_name} COMMAND ${_target})
    set_tests_properties(${_ctest_name} PROPERTIES
        LABELS "relacy" TIMEOUT ${_relacy_timeout})
endforeach()

if(LUMEX_RELACY_MUTANTS)
    foreach(_row IN LISTS _relacy_mutants)
        string(REPLACE "|" ";" _row "${_row}")
        list(GET _row 0 _number)
        list(GET _row 1 _model)
        list(GET _row 2 _filter)
        list(GET _row 3 _verdict)
        string(TOLOWER "${_model}" _lower)
        set(_target LumexSplitCountRelacy${_model}_M${_number})
        _lumex_relacy_executable(${_target}
            LumexSplitCountRelacy${_model}.relacy.cpp
            LUMEX_RELACY_MUTANT=${_number})
        lumex_test_name(_ctest_name "mutant_m${_number}_${_lower}_${_verdict}")
        add_test(NAME ${_ctest_name}
            COMMAND ${_target} --only ${_filter})
        set(_expect_failure FALSE)
        if(_verdict STREQUAL "caught")
            set(_expect_failure TRUE)
        endif()
        set_tests_properties(${_ctest_name} PROPERTIES
            LABELS "relacy_mutant" WILL_FAIL ${_expect_failure}
            TIMEOUT ${_relacy_timeout})
    endforeach()
endif()
