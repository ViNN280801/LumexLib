# lumex/tests/applied/resource_monitor/process/detail/CMakeLists.txt

# resource_monitor has one standard in lumex/tests/LumexTestStandards.cmake,
# C++11: LumexResourceMonitorProcessDetailCxx11Tests builds every
# *.cxx11.tests.cpp of this directory.
lumex_add_standard_suites(ResourceMonitorProcessDetail
    MODULE resource_monitor
    LINK
        lumex::resource_monitor
)
