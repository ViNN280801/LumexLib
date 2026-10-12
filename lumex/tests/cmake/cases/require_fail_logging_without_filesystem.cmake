# Earlier filesystem consumers must be OFF so the logging edge is first.

include("${LUMEX_SOURCE_DIR}/lumex/tests/cmake/setup_all_on.cmake")
set(LUMEX_BUILD_FILESYSTEM OFF)
set(LUMEX_BUILD_TEMPORARY OFF)
set(LUMEX_BUILD_EXCEPTIONS OFF)
set(LUMEX_BUILD_SETTINGS OFF)
include("${LUMEX_SOURCE_DIR}/cmake/LumexModules.cmake")
lumex_check_module_dependencies()
message(FATAL_ERROR "expected lumex_check_module_dependencies to stop")
