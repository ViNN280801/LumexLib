include("${LUMEX_SOURCE_DIR}/lumex/tests/cmake/setup_all_on.cmake")
set(LUMEX_BUILD_ENVIRONMENT OFF)
# TIME PUBLIC-links environment; disable it and its consumers so this case
# isolates TEMPORARY -> ENVIRONMENT.
set(LUMEX_BUILD_TIME OFF)
set(LUMEX_BUILD_EXCEPTIONS OFF)
set(LUMEX_BUILD_LOGGING OFF)
set(LUMEX_BUILD_RESOURCE_MONITOR OFF)
set(LUMEX_BUILD_SETTINGS OFF)
set(LUMEX_BUILD_HARDWARE OFF)
include("${LUMEX_SOURCE_DIR}/cmake/LumexModules.cmake")
lumex_check_module_dependencies()
message(FATAL_ERROR "expected lumex_check_module_dependencies to stop")
