# lumex/tests/core/utility/CMakeLists.txt

set(TEST_EXECUTABLE_NAME LumexUtilityTests)

add_executable(
    ${TEST_EXECUTABLE_NAME}
    LumexBit.tests.cpp
    LumexCallbackSlot.tests.cpp
    LumexCast.tests.cpp
    LumexCoreDumpGenerator.tests.cpp
    LumexDebug.tests.cpp
    LumexDemangle.tests.cpp
    LumexExceptionMacros.tests.cpp
    LumexMemRead.tests.cpp
    LumexRanges.tests.cpp
    LumexSafeNumericComparator.tests.cpp
    LumexRangeTraits.tests.cpp
    LumexStreamTraits.tests.cpp
    LumexTypeTraits.tests.cpp
    LumexTypeTraitsTopics.tests.cpp
    LumexUtilities.tests.cpp
)

set_target_properties(${TEST_EXECUTABLE_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
)

target_link_libraries(
    ${TEST_EXECUTABLE_NAME}
    PRIVATE
    lumex::utility
)

# LumexBit.hpp / LumexCast.hpp / LumexMemRead.hpp / LumexRanges.hpp and the
# LUMEX_DEFINE_ENUM_TRAITS test (using enum) all require C++20.
lumex_test_use_gtest(${TEST_EXECUTABLE_NAME} CXX_STANDARD 20)

# LumexDebug.hpp's captureStackTrace() uses the DbgHelp API (SymInitialize, SymFromAddr,
# SymGetLineFromAddr64, SymGetModuleInfo64, SymRefreshModuleList, ...) on Windows.
if(WIN32)
    target_link_libraries(${TEST_EXECUTABLE_NAME} PRIVATE dbghelp)
endif()

if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.18")
    gtest_discover_tests(${TEST_EXECUTABLE_NAME}
        DISCOVERY_TIMEOUT 60
        DISCOVERY_MODE PRE_TEST
    )
else()
    add_test(NAME ${TEST_EXECUTABLE_NAME} COMMAND ${TEST_EXECUTABLE_NAME})
endif()

# Same TypeTraits sources, pinned to C++11. lumex_enum_traits_t / using enum stay
# behind #if __cplusplus >= 202002L in the .cpp, so this suite proves
# CleanType, is_callable, is_optional (optional), and default_return
# on the library floor. UtilityTests remains CXX 20 for Bit/Cast/Ranges
# plus the lumex_enum_traits_t cases.
set(TYPE_TRAITS_TEST_NAME LumexTypeTraitsTests)

add_executable(
    ${TYPE_TRAITS_TEST_NAME}
    LumexExceptionMacros.tests.cpp
    LumexRangeTraits.tests.cpp
    LumexStreamTraits.tests.cpp
    LumexTypeTraits.tests.cpp
    LumexTypeTraitsTopics.tests.cpp
)

set_target_properties(${TYPE_TRAITS_TEST_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
)

target_link_libraries(
    ${TYPE_TRAITS_TEST_NAME}
    PRIVATE
    lumex::utility
)

lumex_test_use_gtest(${TYPE_TRAITS_TEST_NAME} CXX_STANDARD 11)

if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.18")
    gtest_discover_tests(${TYPE_TRAITS_TEST_NAME}
        DISCOVERY_TIMEOUT 60
        DISCOVERY_MODE PRE_TEST
    )
else()
    add_test(NAME ${TYPE_TRAITS_TEST_NAME} COMMAND ${TYPE_TRAITS_TEST_NAME})
endif()

# Keep a C++11 target that instantiates every comma-bearing
# is_safe_comparable<T, U> assertion. C++20 compilation does not prove that
# the C++11 expansion of LUMEX_STATIC_ASSERT_MSG remains valid.
set(SAFE_NUMERIC_CXX11_TEST_NAME LumexSafeNumericComparatorCxx11Tests)

add_executable(
    ${SAFE_NUMERIC_CXX11_TEST_NAME}
    LumexSafeNumericComparator.cxx11.tests.cpp
)

set_target_properties(${SAFE_NUMERIC_CXX11_TEST_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
)

target_link_libraries(
    ${SAFE_NUMERIC_CXX11_TEST_NAME}
    PRIVATE
    lumex::utility
)

lumex_test_use_gtest(${SAFE_NUMERIC_CXX11_TEST_NAME} CXX_STANDARD 11)

if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.18")
    gtest_discover_tests(${SAFE_NUMERIC_CXX11_TEST_NAME}
        DISCOVERY_TIMEOUT 60
        DISCOVERY_MODE PRE_TEST
    )
else()
    add_test(NAME ${SAFE_NUMERIC_CXX11_TEST_NAME}
        COMMAND ${SAFE_NUMERIC_CXX11_TEST_NAME})
endif()

# get_current_pid () is C++11 header-only; pin the suite to that floor.
set(PROCESS_TEST_NAME LumexProcessTests)

add_executable(
    ${PROCESS_TEST_NAME}
    LumexProcess.tests.cpp
)

set_target_properties(${PROCESS_TEST_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
)

target_link_libraries(
    ${PROCESS_TEST_NAME}
    PRIVATE
    lumex::utility
)

lumex_test_use_gtest(${PROCESS_TEST_NAME} CXX_STANDARD 11)

if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.18")
    gtest_discover_tests(${PROCESS_TEST_NAME}
        DISCOVERY_TIMEOUT 60
        DISCOVERY_MODE PRE_TEST
    )
else()
    add_test(NAME ${PROCESS_TEST_NAME} COMMAND ${PROCESS_TEST_NAME})
endif()

# LumexCallbackSlot.hpp is C++11 header-only; pin a suite to that floor. The
# same source also runs inside LumexUtilityTests at C++20.
set(CALLBACK_SLOT_TEST_NAME LumexCallbackSlotTests)

add_executable(
    ${CALLBACK_SLOT_TEST_NAME}
    LumexCallbackSlot.tests.cpp
)

set_target_properties(${CALLBACK_SLOT_TEST_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
)

target_link_libraries(
    ${CALLBACK_SLOT_TEST_NAME}
    PRIVATE
    lumex::utility
)

lumex_test_use_gtest(${CALLBACK_SLOT_TEST_NAME} CXX_STANDARD 11)

if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.18")
    gtest_discover_tests(${CALLBACK_SLOT_TEST_NAME}
        DISCOVERY_TIMEOUT 60
        DISCOVERY_MODE PRE_TEST
    )
else()
    add_test(NAME ${CALLBACK_SLOT_TEST_NAME} COMMAND ${CALLBACK_SLOT_TEST_NAME})
endif()
