# units links string_view (cmake/LumexModules.cmake): with string_view OFF the units
# edge fails. Every module that requires string_view, directly or through crc, and is
# checked before units is OFF, so that the units edge is the one that stops.

include("${LUMEX_SOURCE_DIR}/lumex/tests/cmake/setup_all_on.cmake")
set(LUMEX_BUILD_STRING_VIEW OFF)
set(LUMEX_BUILD_BASE64 OFF)
set(LUMEX_BUILD_CRC OFF)
set(LUMEX_BUILD_FRAMING OFF)
set(LUMEX_BUILD_XML OFF)
set(LUMEX_BUILD_EXCEPTIONS OFF)
set(LUMEX_BUILD_RESOURCE_MONITOR OFF)
set(LUMEX_BUILD_JSON OFF)
set(LUMEX_BUILD_PROTOCOL OFF)
set(LUMEX_BUILD_TRANSPORT OFF)
include("${LUMEX_SOURCE_DIR}/cmake/LumexModules.cmake")
lumex_check_module_dependencies()
message(FATAL_ERROR "expected lumex_check_module_dependencies to stop")
