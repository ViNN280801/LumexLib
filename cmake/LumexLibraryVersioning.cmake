# LumexLibraryVersioning.cmake
# Lumex-specific wrapper for apply_library_versioning function
#
# This file provides a convenience wrapper that automatically uses Lumex-specific
# vendor information. For universal usage, see LibraryVersioning.cmake

# Include the universal function
include(${CMAKE_CURRENT_LIST_DIR}/LibraryVersioning.cmake)

# Lumex vendor information
set(LUMEX_VENDOR_NAME "Semykin Vladislav")
set(LUMEX_VENDOR_EMAIL "vladislav_semykin01@mail.ru")
set(LUMEX_VENDOR_COMPANY "Lumex Ltd.")

# Get current year dynamically
string(TIMESTAMP LUMEX_COPYRIGHT_YEAR "%Y")

# Wrapper function that calls the universal function with Lumex defaults
# Parameters:
# TARGET_NAME - Name of the target to apply versioning to
# LIBRARY_DESCRIPTION - Optional description of the library (defaults to target name)
function(lumex_apply_library_versioning TARGET_NAME)
  # Optional description parameter
  if(ARGC GREATER 1)
    set(LIB_DESCRIPTION "${ARGV1}")
  else()
    set(LIB_DESCRIPTION "${TARGET_NAME}")
  endif()

  # Call the universal function with Lumex-specific vendor information
  # TARGET_NAME is a positional argument (first parameter), not a named argument
  apply_library_versioning(
    ${TARGET_NAME}
    LIBRARY_DESCRIPTION "${LIB_DESCRIPTION}"
    PROJECT_VERSION "${PROJECT_VERSION}"
    PROJECT_NAME "${PROJECT_NAME}"
    VENDOR_NAME "${LUMEX_VENDOR_NAME}"
    VENDOR_EMAIL "${LUMEX_VENDOR_EMAIL}"
    VENDOR_COMPANY "${LUMEX_VENDOR_COMPANY}"
    COPYRIGHT_YEAR "${LUMEX_COPYRIGHT_YEAR}"
  )
endfunction()
