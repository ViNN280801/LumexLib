# The transport adds no required edge to the framing module (its framed
# adapters are compiled only when framing is built, like the logger adapter):
# transport ON with framing OFF must not FATAL, and the other way round too.

include("${LUMEX_SOURCE_DIR}/lumex/tests/cmake/setup_all_on.cmake")

set(LUMEX_BUILD_FRAMING OFF)
set(LUMEX_BUILD_TRANSPORT ON)

include("${LUMEX_SOURCE_DIR}/cmake/LumexModules.cmake")
lumex_check_module_dependencies()

lumex_effective_on(_framing_on LUMEX_BUILD_FRAMING)
lumex_effective_on(_transport_on LUMEX_BUILD_TRANSPORT)
if(_framing_on)
    message(FATAL_ERROR "framing is effective although LUMEX_BUILD_FRAMING is OFF")
endif()
if(NOT _transport_on)
    message(FATAL_ERROR "transport is not effective")
endif()

# And with framing ON and transport OFF: framing never needs the transport.
set(LUMEX_BUILD_FRAMING ON)
set(LUMEX_BUILD_TRANSPORT OFF)
lumex_check_module_dependencies()
