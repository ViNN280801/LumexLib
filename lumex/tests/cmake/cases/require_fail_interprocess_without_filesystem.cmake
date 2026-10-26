# interprocess links filesystem (cmake/LumexModules.cmake): with filesystem OFF the interprocess edge fails.

include("${LUMEX_SOURCE_DIR}/lumex/tests/cmake/setup_all_on.cmake")
set(LUMEX_BUILD_FILESYSTEM OFF)
# Every module that requires filesystem and is checked before interprocess is off,
# so that interprocess is the one that fails.
set(LUMEX_BUILD_EXCEPTIONS OFF)
set(LUMEX_BUILD_LOGGING OFF)
set(LUMEX_BUILD_RESOURCE_MONITOR OFF)
set(LUMEX_BUILD_SETTINGS OFF)
set(LUMEX_BUILD_TEMPORARY OFF)
set(LUMEX_BUILD_HARDWARE OFF)
set(LUMEX_BUILD_XML OFF)
include("${LUMEX_SOURCE_DIR}/cmake/LumexModules.cmake")
lumex_check_module_dependencies()
message(FATAL_ERROR "expected lumex_check_module_dependencies to stop")
