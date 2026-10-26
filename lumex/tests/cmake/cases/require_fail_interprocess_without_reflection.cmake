# interprocess links reflection (cmake/LumexModules.cmake): with reflection OFF the interprocess edge fails.

include("${LUMEX_SOURCE_DIR}/lumex/tests/cmake/setup_all_on.cmake")
set(LUMEX_BUILD_REFLECTION OFF)
# Every module that requires reflection and is checked before interprocess is off,
# so that interprocess is the one that fails.
set(LUMEX_BUILD_JSON OFF)
include("${LUMEX_SOURCE_DIR}/cmake/LumexModules.cmake")
lumex_check_module_dependencies()
message(FATAL_ERROR "expected lumex_check_module_dependencies to stop")
