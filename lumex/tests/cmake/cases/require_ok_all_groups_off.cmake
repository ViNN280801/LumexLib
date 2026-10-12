# Every group OFF: no module is effective, so no edge can FATAL.

include("${LUMEX_SOURCE_DIR}/lumex/tests/cmake/setup_all_on.cmake")
set(LUMEX_BUILD_CORE OFF)
set(LUMEX_BUILD_APPLIED OFF)
set(LUMEX_BUILD_XML OFF)
include("${LUMEX_SOURCE_DIR}/cmake/LumexModules.cmake")
lumex_check_module_dependencies()
