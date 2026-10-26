# transport links serial (the bounded open; cmake/LumexModules.cmake): with
# serial OFF the transport edge fails.

include("${LUMEX_SOURCE_DIR}/lumex/tests/cmake/setup_all_on.cmake")
set(LUMEX_BUILD_SERIAL OFF)
include("${LUMEX_SOURCE_DIR}/cmake/LumexModules.cmake")
lumex_check_module_dependencies()
message(FATAL_ERROR "expected lumex_check_module_dependencies to stop")
