# lumex/tests/core/contracts/location/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake),
# each built from the files of its standard and the lower ones and linked with
# the library, which is compiled as C++11.
lumex_add_standard_suites(ContractsLocation
    MODULE contracts
    LINK lumex::contracts
    SOURCES ../LumexContractsTestSupport.hpp
)
