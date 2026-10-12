# lumex/tests/core/contracts/CMakeLists.txt
#
# The tests follow the source tree (lumex/core/contracts):
#   location/   source_location
#   violation/  the enumerations and contract_violation
#   handler/    the violation handler (install, nested, thread safety)
#   assert/     LUMEX_CONTRACT_ASSERT: the semantics table, the choice of the
#               semantic per translation unit, the native contract_assert
#               (mock), death tests, constant evaluation
#   this one    LumexContractsGlobalNames.cxx11.tests.cpp, the test of the
#               module umbrella: the module declares no name at global scope
#               but its macros
# Every directory builds one suite per standard of the module
# (lumex/tests/LumexTestStandards.cmake). LumexContractsTestSupport.hpp of this
# directory holds what they share.
lumex_add_standard_suites(Contracts
    MODULE contracts
    LINK lumex::contracts
)

add_subdirectory(location)
add_subdirectory(violation)
add_subdirectory(handler)
add_subdirectory(assert)
