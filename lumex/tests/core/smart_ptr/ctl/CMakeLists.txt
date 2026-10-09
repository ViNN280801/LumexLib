# lumex/tests/core/smart_ptr/ctl/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake),
# each built from the files of its standard and the lower ones. The headers of
# the module's directory hold what the directories share.
lumex_add_standard_suites(SmartPtrCtl
    MODULE smart_ptr
    LINK lumex::smart_ptr
    SOURCES ../LumexSmartPtrTestSupport.hpp ../LumexSmartPtrTestScenarios.hpp
    DISCOVER_ARGS PROPERTIES TIMEOUT 300 LABELS tsan
)
