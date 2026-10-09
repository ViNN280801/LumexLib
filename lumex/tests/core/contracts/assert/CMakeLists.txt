# lumex/tests/core/contracts/assert/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake),
# each built from the files of its standard and the lower ones and linked with
# the library, which is compiled as C++11. The Override files define the macro
# LUMEX_CONTRACTS_SEMANTIC before the header, the Native files mock the
# compiler's contract_assert: each is its own translation unit.
lumex_add_standard_suites(ContractsAssert
    MODULE contracts
    LINK lumex::contracts
    SOURCES ../LumexContractsTestSupport.hpp
    DISCOVER_ARGS PROPERTIES TIMEOUT 300
)
