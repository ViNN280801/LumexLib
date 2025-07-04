cmake_minimum_required(VERSION 3.10)

project(LumexLib
    VERSION 1.0.0
    LANGUAGES CXX
)

# --- C++ Standard ---
# Set C++11 as the default, but allow overriding it.
if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 11)
endif()

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF) # Use -std=c++11 instead of -std=gnu++11

# --- Options ---
option(LUMEX_BUILD_SHARED_LIBS "Build shared libraries (.dll/.so)" ON)

if(LUMEX_BUILD_SHARED_LIBS)
    set(BUILD_SHARED_LIBS ON)
    message(STATUS "Building as shared libraries")
else()
    message(STATUS "Building as static libraries")
endif()

# Set position-independent code for shared libs or static libs that will be linked into shared
if(BUILD_SHARED_LIBS OR(CMAKE_POSITION_INDEPENDENT_CODE AND NOT WIN32))
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

# ---------------------------------------------------------------------------
# Unified output directories – ensures *.dll sit next to the executables
# ---------------------------------------------------------------------------
set(LUMEX_BIN_DIR ${CMAKE_BINARY_DIR}/bin)
set(LUMEX_LIB_DIR ${CMAKE_BINARY_DIR}/lib)

# Multi-config generators (Visual Studio, Xcode)
if(CMAKE_CONFIGURATION_TYPES)
    foreach(CONF ${CMAKE_CONFIGURATION_TYPES})
        string(TOUPPER ${CONF} CONF_UPPER)
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONF_UPPER} ${LUMEX_BIN_DIR}/${CONF})
        set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CONF_UPPER} ${LUMEX_BIN_DIR}/${CONF})
        set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CONF_UPPER} ${LUMEX_LIB_DIR}/${CONF})
    endforeach()

# Single-config generators (Make, Ninja)
else()
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${LUMEX_BIN_DIR})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${LUMEX_BIN_DIR})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${LUMEX_LIB_DIR})
endif()

# --- Includes & Subdirectories ---
add_subdirectory(lumex)

# ==============================================================================
# COMPILER OPTIMIZATIONS & WARNINGS
# ==============================================================================

# Common compiler flags
include(CheckCXXCompilerFlag)

# Warnings
if(MSVC)
    # MSVC specific warning flags
    add_compile_options(
        /W4 # Highest warning level for MSVC
        /permissive- # Enable strict standard compliance
    )

# Specific warnings equivalent to -Wextra, -Wshadow etc. might need individual flags
# For example, /wd#### to disable specific warnings that are too noisy
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # GCC/Clang specific warning flags
    foreach(FLAG -Wall -Wextra -Wpedantic -Wshadow)
        check_cxx_compiler_flag(${FLAG} HAS_FLAG_${FLAG})

        if(HAS_FLAG_${FLAG})
            add_compile_options(${FLAG})
        endif()
    endforeach()
endif()

# ==============================================================================
# INSTALLATION
# ==============================================================================
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Generate and install the export file for targets
install(EXPORT LumexCoreTargets
    FILE
    LumexCoreTargets.cmake
    NAMESPACE
    Lumex::
    DESTINATION
    ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)

# Generate and install the export file for LumexApplied targets
install(EXPORT LumexAppliedTargets
    FILE
    LumexAppliedTargets.cmake
    NAMESPACE
    Lumex::
    DESTINATION
    ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)

# Generate and install the config version file
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/LumexLibConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

# Generate the main config file
configure_package_config_file(
    "cmake/LumexLibConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/LumexLibConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/LumexLib"
)

# Install the generated config files
install(
    FILES
    "${CMAKE_CURRENT_BINARY_DIR}/LumexLibConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/LumexLibConfigVersion.cmake"
    DESTINATION
    ${CMAKE_INSTALL_LIBDIR}/cmake/LumexLib
)

# ==============================================================================
# TESTING
# ==============================================================================
option(LUMEX_BUILD_TESTS "Build the tests for LumexCore" ON)

if(LUMEX_BUILD_TESTS)
    message(STATUS "Building with tests enabled")
    enable_testing()
    add_subdirectory(lumex/tests)

    add_custom_target(run_all_tests ALL
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        DEPENDS
        LumexTimeTests
        LumexOptionalTests
        LumexNumberGeneratorTests
        LumexStringifyTests
        COMMENT "Running all tests..."
    )
endif()
