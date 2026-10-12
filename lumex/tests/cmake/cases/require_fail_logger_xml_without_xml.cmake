# LUMEX_LOGGER_CONFIG_FORMAT=XML needs an effective XML module.

include("${LUMEX_SOURCE_DIR}/lumex/tests/cmake/setup_all_on.cmake")
set(LUMEX_BUILD_XML OFF)
set(LUMEX_LOGGER_CONFIG_FORMAT "XML")
include("${LUMEX_SOURCE_DIR}/cmake/LumexModules.cmake")
lumex_check_module_dependencies()
message(FATAL_ERROR "expected lumex_check_module_dependencies to stop")
