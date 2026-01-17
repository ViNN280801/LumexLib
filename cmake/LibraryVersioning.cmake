# ApplyLibraryVersioning.cmake
# Universal CMake module for applying versioning and vendor metadata to shared libraries
#
# This module provides a reusable function that can be used in any CMake project
# to automatically embed version information and vendor metadata into shared libraries
# (.dll on Windows, .so on Linux).
#
# Usage:
#   1. Copy this file to your project's cmake/ directory (or any directory in CMAKE_MODULE_PATH)
#   2. Include it in your CMakeLists.txt:
#      include(cmake/ApplyLibraryVersioning)  # or include(ApplyLibraryVersioning) if in CMAKE_MODULE_PATH
#   3. Call the function for each library:
#      apply_library_versioning(
#        TARGET_NAME MyLibrary
#        LIBRARY_DESCRIPTION "My Library Description"
#        PROJECT_VERSION "1.2.3"
#        PROJECT_NAME "MyProject"
#        VENDOR_NAME "John Doe"
#        VENDOR_EMAIL "john@example.com"
#        VENDOR_COMPANY "My Company Ltd."
#        COPYRIGHT_YEAR "2025"
#      )
#
# Parameters:
#   TARGET_NAME (required)
#     - Name of the CMake target (library) to apply versioning to
#
#   LIBRARY_DESCRIPTION (optional, defaults to TARGET_NAME)
#     - Description of the library (shown in Windows file properties)
#
#   PROJECT_VERSION (optional, defaults to ${PROJECT_VERSION} if defined)
#     - Version string in format "major.minor.patch" (e.g., "1.2.3")
#     - If not provided and PROJECT_VERSION is not set, function will fail
#
#   PROJECT_NAME (optional, defaults to ${PROJECT_NAME} if defined)
#     - Name of the project (shown in Windows file properties)
#     - If not provided and PROJECT_NAME is not set, defaults to TARGET_NAME
#
#   VENDOR_NAME (optional, defaults to empty string)
#     - Name of the vendor/author
#
#   VENDOR_EMAIL (optional, defaults to empty string)
#     - Email address of the vendor/author
#
#   VENDOR_COMPANY (optional, defaults to empty string)
#     - Company name
#
#   COPYRIGHT_YEAR (optional, defaults to current year)
#     - Copyright year (e.g., "2025")
#     - If not provided, automatically uses current year
#
# Requirements:
#   - CMake 3.10 or higher
#   - For Windows: RC language must be enabled in project() (add RC to LANGUAGES)
#   - For Linux: No special requirements
#
# Examples:
#
#   Example 1: Minimal usage (uses PROJECT_VERSION and PROJECT_NAME from project())
#   --------------------------------------------------------------------------------
#   project(MyProject VERSION 1.2.3 LANGUAGES CXX RC)
#   include(cmake/ApplyLibraryVersioning)  # Adjust path as needed
#
#   add_library(MyLib SHARED mylib.cpp)
#   apply_library_versioning(
#     TARGET_NAME MyLib
#     LIBRARY_DESCRIPTION "My Awesome Library"
#     VENDOR_NAME "John Doe"
#     VENDOR_EMAIL "john@example.com"
#     VENDOR_COMPANY "My Company Ltd."
#   )
#
#   Example 2: Full usage with all parameters
#   --------------------------------------------------------------------------------
#   include(cmake/ApplyLibraryVersioning)  # Adjust path as needed
#
#   add_library(MyLib SHARED mylib.cpp)
#   apply_library_versioning(
#     TARGET_NAME MyLib
#     LIBRARY_DESCRIPTION "My Awesome Library"
#     PROJECT_VERSION "2.0.1"
#     PROJECT_NAME "MyProject"
#     VENDOR_NAME "John Doe"
#     VENDOR_EMAIL "john@example.com"
#     VENDOR_COMPANY "My Company Ltd."
#     COPYRIGHT_YEAR "2025"
#   )
#
#   Example 3: Minimal usage (only required parameters)
#   --------------------------------------------------------------------------------
#   project(MyProject VERSION 1.0.0 LANGUAGES CXX RC)
#   include(cmake/ApplyLibraryVersioning)  # Adjust path as needed
#
#   add_library(MyLib SHARED mylib.cpp)
#   apply_library_versioning(TARGET_NAME MyLib)
#   # Uses: PROJECT_VERSION, PROJECT_NAME, current year, empty vendor fields
#
#   Example 4: Usage in subdirectory CMakeLists.txt
#   --------------------------------------------------------------------------------
#   # In subdirectory/CMakeLists.txt
#   include(cmake/ApplyLibraryVersioning)  # Adjust path as needed (relative to project root)
#
#   add_library(SubLib SHARED sublib.cpp)
#   apply_library_versioning(
#     TARGET_NAME SubLib
#     LIBRARY_DESCRIPTION "Subdirectory Library"
#     VENDOR_COMPANY "My Company"
#   )

# ==============================================================================
# UNIVERSAL FUNCTION
# ==============================================================================

function(apply_library_versioning TARGET_NAME)
  # Parse named arguments
  set(options "")
  set(oneValueArgs
    LIBRARY_DESCRIPTION
    PROJECT_VERSION
    PROJECT_NAME
    VENDOR_NAME
    VENDOR_EMAIL
    VENDOR_COMPANY
    COPYRIGHT_YEAR
  )
  set(multiValueArgs "")
  
  cmake_parse_arguments(PARSE_ARGV 1 ARG "${options}" "${oneValueArgs}" "${multiValueArgs}")
  
  # Validate TARGET_NAME
  if(NOT TARGET_NAME)
    message(FATAL_ERROR "apply_library_versioning: TARGET_NAME is required")
  endif()
  
  # Set defaults for optional parameters
  if(NOT ARG_LIBRARY_DESCRIPTION)
    set(ARG_LIBRARY_DESCRIPTION "${TARGET_NAME}")
  endif()
  
  if(NOT ARG_PROJECT_VERSION)
    if(DEFINED PROJECT_VERSION)
      set(ARG_PROJECT_VERSION "${PROJECT_VERSION}")
    else()
      message(FATAL_ERROR "apply_library_versioning: PROJECT_VERSION must be provided or PROJECT_VERSION must be set in project()")
    endif()
  endif()
  
  if(NOT ARG_PROJECT_NAME)
    if(DEFINED PROJECT_NAME)
      set(ARG_PROJECT_NAME "${PROJECT_NAME}")
    else()
      set(ARG_PROJECT_NAME "${TARGET_NAME}")
    endif()
  endif()
  
  if(NOT ARG_COPYRIGHT_YEAR)
    string(TIMESTAMP ARG_COPYRIGHT_YEAR "%Y")
  endif()
  
  # Default empty strings for vendor info if not provided
  if(NOT ARG_VENDOR_NAME)
    set(ARG_VENDOR_NAME "")
  endif()
  
  if(NOT ARG_VENDOR_EMAIL)
    set(ARG_VENDOR_EMAIL "")
  endif()
  
  if(NOT ARG_VENDOR_COMPANY)
    set(ARG_VENDOR_COMPANY "")
  endif()
  
  # Parse version string (e.g., "1.2.3" -> major=1, minor=2, patch=3)
  string(REPLACE "." ";" VERSION_LIST ${ARG_PROJECT_VERSION})
  list(LENGTH VERSION_LIST VERSION_LIST_LENGTH)
  
  if(VERSION_LIST_LENGTH LESS 3)
    message(FATAL_ERROR "apply_library_versioning: PROJECT_VERSION must be in format 'major.minor.patch' (e.g., '1.2.3')")
  endif()
  
  list(GET VERSION_LIST 0 VERSION_MAJOR)
  list(GET VERSION_LIST 1 VERSION_MINOR)
  list(GET VERSION_LIST 2 VERSION_PATCH)
  
  # Calculate version numbers
  # Windows uses: major.minor.patch.build format
  # Linux .so versioning: use SOVERSION = major.minor, VERSION = major.minor.patch
  set(SO_VERSION "${VERSION_MAJOR}.${VERSION_MINOR}")
  set(LIB_VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")
  set(DLL_VERSION "${VERSION_MAJOR}.${VERSION_MINOR}")
  
  # Skip INTERFACE libraries (they don't produce .so/.dll files)
  get_target_property(TARGET_TYPE ${TARGET_NAME} TYPE)
  if(TARGET_TYPE STREQUAL "INTERFACE_LIBRARY")
    message(STATUS "Skipping versioning for INTERFACE library: ${TARGET_NAME}")
    return()
  endif()
  
  # Apply version properties (works for both Windows and Linux)
  set_target_properties(${TARGET_NAME} PROPERTIES
    VERSION ${LIB_VERSION}
    SOVERSION ${SO_VERSION}
    OUTPUT_NAME ${TARGET_NAME}
  )
  
  # Windows-specific: Create version resource file (only for shared libraries)
  if(WIN32 AND BUILD_SHARED_LIBS)
    # Generate version resource file in build directory
    # CRITICAL: Must use "1 VERSIONINFO" not "VS_VERSION_INFO VERSIONINFO"
    set(VERSION_RC "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}_version.rc")
    
    # Build copyright string
    set(COPYRIGHT_STRING "")
    if(ARG_VENDOR_COMPANY)
      set(COPYRIGHT_STRING "Copyright (C) ${ARG_COPYRIGHT_YEAR} ${ARG_VENDOR_COMPANY}")
    else()
      set(COPYRIGHT_STRING "Copyright (C) ${ARG_COPYRIGHT_YEAR}")
    endif()
    
    # Generate version resource file
    set(RC_CONTENT
      "// Version resource file for ${TARGET_NAME}\n"
      "// Auto-generated by CMake - do not edit manually\n"
      "\n"
      "#include <windows.h>\n"
      "\n"
      "1 VERSIONINFO\n"
      "FILEVERSION ${VERSION_MAJOR},${VERSION_MINOR},${VERSION_PATCH},0\n"
      "PRODUCTVERSION ${VERSION_MAJOR},${VERSION_MINOR},${VERSION_PATCH},0\n"
      "FILEFLAGSMASK 0x3fL\n"
      "#ifdef _DEBUG\n"
      "FILEFLAGS VS_FF_DEBUG\n"
      "#else\n"
      "FILEFLAGS 0x0L\n"
      "#endif\n"
      "FILEOS VOS__WINDOWS32\n"
      "FILETYPE VFT_DLL\n"
      "FILESUBTYPE VFT2_UNKNOWN\n"
      "BEGIN\n"
      "  BLOCK \"StringFileInfo\"\n"
      "  BEGIN\n"
      "    BLOCK \"040904E4\"\n"
      "    BEGIN\n"
    )
    
    # Add vendor company if provided
    if(ARG_VENDOR_COMPANY)
      string(APPEND RC_CONTENT "      VALUE \"CompanyName\", \"${ARG_VENDOR_COMPANY}\\0\"\n")
    endif()
    
    # Add file description
    string(APPEND RC_CONTENT "      VALUE \"FileDescription\", \"${ARG_LIBRARY_DESCRIPTION}\\0\"\n")
    string(APPEND RC_CONTENT "      VALUE \"FileVersion\", \"${ARG_PROJECT_VERSION}\\0\"\n")
    string(APPEND RC_CONTENT "      VALUE \"InternalName\", \"${TARGET_NAME}\\0\"\n")
    string(APPEND RC_CONTENT "      VALUE \"LegalCopyright\", \"${COPYRIGHT_STRING}\\0\"\n")
    string(APPEND RC_CONTENT "      VALUE \"OriginalFilename\", \"${TARGET_NAME}.dll\\0\"\n")
    string(APPEND RC_CONTENT "      VALUE \"ProductName\", \"${ARG_PROJECT_NAME}\\0\"\n")
    string(APPEND RC_CONTENT "      VALUE \"ProductVersion\", \"${ARG_PROJECT_VERSION}\\0\"\n")
    
    # Add author and contact if provided
    if(ARG_VENDOR_NAME)
      string(APPEND RC_CONTENT "      VALUE \"Author\", \"${ARG_VENDOR_NAME}\\0\"\n")
    endif()
    
    if(ARG_VENDOR_EMAIL)
      string(APPEND RC_CONTENT "      VALUE \"Contact\", \"${ARG_VENDOR_EMAIL}\\0\"\n")
    endif()
    
    # Close StringFileInfo block
    string(APPEND RC_CONTENT
      "    END\n"
      "  END\n"
      "  BLOCK \"VarFileInfo\"\n"
      "  BEGIN\n"
      "    VALUE \"Translation\", 0x0409, 1252\n"
      "  END\n"
      "END\n"
    )
    
    # Write the file (configure time)
    file(WRITE ${VERSION_RC} ${RC_CONTENT})
    
    # Add version resource to target
    # CRITICAL: Must add as source file so CMake compiles it with RC compiler
    target_sources(${TARGET_NAME} PRIVATE ${VERSION_RC})
    
    # Set properties for RC file compilation
    if(MSVC)
      set_source_files_properties(${VERSION_RC} PROPERTIES
        COMPILE_FLAGS "/nologo"
      )
    endif()
    
    # Set linker flags for version info (optional, but helps with DLL version)
    set_target_properties(${TARGET_NAME} PROPERTIES
      LINK_FLAGS "/VERSION:${DLL_VERSION}"
    )
  endif()
  
  # Linux-specific: Set library version (affects .so file naming)
  if(UNIX AND BUILD_SHARED_LIBS)
    set_target_properties(${TARGET_NAME} PROPERTIES
      VERSION ${LIB_VERSION}
      SOVERSION ${SO_VERSION}
    )
  endif()
  
  # Set common properties
  set_target_properties(${TARGET_NAME} PROPERTIES
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
  )
  
  message(STATUS "Applied versioning to ${TARGET_NAME}: ${ARG_PROJECT_VERSION} (SO: ${SO_VERSION})")
endfunction()
