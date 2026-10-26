# framing links expected (cmake/LumexModules.cmake): with expected OFF the framing edge fails.

include("${LUMEX_SOURCE_DIR}/lumex/tests/cmake/setup_all_on.cmake")
set(LUMEX_BUILD_EXPECTED OFF)
# resource_monitor links expected and is checked first.
set(LUMEX_BUILD_RESOURCE_MONITOR OFF)
include("${LUMEX_SOURCE_DIR}/cmake/LumexModules.cmake")
lumex_check_module_dependencies()
message(FATAL_ERROR "expected lumex_check_module_dependencies to stop")
