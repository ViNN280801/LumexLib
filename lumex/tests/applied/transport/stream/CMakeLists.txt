# lumex/tests/applied/transport/stream/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
# LumexTransportStreamCxx<std>Tests runs LumexByteStream.cxx11.tests.cpp
# (byte_stream and io_runner over the mock stream) and
# LumexByteStreamWriteAllCancel.cxx11.tests.cpp (write_all cancelled between
# two of its writes, over a device of the test).
lumex_add_standard_suites(TransportStream
    MODULE transport
    LINK lumex::transport
)
