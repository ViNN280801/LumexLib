# Group OFF must not rewrite the per-module option value.

include("${LUMEX_SOURCE_DIR}/lumex/tests/cmake/setup_all_on.cmake")
include("${LUMEX_SOURCE_DIR}/cmake/LumexModules.cmake")

set(LUMEX_BUILD_CORE OFF)
if(NOT LUMEX_BUILD_BASE64)
    message(FATAL_ERROR "CORE=OFF must not FORCE LUMEX_BUILD_BASE64 off")
endif()
if(NOT LUMEX_BUILD_UTILITY)
    message(FATAL_ERROR "CORE=OFF must not FORCE LUMEX_BUILD_UTILITY off")
endif()

lumex_effective_on(_base64 LUMEX_BUILD_BASE64)
if(_base64)
    message(FATAL_ERROR "BASE64 must be effective OFF when CORE=OFF")
endif()

set(LUMEX_BUILD_APPLIED OFF)
if(NOT LUMEX_BUILD_LOGGER)
    message(FATAL_ERROR "APPLIED=OFF must not FORCE LUMEX_BUILD_LOGGER off")
endif()
lumex_effective_on(_logger LUMEX_BUILD_LOGGER)
if(_logger)
    message(FATAL_ERROR "LOGGER must be effective OFF when APPLIED=OFF")
endif()
