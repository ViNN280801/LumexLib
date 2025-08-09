# lumex/tests/CMakeLists.txt

# Fetch GoogleTest
include(FetchContent)
FetchContent_Declare(
    googletest

    # Using a known-stable release of googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    CMAKE_ARGS -DCMAKE_POLICY_VERSION_MINIMUM=3.5 # Set policy version for googletest
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_subdirectory(applied)
add_subdirectory(core)

if(LUMEX_BUILD_XML)
    add_subdirectory(xml)
endif()
