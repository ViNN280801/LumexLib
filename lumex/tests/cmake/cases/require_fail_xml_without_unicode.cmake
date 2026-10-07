# XML group ON, unicode module OFF: lumex::xml links lumex::unicode PUBLIC
# (the UTF transcoders of its parser and buffer conversions live there).

include("${LUMEX_SOURCE_DIR}/lumex/tests/cmake/setup_all_on.cmake")
set(LUMEX_BUILD_UNICODE OFF)
include("${LUMEX_SOURCE_DIR}/cmake/LumexModules.cmake")
lumex_check_module_dependencies()
message(FATAL_ERROR "expected lumex_check_module_dependencies to stop")
