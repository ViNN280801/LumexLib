# lumex/tests/applied/transport/protocol/discovery/CMakeLists.txt

# LumexTransportProtocolDiscoveryCxx<std>Tests: async_discover<P, Wire> (lumex/
# applied/transport/protocol/discovery): every reason, mixed candidates on the
# mocks and a loopback TCP peer, the out-of-band sequence, the bound of the
# probes in parallel, the tokens, the cancellation and the lifetime. On a
# virtual clock, except the TCP peer; the invented table of the reconnect tests.
# ../CMakeLists.txt adds this directory only when the transport compiles the
# engine.
lumex_add_standard_suites(TransportProtocolDiscovery
    MODULE transport
    LINK lumex::transport lumex::protocol lumex::framing
    DISCOVER_ARGS PROPERTIES TIMEOUT 120
)
