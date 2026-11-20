# units links expected (cmake/LumexModules.cmake): with expected OFF the units edge
# fails. Every module that requires expected and is checked before units is OFF,
# so that the units edge is the one that stops.

include("${LUMEX_SOURCE_DIR}/lumex/tests/cmake/setup_all_on.cmake")
set(LUMEX_BUILD_EXPECTED OFF)
set(LUMEX_BUILD_RESOURCE_MONITOR OFF)
set(LUMEX_BUILD_FRAMING OFF)
set(LUMEX_BUILD_INTERPROCESS OFF)
set(LUMEX_BUILD_PROTOCOL OFF)
set(LUMEX_BUILD_TRANSPORT OFF)
include("${LUMEX_SOURCE_DIR}/cmake/LumexModules.cmake")
lumex_check_module_dependencies()
message(FATAL_ERROR "expected lumex_check_module_dependencies to stop")
