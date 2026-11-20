# lumex/tests/core/units/data/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
#   LumexUnitsDataCxx11Tests  LumexUnitsData.cxx11.tests.cpp: the catalog in
#                             constant expressions at C++11, the shape of every
#                             row and the prefix table
#                             LumexUnitsOracle.cxx11.tests.cpp: every row and
#                             every prefix against the oracle
# and the same files at C++14, C++17 and C++20.
lumex_add_standard_suites(UnitsData
    MODULE units
    LINK lumex::units
    SOURCES
        ../LumexUnitsTestSupport.hpp
        ../oracle/LumexUnitsOracleData.hpp
    VARIANT two_product DEFINITIONS LUMEX_UNITS_ROUTE=2
    VARIANT exact DEFINITIONS LUMEX_UNITS_ROUTE=3
    VARIANT unsafe_math DEFINITIONS LUMEX_UNITS_TEST_UNSAFE_MATH=1
    TARGETS_VAR _lumex_units_data_targets
)
lumex_units_unsafe_math_options(${_lumex_units_data_targets})
