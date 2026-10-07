# lumex/tests/applied/resource_monitor/CMakeLists.txt

# The tests follow the source tree (lumex/applied/resource_monitor): monitor/
# and monitor/detail/ test the system sampler and the watch log, process/ and
# process/detail/ the process monitor.
add_subdirectory(monitor)
add_subdirectory(process)
