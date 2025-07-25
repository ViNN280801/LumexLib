# lumex/tests/CMakeLists.txt

# Fetch GoogleTest
include(FetchContent)
FetchContent_Declare(
    googletest

    # Using a known-stable release of googletest
    URL https://github.com/google/googletest/archive/release-1.11.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_subdirectory(core)
