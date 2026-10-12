# lumex/tests/core/hazard_pointer/engine/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake),
# each built from the files of its standard and the lower ones and linked with
# the library, which is compiled as C++11. LumexHazardPointerTestSupport.hpp
# of the module's directory holds what the directories share.
# STANDARDS narrows the module's row to 11 17 20 for this directory. Nothing in
# its tests or in the sources they test differs at C++14 from the next lower
# standard (the text of the directory's own files after preprocessing is the
# same, no __cplusplus threshold or LUMEX_HAS_* feature of those standards is
# used there; "Narrowing the row of one directory" in
# lumex/tests/LumexTestStandards.cmake), so the dropped suites would run the
# same code again; the suite names above for them do not exist.
lumex_add_standard_suites(HazardPointerEngine
    MODULE hazard_pointer
    LINK lumex::hazard_pointer
    SOURCES ../LumexHazardPointerTestSupport.hpp
    DISCOVER_ARGS PROPERTIES TIMEOUT 300
    STANDARDS 11 17 20
)
