# LUMEX_LOGGER_CONFIG_FORMAT=INI needs an effective settings module.

include("${LUMEX_SOURCE_DIR}/lumex/tests/cmake/setup_all_on.cmake")
set(LUMEX_BUILD_SETTINGS OFF)
set(LUMEX_LOGGER_CONFIG_FORMAT "INI")
include("${LUMEX_SOURCE_DIR}/cmake/LumexModules.cmake")
lumex_check_module_dependencies()
message(FATAL_ERROR "expected lumex_check_module_dependencies to stop")
