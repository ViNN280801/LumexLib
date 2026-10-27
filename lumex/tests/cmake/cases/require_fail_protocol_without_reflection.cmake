# protocol links reflection (cmake/LumexModules.cmake): with reflection OFF the protocol edge fails.

include("${LUMEX_SOURCE_DIR}/lumex/tests/cmake/setup_all_on.cmake")
set(LUMEX_BUILD_REFLECTION OFF)
# json and interprocess link reflection and are checked first.
set(LUMEX_BUILD_JSON OFF)
set(LUMEX_BUILD_INTERPROCESS OFF)
include("${LUMEX_SOURCE_DIR}/cmake/LumexModules.cmake")
lumex_check_module_dependencies()
message(FATAL_ERROR "expected lumex_check_module_dependencies to stop")
