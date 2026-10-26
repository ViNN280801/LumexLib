# interprocess links expected (cmake/LumexModules.cmake): with expected OFF the interprocess edge fails.

include("${LUMEX_SOURCE_DIR}/lumex/tests/cmake/setup_all_on.cmake")
set(LUMEX_BUILD_EXPECTED OFF)
# Every module that requires expected and is checked before interprocess is off,
# so that interprocess is the one that fails.
set(LUMEX_BUILD_FRAMING OFF)
set(LUMEX_BUILD_RESOURCE_MONITOR OFF)
set(LUMEX_BUILD_TRANSPORT OFF)
include("${LUMEX_SOURCE_DIR}/cmake/LumexModules.cmake")
lumex_check_module_dependencies()
message(FATAL_ERROR "expected lumex_check_module_dependencies to stop")
