# lumex/tests/applied/resource_monitor/CMakeLists.txt

# resource_monitor has one standard in lumex/tests/LumexTestStandards.cmake,
# C++17: LumexResourceMonitorCxx17Tests builds every *.cxx17.tests.cpp of this
# directory.
lumex_add_standard_suites(ResourceMonitor
    MODULE resource_monitor
    LINK
        lumex::resource_monitor
    TARGETS_VAR _resource_monitor_suites
)

# The tests call std::filesystem themselves (scratch directories):
# lumex_configure_target links stdc++fs / c++fs where the toolchain keeps it
# in a separate library (GCC 8).
foreach(_suite IN LISTS _resource_monitor_suites)
    set_target_properties(${_suite} PROPERTIES LUMEX_USES_STD_FILESYSTEM ON)
endforeach()
