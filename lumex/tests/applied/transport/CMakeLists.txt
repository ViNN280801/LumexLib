# lumex/tests/applied/transport/CMakeLists.txt

# The tests follow the source tree (lumex/applied/transport): one directory per
# source directory, each with one suite per standard of the module
# (lumex/tests/LumexTestStandards.cmake).
add_subdirectory(datagram)
add_subdirectory(error)
add_subdirectory(log)
add_subdirectory(net)
add_subdirectory(serial)
add_subdirectory(stream)

# The module's own suites: the completion token styles through the umbrella
# (LumexTransportTokens.cxx11, LumexTransportAwaitable.cxx20 from C++20).
lumex_add_standard_suites(Transport
    MODULE transport
    LINK lumex::transport
    SOURCES LumexTransportTokenFixture.hpp
)
