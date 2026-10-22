# lumex/applied/transport/CMakeLists.txt

set(LUMEX_TRANSPORT_NAME LumexApplied_transport)
set(EXPORT_TO LumexAppliedTargets)

# Asio (and Boost.Context for the yield_context token) come from the vendored
# trees or from the consumer's own targets (cmake/LumexAsio.cmake,
# cmake/LumexBoostContext.cmake).
lumex_setup_asio()
if(LUMEX_TRANSPORT_WITH_SPAWN)
    lumex_setup_boost_context()
endif()

file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
if(NOT LUMEX_TRANSPORT_WITH_TESTING)
    list(FILTER SOURCES EXCLUDE REGEX "/testing/")
endif()
# The adapter to the logger module (log/adapter/: install_lumex_logger_sink,
# lumex_logger_watch_timer) is compiled only when the logger is built. It is
# no required edge: without the logger the transport builds as before.
lumex_effective_on(_transport_with_logger LUMEX_BUILD_LOGGER)
if(NOT _transport_with_logger)
    list(FILTER SOURCES EXCLUDE REGEX "/log/adapter/")
endif()

# Always static (spec G6): the library holds Asio objects, and a static
# library keeps the consumer's binary the only owner of its io_context and of
# the transport's log slot.
add_library(${LUMEX_TRANSPORT_NAME} STATIC ${SOURCES})
add_library(lumex::transport ALIAS ${LUMEX_TRANSPORT_NAME})

target_include_directories(${LUMEX_TRANSPORT_NAME} PUBLIC
    $<BUILD_INTERFACE:${LumexLib_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(${LUMEX_TRANSPORT_NAME} PUBLIC
    lumex::serial
    lumex::expected
    lumex::span
    lumex::contracts
    lumex::utility
    lumex::3rdparty_asio
)
if(LUMEX_TRANSPORT_WITH_SPAWN)
    target_link_libraries(${LUMEX_TRANSPORT_NAME} PUBLIC
        lumex::3rdparty_boost_context)
    target_compile_definitions(${LUMEX_TRANSPORT_NAME} PUBLIC
        LUMEX_TRANSPORT_WITH_SPAWN=1)
endif()
if(LUMEX_TRANSPORT_WITH_TESTING)
    target_compile_definitions(${LUMEX_TRANSPORT_NAME} PUBLIC
        LUMEX_TRANSPORT_WITH_TESTING=1)
endif()
if(_transport_with_logger)
    target_link_libraries(${LUMEX_TRANSPORT_NAME} PUBLIC lumex::logger)
    target_compile_definitions(${LUMEX_TRANSPORT_NAME} PUBLIC
        LUMEX_TRANSPORT_WITH_LOGGER=1)
endif()
# The lowest log level compiled in, for the library and its consumers
# (LUMEX_TRANSPORT_LOG_MIN_LEVEL of log/LumexTransportLog.hpp: 0 trace .. 4
# error; TRACE passes nothing).
string(TOUPPER "${LUMEX_TRANSPORT_LOG_MIN_LEVEL}" _log_min)
set(_log_levels TRACE DEBUG INFO WARNING ERROR)
list(FIND _log_levels "${_log_min}" _log_min_index)
if(_log_min_index EQUAL -1)
    message(FATAL_ERROR
        "LUMEX_TRANSPORT_LOG_MIN_LEVEL must be TRACE, DEBUG, INFO, WARNING or "
        "ERROR, not '${LUMEX_TRANSPORT_LOG_MIN_LEVEL}'")
endif()
if(_log_min_index GREATER 0)
    target_compile_definitions(${LUMEX_TRANSPORT_NAME} PUBLIC
        LUMEX_TRANSPORT_LOG_MIN_LEVEL=${_log_min_index})
endif()

if(WIN32)
    # Asio's Windows headers want a target version; Windows 10, as the rest
    # of the library.
    target_compile_definitions(${LUMEX_TRANSPORT_NAME} PUBLIC
        _WIN32_WINNT=0x0A00)
endif()

set_target_properties(${LUMEX_TRANSPORT_NAME} PROPERTIES
    POSITION_INDEPENDENT_CODE ON)

lumex_apply_library_versioning(${LUMEX_TRANSPORT_NAME} "Lumex Transport Library")

if(LUMEX_INSTALL)
    install(TARGETS ${LUMEX_TRANSPORT_NAME} EXPORT ${EXPORT_TO}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
    # The adapter's header only with the adapter (it needs the logger).
    set(_transport_install_excludes "")
    if(NOT _transport_with_logger)
        set(_transport_install_excludes PATTERN "adapter" EXCLUDE)
    endif()
    install(
        DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
        DESTINATION include/lumex/applied/transport
        FILES_MATCHING
        PATTERN "*.hpp"
        PATTERN "LumexTransport"
        ${_transport_install_excludes}
    )
    # The public headers include Asio: the vendored copy goes with them.
    lumex_third_party_install(lumex_3rdparty_asio ${EXPORT_TO})
    if(LUMEX_TRANSPORT_WITH_SPAWN)
        lumex_third_party_install(lumex_3rdparty_boost_context ${EXPORT_TO})
    endif()
endif()
