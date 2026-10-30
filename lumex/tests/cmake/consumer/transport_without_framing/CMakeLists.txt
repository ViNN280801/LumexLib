# Fixture of cmake.consumer_transport_without_framing (see main.cpp).
#
# The framed adapters of the transport (framed/) are compiled only when the
# framing module is built, as the logger adapter is: no required edge. With
# LUMEX_BUILD_FRAMING OFF the transport configures, builds and runs, its
# target does not link the framing module and does not define
# LUMEX_TRANSPORT_WITH_FRAMING, and the umbrella header leaves the framed
# headers out. The dialogue engine (protocol/) needs the framing module too:
# with the protocol module ON it stays out as well (no link to the protocol
# module, no LUMEX_TRANSPORT_WITH_PROTOCOL, no source of protocol/).

cmake_minimum_required(VERSION 3.16)
project(LumexTransportWithoutFraming LANGUAGES CXX)

if(NOT LUMEX_SOURCE_DIR)
    message(FATAL_ERROR "pass -DLUMEX_SOURCE_DIR=<LumexLib checkout>")
endif()

set(LUMEX_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_XML OFF CACHE BOOL "" FORCE)
foreach(_opt HARDWARE JSON LOGGER LOGGING RESOURCE_MONITOR SETTINGS FRAMING)
    set(LUMEX_BUILD_${_opt} OFF CACHE BOOL "" FORCE)
endforeach()
set(LUMEX_BUILD_TRANSPORT ON CACHE BOOL "" FORCE)
set(LUMEX_TRANSPORT_WITH_TESTING ON CACHE BOOL "" FORCE)

add_subdirectory("${LUMEX_SOURCE_DIR}" lumexlib)

if(TARGET lumex::framing)
    message(FATAL_ERROR "the framing module is configured although LUMEX_BUILD_FRAMING is OFF")
endif()
if(NOT TARGET lumex::transport)
    message(FATAL_ERROR "lumex::transport is missing: the transport must not need the framing module")
endif()
get_target_property(_transport_libs lumex::transport LINK_LIBRARIES)
if("${_transport_libs}" MATCHES "framing")
    message(FATAL_ERROR "lumex::transport links the framing module: ${_transport_libs}")
endif()
get_target_property(_transport_defs lumex::transport COMPILE_DEFINITIONS)
if("${_transport_defs}" MATCHES "LUMEX_TRANSPORT_WITH_FRAMING")
    message(FATAL_ERROR "lumex::transport defines LUMEX_TRANSPORT_WITH_FRAMING: ${_transport_defs}")
endif()
if(NOT TARGET lumex::protocol)
    message(FATAL_ERROR "the protocol module is missing: it does not need the framing module")
endif()
if("${_transport_libs}" MATCHES "protocol")
    message(FATAL_ERROR "lumex::transport links the protocol module without the framing module: ${_transport_libs}")
endif()
if("${_transport_defs}" MATCHES "LUMEX_TRANSPORT_WITH_PROTOCOL")
    message(FATAL_ERROR "lumex::transport defines LUMEX_TRANSPORT_WITH_PROTOCOL: ${_transport_defs}")
endif()
get_target_property(_transport_sources lumex::transport SOURCES)
foreach(_source IN LISTS _transport_sources)
    if(_source MATCHES "/framed/")
        message(FATAL_ERROR "lumex::transport compiles a framed source: ${_source}")
    endif()
    if(_source MATCHES "/protocol/")
        message(FATAL_ERROR "lumex::transport compiles a dialogue source: ${_source}")
    endif()
endforeach()

add_executable(transport_without_framing_consumer main.cpp)
target_link_libraries(transport_without_framing_consumer PRIVATE lumex::transport)
