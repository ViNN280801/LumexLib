# lumex/tests/CMakeLists.txt

# CMake-file tests do not need GoogleTest. Register them even when every
# library group is OFF so -DLUMEX_BUILD_TESTS=ON still has a suite.
add_subdirectory(cmake)

# Two vendored GoogleTest trees (not FetchContent):
#   1.12.1 for C++11/14 tests, 1.18.0 for C++17+ tests.
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/LumexGoogleTest.cmake")
include(GoogleTest)

lumex_add_subdirectory_if(LUMEX_BUILD_APPLIED applied)
lumex_add_subdirectory_if(LUMEX_BUILD_CORE core)
lumex_add_subdirectory_if(LUMEX_BUILD_XML xml)
