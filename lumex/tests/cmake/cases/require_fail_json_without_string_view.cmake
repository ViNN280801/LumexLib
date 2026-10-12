include("${LUMEX_SOURCE_DIR}/lumex/tests/cmake/setup_all_on.cmake")
set(LUMEX_BUILD_STRING_VIEW OFF)
# Every module that requires string_view and is checked before json is off,
# so that json is the one that fails.
set(LUMEX_BUILD_BASE64 OFF)
set(LUMEX_BUILD_CRC OFF)
set(LUMEX_BUILD_EXCEPTIONS OFF)
set(LUMEX_BUILD_XML OFF)
set(LUMEX_BUILD_RESOURCE_MONITOR OFF)
include("${LUMEX_SOURCE_DIR}/cmake/LumexModules.cmake")
lumex_check_module_dependencies()
message(FATAL_ERROR "expected lumex_check_module_dependencies to stop")
