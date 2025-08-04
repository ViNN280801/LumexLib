# Root CMakeLists.txt

cmake_minimum_required(VERSION 3.10)

project(LumexLib
    VERSION 1.1.0
    LANGUAGES CXX
)

if(ARCHITECTURE)
    message(STATUS "Building for target architecture: ${ARCHITECTURE}")
endif()

# --- C++ Standard ---
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

# ==============================================================================
# COMPILER FLAGS & WARNINGS
# ==============================================================================

# Include CheckCXXCompilerFlag for feature detection
include(CheckCXXCompilerFlag)

# Enable Link Time Optimization for Release builds
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON)

if(MSVC)
    # ==============================================================================
    # MSVC SPECIFIC FLAGS
    # ==============================================================================

    # Base warning level and compliance
    add_compile_options(
        /W4 # Highest warning level
        /permissive- # Strict standard compliance
        /MP # Multi-processor compilation
        /Zc:__cplusplus # Enable correct __cplusplus macro
        /Zc:inline # Remove unreferenced functions/data
        /Zc:throwingNew # Assume operator new throws on failure
        /EHsc # Exception handling (C++ exceptions only)
    )

    # Configuration-specific optimizations
    add_compile_options(

        # === DEBUG CONFIGURATION ===
        $<$<CONFIG:Debug>:/Od> # Disable optimization
        $<$<CONFIG:Debug>:/MDd> # Dynamic runtime library (debug)
        $<$<CONFIG:Debug>:/RTC1> # Runtime checks
        $<$<CONFIG:Debug>:/Zi> # Debug information
        $<$<CONFIG:Debug>:/DDEBUG> # Debug preprocessor define
        $<$<CONFIG:Debug>:/D_DEBUG> # Debug preprocessor define

        # === RELEASE CONFIGURATION ===
        $<$<CONFIG:Release>:/O2> # Maximum speed optimization
        $<$<CONFIG:Release>:/Oi> # Enable intrinsic functions
        $<$<CONFIG:Release>:/Ot> # Favor speed over size
        $<$<CONFIG:Release>:/Oy> # Omit frame pointers
        $<$<CONFIG:Release>:/GL> # Whole program optimization
        $<$<CONFIG:Release>:/Gy> # Function-level linking
        $<$<CONFIG:Release>:/GS-> # Disable security checks for performance
        $<$<CONFIG:Release>:/arch:AVX2> # Enable AVX2 instructions (if supported)
        $<$<CONFIG:Release>:/fp:fast> # Fast floating point model
        $<$<CONFIG:Release>:/MD> # Dynamic runtime library (release)
        $<$<CONFIG:Release>:/DNDEBUG> # Release preprocessor define

        # === RELEASE WITH DEBUG INFO ===
        $<$<CONFIG:RelWithDebInfo>:/O2> # Maximum speed optimization
        $<$<CONFIG:RelWithDebInfo>:/g1> # Minimal debug info
        $<$<CONFIG:RelWithDebInfo>:/MD> # Dynamic runtime library
        $<$<CONFIG:RelWithDebInfo>:/DNDEBUG> # Release preprocessor define

        # === MIN SIZE RELEASE CONFIGURATION ===
        $<$<CONFIG:MinSizeRel>:/O1> # Optimize for size
        $<$<CONFIG:MinSizeRel>:/Os> # Favor small code
        $<$<CONFIG:MinSizeRel>:/MD> # Dynamic runtime library
        $<$<CONFIG:MinSizeRel>:/DNDEBUG> # Release preprocessor define
    )

    # Linker optimizations for Release builds
    add_link_options(
        $<$<CONFIG:Release>:/LTCG> # Link-time code generation
        $<$<CONFIG:Release>:/OPT:REF> # Remove unreferenced functions
        $<$<CONFIG:Release>:/OPT:ICF> # Enable COMDAT folding
        $<$<CONFIG:Release>:/INCREMENTAL:NO> # Disable incremental linking

        $<$<CONFIG:RelWithDebInfo>:/LTCG> # Link-time code generation
        $<$<CONFIG:RelWithDebInfo>:/OPT:REF> # Remove unreferenced functions
        $<$<CONFIG:RelWithDebInfo>:/OPT:ICF> # Enable COMDAT folding
        $<$<CONFIG:RelWithDebInfo>:/DEBUG> # Generate debug info
        $<$<CONFIG:RelWithDebInfo>:/INCREMENTAL:NO> # Disable incremental linking

        $<$<CONFIG:MinSizeRel>:/LTCG> # Link-time code generation
        $<$<CONFIG:MinSizeRel>:/OPT:REF> # Remove unreferenced functions
        $<$<CONFIG:MinSizeRel>:/OPT:ICF> # Enable COMDAT folding
        $<$<CONFIG:MinSizeRel>:/INCREMENTAL:NO> # Disable incremental linking
    )

else()
    # ==============================================================================
    # GCC/CLANG SPECIFIC FLAGS
    # ==============================================================================

    # Base warning flags
    set(COMMON_WARNING_FLAGS
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wconversion
        -Wsign-conversion
        -Wunused
        -Wno-unused-parameter
        -Werror=return-type
        -Wcast-align
        -Wcast-qual
        -Wctor-dtor-privacy
        -Wdisabled-optimization
        -Wformat=2
        -Winit-self
        -Wmissing-declarations
        -Wmissing-include-dirs
        -Wold-style-cast
        -Woverloaded-virtual
        -Wredundant-decls
        -Wsign-promo
        -Wstrict-overflow=2
        -Wundef
    )

    # Check and apply warning flags
    foreach(FLAG ${COMMON_WARNING_FLAGS})
        string(REGEX REPLACE "[-=+]" "_" FLAG_VAR ${FLAG})
        check_cxx_compiler_flag(${FLAG} COMPILER_SUPPORTS${FLAG_VAR})

        if(COMPILER_SUPPORTS${FLAG_VAR})
            add_compile_options(${FLAG})
        endif()
    endforeach()

    # Detect CPU architecture for march optimization
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64")
        set(NATIVE_ARCH_FLAG "-march=native")
        set(TUNE_ARCH_FLAG "-mtune=native")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "i386|i686")
        set(NATIVE_ARCH_FLAG "-march=native")
        set(TUNE_ARCH_FLAG "-mtune=native")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm|ARM")
        set(NATIVE_ARCH_FLAG "-mcpu=native")
        set(TUNE_ARCH_FLAG "")
    else()
        set(NATIVE_ARCH_FLAG "")
        set(TUNE_ARCH_FLAG "")
    endif()

    # Check if compiler supports the architecture flags
    if(NATIVE_ARCH_FLAG)
        check_cxx_compiler_flag(${NATIVE_ARCH_FLAG} COMPILER_SUPPORTS_MARCH_NATIVE)
    endif()

    if(TUNE_ARCH_FLAG)
        check_cxx_compiler_flag(${TUNE_ARCH_FLAG} COMPILER_SUPPORTS_MTUNE_NATIVE)
    endif()

    # Configuration-specific optimizations
    add_compile_options(

        # === DEBUG CONFIGURATION ===
        $<$<CONFIG:Debug>:-O0> # No optimization
        $<$<CONFIG:Debug>:-g3> # Maximum debug info
        $<$<CONFIG:Debug>:-ggdb> # GDB-specific debug info
        $<$<CONFIG:Debug>:-fno-omit-frame-pointer> # Keep frame pointers for debugging
        $<$<CONFIG:Debug>:-fno-optimize-sibling-calls> # Don't optimize tail calls
        $<$<CONFIG:Debug>:-fstack-protector-strong> # Stack protection
        $<$<CONFIG:Debug>:-DDEBUG> # Debug preprocessor define
        $<$<CONFIG:Debug>:-D_DEBUG> # Debug preprocessor define

        # === RELEASE CONFIGURATION ===
        $<$<CONFIG:Release>:-O3> # Maximum optimization
        $<$<CONFIG:Release>:-DNDEBUG> # Release preprocessor define
        $<$<CONFIG:Release>:-ffast-math> # Fast math optimizations
        $<$<CONFIG:Release>:-funroll-loops> # Unroll loops
        $<$<CONFIG:Release>:-fprefetch-loop-arrays> # Prefetch loop arrays
        $<$<CONFIG:Release>:-fomit-frame-pointer> # Omit frame pointers
        $<$<CONFIG:Release>:-ffunction-sections> # Each function in separate section
        $<$<CONFIG:Release>:-fdata-sections> # Each data in separate section

        # === RELEASE WITH DEBUG INFO ===
        $<$<CONFIG:RelWithDebInfo>:-O2> # Optimize for speed
        $<$<CONFIG:RelWithDebInfo>:-g1> # Minimal debug info
        $<$<CONFIG:RelWithDebInfo>:-DNDEBUG> # Release preprocessor define
        $<$<CONFIG:RelWithDebInfo>:-ffunction-sections> # Each function in separate section
        $<$<CONFIG:RelWithDebInfo>:-fdata-sections> # Each data in separate section

        # === MIN SIZE RELEASE CONFIGURATION ===
        $<$<CONFIG:MinSizeRel>:-Os> # Optimize for size
        $<$<CONFIG:MinSizeRel>:-DNDEBUG> # Release preprocessor define
        $<$<CONFIG:MinSizeRel>:-ffunction-sections> # Each function in separate section
        $<$<CONFIG:MinSizeRel>:-fdata-sections> # Each data in separate section
        $<$<CONFIG:MinSizeRel>:-fno-unroll-loops> # Don't unroll loops to save space
    )

    # Apply architecture-specific optimizations for Release builds
    if(COMPILER_SUPPORTS_MARCH_NATIVE)
        add_compile_options(
            $<$<CONFIG:Release>:${NATIVE_ARCH_FLAG}>
            $<$<CONFIG:RelWithDebInfo>:${NATIVE_ARCH_FLAG}>
        )
    endif()

    if(COMPILER_SUPPORTS_MTUNE_NATIVE AND TUNE_ARCH_FLAG)
        add_compile_options(
            $<$<CONFIG:Release>:${TUNE_ARCH_FLAG}>
            $<$<CONFIG:RelWithDebInfo>:${TUNE_ARCH_FLAG}>
        )
    endif()

    # Linker optimizations
    add_link_options(
        $<$<CONFIG:Release>:-Wl,--gc-sections> # Remove unused sections
        $<$<CONFIG:Release>:-Wl,--strip-all> # Strip all symbols

        $<$<CONFIG:RelWithDebInfo>:-Wl,--gc-sections> # Remove unused sections

        $<$<CONFIG:MinSizeRel>:-Wl,--gc-sections> # Remove unused sections
        $<$<CONFIG:MinSizeRel>:-Wl,--strip-all> # Strip all symbols
    )

    # GCC/Clang specific additional optimizations
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        # GCC-specific flags
        add_compile_options(
            $<$<CONFIG:Release>:-flto=auto> # Link Time Optimization (auto threads)
            $<$<CONFIG:Release>:-fuse-linker-plugin> # Use linker plugin for LTO

            $<$<CONFIG:RelWithDebInfo>:-flto=auto>
            $<$<CONFIG:RelWithDebInfo>:-fuse-linker-plugin>
        )

        # Check for newer GCC features
        if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "7.0")
            add_compile_options(
                $<$<CONFIG:Release>:-falign-functions=32>
                $<$<CONFIG:Release>:-falign-loops=32>
            )
        endif()

    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        # Clang-specific flags
        add_compile_options(
            $<$<CONFIG:Release>:-flto=thin> # Thin Link Time Optimization

            $<$<CONFIG:RelWithDebInfo>:-flto=thin>
        )

        # Clang-specific optimizations
        check_cxx_compiler_flag("-fvectorize" CLANG_SUPPORTS_VECTORIZE)

        if(CLANG_SUPPORTS_VECTORIZE)
            add_compile_options(
                $<$<CONFIG:Release>:-fvectorize>
                $<$<CONFIG:RelWithDebInfo>:-fvectorize>
            )
        endif()
    endif()

    # Additional performance flags for specific architectures
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
        check_cxx_compiler_flag("-msse4.2" COMPILER_SUPPORTS_SSE42)

        if(COMPILER_SUPPORTS_SSE42)
            add_compile_options(
                $<$<CONFIG:Release>:-msse4.2>
                $<$<CONFIG:RelWithDebInfo>:-msse4.2>
            )
        endif()

        # Additional SIMD optimizations for x86_64
        check_cxx_compiler_flag("-mavx" COMPILER_SUPPORTS_AVX)

        if(COMPILER_SUPPORTS_AVX)
            add_compile_options(
                $<$<CONFIG:Release>:-mavx>
                $<$<CONFIG:RelWithDebInfo>:-mavx>
            )
        endif()

        check_cxx_compiler_flag("-mavx2" COMPILER_SUPPORTS_AVX2)

        if(COMPILER_SUPPORTS_AVX2)
            add_compile_options(
                $<$<CONFIG:Release>:-mavx2>
                $<$<CONFIG:RelWithDebInfo>:-mavx2>
            )
        endif()

        # Cache optimizations
        check_cxx_compiler_flag("-prefetch-loop-arrays" COMPILER_SUPPORTS_PREFETCH)

        if(COMPILER_SUPPORTS_PREFETCH)
            add_compile_options(
                $<$<CONFIG:Release>:-prefetch-loop-arrays>
                $<$<CONFIG:RelWithDebInfo>:-prefetch-loop-arrays>
            )
        endif()
    endif()

    # Additional aggressive optimization flags for Release
    check_cxx_compiler_flag("-finline-functions" COMPILER_SUPPORTS_INLINE_FUNCTIONS)

    if(COMPILER_SUPPORTS_INLINE_FUNCTIONS)
        add_compile_options(
            $<$<CONFIG:Release>:-finline-functions>
        )
    endif()

    check_cxx_compiler_flag("-fmodulo-sched" COMPILER_SUPPORTS_MODULO_SCHED)

    if(COMPILER_SUPPORTS_MODULO_SCHED)
        add_compile_options(
            $<$<CONFIG:Release>:-fmodulo-sched>
        )
    endif()

    check_cxx_compiler_flag("-ftracer" COMPILER_SUPPORTS_TRACER)

    if(COMPILER_SUPPORTS_TRACER)
        add_compile_options(
            $<$<CONFIG:Release>:-ftracer>
        )
    endif()

    # Profile Guided Optimization hints
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "4.9")
        check_cxx_compiler_flag("-fprofile-correction" COMPILER_SUPPORTS_PROFILE_CORRECTION)

        if(COMPILER_SUPPORTS_PROFILE_CORRECTION)
            add_compile_options(
                $<$<CONFIG:Release>:-fprofile-correction>
            )
        endif()
    endif()
endif()

# ==============================================================================
# ADDITIONAL PERFORMANCE SETTINGS
# ==============================================================================

# Set default build type if not specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to 'Release' as none was specified.")
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()

# Print build configuration info
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Target architecture: ${CMAKE_SYSTEM_PROCESSOR}")

# Platform-specific optimizations
if(UNIX AND NOT APPLE)
    # Linux-specific optimizations
    add_compile_definitions(_GNU_SOURCE)
elseif(APPLE)
    # macOS-specific optimizations
    add_compile_options(-fno-common)
elseif(WIN32)
    # Windows-specific optimizations
    add_compile_definitions(
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
        _SCL_SECURE_NO_WARNINGS
    )
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
# OWN COMPONENTS
# ==============================================================================
option(LUMEX_BUILD_XML "Build Xml" OFF)

if(LUMEX_BUILD_XML)
    option(LUMEX_XML_WCHAR_MODE "Build Xml with wchar_t mode" OFF)

    if(LUMEX_XML_WCHAR_MODE)
        add_compile_definitions(LUMEX_XML_WCHAR_MODE)
        message(STATUS "Building with Xml wchar_t mode enabled")
    endif()

    add_subdirectory(lumex/xml)
    message(STATUS "Building with Xml enabled")
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

if(LUMEX_BUILD_XML)
    install(EXPORT LumexXmlTargets
        FILE
        LumexXmlTargets.cmake
        NAMESPACE
        Lumex::
        DESTINATION
        ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
    )
endif()

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
# DOCUMENTATION
# ==============================================================================
option(LUMEX_BUILD_DOCUMENTATION "Build Doxygen documentation" OFF)

if(LUMEX_BUILD_DOCUMENTATION)
    message(STATUS "Building with documentation enabled")
    find_program(DOXYGEN_EXECUTABLE NAMES doxygen)

    if(NOT DOXYGEN_EXECUTABLE)
        message(FATAL_ERROR "Doxygen executable not found! Please install Doxygen or ensure it's in your PATH.")
    endif()

    set(DOXYFILE_PATH "${CMAKE_SOURCE_DIR}/Doxyfile")
    set(DOXYGEN_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/docs")

    add_custom_command(
        OUTPUT "${DOXYGEN_OUTPUT_DIR}/html/index.html" # Track a known output file for dependency
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYFILE_PATH}
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" # Run Doxygen in the project root
        COMMENT "Generating Doxygen documentation"
        VERBATIM # Ensure arguments are passed literally
    )

    # Marking it ALL ensures it gets built with the default build target.
    add_custom_target(generate_documentation ALL
        DEPENDS "${DOXYGEN_OUTPUT_DIR}/html/index.html"
        COMMENT "Doxygen documentation generated"
    )

    # Install the generated documentation
    install(DIRECTORY "${DOXYGEN_OUTPUT_DIR}/" # Trailing slash copies directory contents
        DESTINATION "${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/docs"

        # Exclude common Doxygen internal files or backups from installation
        PATTERN "*.map" EXCLUDE
        PATTERN "*.md5" EXCLUDE
        PATTERN "*.tag" EXCLUDE
        PATTERN "*.bak" EXCLUDE
    )
endif()

# ==============================================================================
# TESTING
# ==============================================================================
option(LUMEX_BUILD_TESTS "Build the tests for LumexCore" OFF)

if(LUMEX_BUILD_TESTS)
    message(STATUS "Building with tests enabled")
    enable_testing()
    add_subdirectory(lumex/tests)

    add_custom_target(run_all_tests ALL
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        DEPENDS

        # Lumex Applied tests
        LumexHardwareCapabilitiesTests
        LumexLoggingTests
        LumexSettingsTests

        # Lumex Core tests
        LumexBase64Tests
        LumexEnvironmentTests
        LumexExceptionsTests
        LumexFilesystemTests
        LumexNumberGeneratorTests
        LumexOptionalTests
        LumexStringifyTests
        LumexStringViewTests
        LumexTemporaryTests
        LumexTimeTests
        COMMENT "Running all the tests..."
    )
endif()
