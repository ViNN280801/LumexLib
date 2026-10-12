# lumex/tests/applied/resource_monitor/monitor/CMakeLists.txt

# resource_monitor has one standard in lumex/tests/LumexTestStandards.cmake,
# C++11: LumexResourceMonitorMonitorCxx11Tests builds every *.cxx11.tests.cpp
# of this directory.
lumex_add_standard_suites(ResourceMonitorMonitor
    MODULE resource_monitor
    LINK
        lumex::resource_monitor
)

add_subdirectory(detail)
