# lumex/tests/applied/transport/CMakeLists.txt

# The tests follow the source tree (lumex/applied/transport): one directory per
# source directory, each with one suite per standard of the module
# (lumex/tests/LumexTestStandards.cmake).
add_subdirectory(datagram)
add_subdirectory(detail)
add_subdirectory(error)
add_subdirectory(log)
add_subdirectory(net)
add_subdirectory(serial)
add_subdirectory(session)
add_subdirectory(statistics)
add_subdirectory(stream)
add_subdirectory(testing)
add_subdirectory(time)

# The framed adapters are compiled only with the framing module
# (lumex/applied/transport/CMakeLists.txt), and so are their tests.
lumex_effective_on(_transport_tests_with_framing LUMEX_BUILD_FRAMING)
if(_transport_tests_with_framing)
    add_subdirectory(framed)
endif()

# The HID transport is compiled only with LUMEX_TRANSPORT_WITH_HID
# (lumex/applied/transport/CMakeLists.txt), and so are its tests.
if(LUMEX_TRANSPORT_WITH_HID)
    add_subdirectory(hid)
endif()

# The module's own suites: the completion token styles through the umbrella
# (LumexTransportTokens.cxx11, LumexTransportAwaitable.cxx20 from C++20),
# asio::deferred, per-operation cancellation, the executor of the handler
# (LumexTransportDeferred, -Cancellation, -OutstandingWork) and handlers that
# keep their owner (LumexTransportHandlerCycle). CTest runs each test in a
# process of its own; the timeout fails a test that hangs instead of blocking
# the run.
lumex_add_standard_suites(Transport
    MODULE transport
    LINK lumex::transport
    SOURCES LumexTransportTokenFixture.hpp
    DISCOVER_ARGS PROPERTIES TIMEOUT 120
)
