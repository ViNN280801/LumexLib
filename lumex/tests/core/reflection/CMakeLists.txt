# lumex/tests/core/reflection/CMakeLists.txt

set(TEST_EXECUTABLE_NAME LumexReflectionTests)

add_executable(
    ${TEST_EXECUTABLE_NAME}
    LumexReflectedEnum.tests.cpp
    LumexVarInfo.tests.cpp
)

set_target_properties(${TEST_EXECUTABLE_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
)

target_link_libraries(
    ${TEST_EXECUTABLE_NAME}
    PRIVATE
    lumex::reflection
)

lumex_test_use_gtest(${TEST_EXECUTABLE_NAME} CXX_STANDARD 11)
gtest_discover_tests(${TEST_EXECUTABLE_NAME})

# field_reflection/ needs LUMEX_WITH_FIELD_REFLECTION and the
# in-tree nlohmann_json::nlohmann_json target from LumexNlohmannJson.cmake.
# C++11 suite proves count. C++14 suite proves indexed get
# (friend-auto loophole). C++20 suite proves names + to_json
# (pointer NTTP pretty names).
if(LUMEX_WITH_FIELD_REFLECTION AND TARGET nlohmann_json::nlohmann_json)
    function(lumex_add_field_reflection_tests target_name cxx_standard)
        add_executable(${target_name} LumexFieldReflection.tests.cpp)
        target_link_libraries(
            ${target_name}
            PRIVATE
            lumex::reflection
            nlohmann_json::nlohmann_json
        )
        target_compile_definitions(
            ${target_name}
            PRIVATE
            LUMEX_WITH_FIELD_REFLECTION
        )
        lumex_test_use_gtest(${target_name} CXX_STANDARD ${cxx_standard})
        gtest_discover_tests(${target_name})
    endfunction()

    lumex_add_field_reflection_tests(LumexFieldReflectionTests 11)
    lumex_add_field_reflection_tests(LumexFieldReflectionGetTests 14)
    lumex_add_field_reflection_tests(LumexFieldReflectionNamesTests 20)
else()
    message(STATUS "Skipping LumexFieldReflectionTests: requires LUMEX_WITH_FIELD_REFLECTION=ON and an available nlohmann_json::nlohmann_json target")
endif()
