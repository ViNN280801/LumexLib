# lumex/tests/applied/transport/hid/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
# LumexTransportHidCxx<std>Tests runs the *.cxx11.tests.cpp files: the report
# descriptor analysis (the DChannel cases and the sizes per report ID) and
# hid_device over a fake hidapi seam (LumexFakeHidapi.hpp): the report ID
# both ways, padding, report_too_large, the reader thread and the runtime,
# cancellation, the token styles and the statistics; the HID selectors, the
# factory they make (the device found at each open, a message_session that
# reconnects to a device that came back under another path) and the device
# watcher over a fake HID enumeration. ../CMakeLists.txt adds
# this directory only with LUMEX_TRANSPORT_WITH_HID.
lumex_add_standard_suites(TransportHid
    MODULE transport
    LINK lumex::transport
    SOURCES LumexFakeHidapi.hpp
    DISCOVER_ARGS PROPERTIES TIMEOUT 120
)
