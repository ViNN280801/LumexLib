# lumex/tests/applied/transport/matrix/CMakeLists.txt

# LumexTransportMatrixCxx11Tests: the transport matrix. Every layer of the
# de-identified device tables (lumex/tests/core/protocol/devices) that has a
# wire form of its own, under every behaviour of the device simulator, over
# the mock transport of its wire, on a virtual clock: one CTest test per cell,
# transport.matrix.device_<letter>.<layer letter>.<stream|datagram>.<behaviour>,
# and a subset on a runner of four threads (transport.matrix.threads.*) for
# the thread sanitizer. The harness is LumexTransportMatrixSupport.{hpp,cpp};
# one translation unit per device keeps the compile time and memory of each
# in bounds. The matrix tests behaviour, not language features: C++11 only,
# label matrix. ../CMakeLists.txt adds this directory only when the transport
# compiles the engine and the simulator (protocol and framing modules).
lumex_add_standard_suites(TransportMatrix
    MODULE transport
    STANDARDS 11
    LINK lumex::transport lumex::protocol lumex::framing
    SOURCES
        LumexTransportMatrixSupport.hpp
        LumexTransportMatrixSupport.cpp
    DISCOVER_ARGS PROPERTIES TIMEOUT 60 LABELS matrix
)
