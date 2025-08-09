# lumex/tests/xml/CMakeLists.txt

set(TEST_EXECUTABLE_NAME LumexXmlTests)

add_executable(
    ${TEST_EXECUTABLE_NAME}
    LumexXml.tests.cpp
)

target_link_libraries(
    ${TEST_EXECUTABLE_NAME}
    PRIVATE
    GTest::gtest_main
    Lumex::xml
)

if(WIN32)
    add_custom_command(
        TARGET ${TEST_EXECUTABLE_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:GTest::gtest>
        $<TARGET_FILE_DIR:${TEST_EXECUTABLE_NAME}>

        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:GTest::gtest_main>
        $<TARGET_FILE_DIR:${TEST_EXECUTABLE_NAME}>

        COMMENT "Copying GoogleTest DLLs for runtime execution"
    )
endif()

include(GoogleTest)
gtest_discover_tests(${TEST_EXECUTABLE_NAME})
