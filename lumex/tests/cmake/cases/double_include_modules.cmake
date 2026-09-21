# include_guard(GLOBAL) must make a second include a no-op.

include("${LUMEX_SOURCE_DIR}/lumex/tests/cmake/setup_all_on.cmake")
include("${LUMEX_SOURCE_DIR}/cmake/LumexModules.cmake")
include("${LUMEX_SOURCE_DIR}/cmake/LumexModules.cmake")
lumex_check_module_dependencies()

if(NOT LUMEX_CORE_MODULE_OPTIONS)
    message(FATAL_ERROR "second include dropped LUMEX_CORE_MODULE_OPTIONS")
endif()
if(NOT LUMEX_APPLIED_MODULE_OPTIONS)
    message(FATAL_ERROR "second include dropped LUMEX_APPLIED_MODULE_OPTIONS")
endif()
