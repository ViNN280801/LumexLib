# lumex/tests/applied/transport/protocol/client/CMakeLists.txt

# LumexTransportProtocolClientCxx<std>Tests: the generated client<P>
# (lumex/applied/transport/protocol/client) over the device simulator on the
# mocks: every form of a method, identify () and the layers, the lifetime and
# the threads. The sync calls run with a runner on another thread and real
# short deadlines; the rest on a virtual clock. The synthetic mixer table of
# LumexClientTestTables.hpp, and the de-identified device table of device J
# for one template over two wire forms. ../CMakeLists.txt adds this directory
# only when the transport compiles the engine.
lumex_add_standard_suites(TransportProtocolClient
    MODULE transport
    LINK lumex::transport lumex::protocol lumex::framing
    SOURCES
        LumexClientTestSupport.hpp
        LumexClientTestTables.hpp
    DISCOVER_ARGS PROPERTIES TIMEOUT 120
)
