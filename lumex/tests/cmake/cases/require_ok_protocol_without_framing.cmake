# protocol does not link framing (cmake/LumexModules.cmake): with framing OFF
# the protocol stays ON and the dependency check passes.

include("${LUMEX_SOURCE_DIR}/lumex/tests/cmake/setup_all_on.cmake")
set(LUMEX_BUILD_FRAMING OFF)
# transport links framing.
set(LUMEX_BUILD_TRANSPORT OFF)
include("${LUMEX_SOURCE_DIR}/cmake/LumexModules.cmake")
lumex_check_module_dependencies()
if(NOT LUMEX_BUILD_PROTOCOL)
    message(FATAL_ERROR "LUMEX_BUILD_PROTOCOL was switched off")
endif()
