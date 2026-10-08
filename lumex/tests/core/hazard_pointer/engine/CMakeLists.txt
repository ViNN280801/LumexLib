# lumex/tests/core/hazard_pointer/engine/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake),
# each built from the files of its standard and the lower ones and linked with
# the library, which is compiled as C++11. LumexHazardPointerTestSupport.hpp
# of the module's directory holds what the directories share.
lumex_add_standard_suites(HazardPointerEngine
    MODULE hazard_pointer
    LINK lumex::hazard_pointer
    SOURCES ../LumexHazardPointerTestSupport.hpp
    DISCOVER_ARGS PROPERTIES TIMEOUT 300
)
