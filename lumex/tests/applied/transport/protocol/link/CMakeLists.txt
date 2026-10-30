# lumex/tests/applied/transport/protocol/link/CMakeLists.txt

# LumexTransportProtocolLinkCxx<std>Tests: frame_link over a framed stream
# and a framed datagram transport on the mocks: frames both ways, the report
# ID of the datagram link, cancel, close and the empty link.
# ../CMakeLists.txt adds this directory only when the transport compiles the
# engine (protocol and framing modules). C++11 and C++20.
lumex_add_standard_suites(TransportProtocolLink
    MODULE transport
    STANDARDS 11 20
    LINK lumex::transport lumex::protocol lumex::framing
    DISCOVER_ARGS PROPERTIES TIMEOUT 120
)
