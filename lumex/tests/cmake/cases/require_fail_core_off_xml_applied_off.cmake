# XML stays ON while both CORE and APPLIED are OFF. Utility is
# effective-off, so XML must FATAL even with no applied consumers.

include("${LUMEX_SOURCE_DIR}/lumex/tests/cmake/setup_all_on.cmake")
set(LUMEX_BUILD_CORE OFF)
set(LUMEX_BUILD_APPLIED OFF)
include("${LUMEX_SOURCE_DIR}/cmake/LumexModules.cmake")
lumex_check_module_dependencies()
message(FATAL_ERROR "expected lumex_check_module_dependencies to stop")
