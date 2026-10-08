# lumex/tests/core/hazard_pointer/CMakeLists.txt

# The tests follow the source tree (lumex/core/hazard_pointer):
#   base/    hazard_pointer_obj_base and the protectable trait
#   holder/  hazard_pointer, make_hazard_pointer, swap, the batch functions
#            and the extension overloads
#   engine/  the slot pool, the thread cache, the retired lists and passes,
#            the stress tests and the deliberately wrong implementation
#   this one LumexHazardPointerGlobalNames.cxx11.tests.cpp (the module declares
#            nothing at global scope), and LumexHazardPointerTestSupport.hpp,
#            which the directories share
# Every directory has one suite per standard of the module (C++11, 14, 17,
# 20; lumex/tests/LumexTestStandards.cmake). The library is compiled as C++11
# and every suite links it. The tests that need a different build of the
# engine (the test hooks of the Dekker race, two shared libraries, static
# destruction, the rejected uses) are
# consumer fixtures under lumex/tests/cmake/consumer/hazard_pointer_*.
lumex_add_standard_suites(HazardPointer
    MODULE hazard_pointer
    LINK lumex::hazard_pointer
    SOURCES LumexHazardPointerTestSupport.hpp
    DISCOVER_ARGS PROPERTIES TIMEOUT 300
)

add_subdirectory(base)
add_subdirectory(holder)
add_subdirectory(engine)
