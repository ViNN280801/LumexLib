# lumex/tests/applied/transport/session/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
# LumexTransportSessionCxx<std>Tests runs the *.cxx11.tests.cpp files: the
# reconnect policy, stream and message sessions over mocks and TCP on
# loopback, the handler lifetime, the stress run, the selectors, the
# device watcher over a fake enumeration, and sessions and the watcher on a
# virtual clock.
lumex_add_standard_suites(TransportSession
    MODULE transport
    LINK lumex::transport
    SOURCES LumexSessionTestSupport.hpp
)
