# lumex/tests/applied/resource_monitor/CMakeLists.txt

# resource_monitor has one standard in lumex/tests/LumexTestStandards.cmake,
# C++11: LumexResourceMonitorCxx11Tests builds every *.cxx11.tests.cpp of this
# directory.
lumex_add_standard_suites(ResourceMonitor
    MODULE resource_monitor
    LINK
        lumex::resource_monitor
)
