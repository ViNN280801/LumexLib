# lumex/tests/core/units/api/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
#   LumexUnitsApiCxx11Tests  LumexUnitsRouteMix.cxx11.tests.cpp and its two
#                            units LumexUnitsRoute{Exact,TwoProduct}: units of
#                            different routes of the product in one program
#                            LumexUnitsLookup.cxx11.tests.cpp: the lookup of a
#                            symbol in both scripts, the shadowed symbols, the
#                            rules of the prefixes, the encoding aliases and
#                            the rejections, and the formatting of a symbol
#                            LumexUnitsConvert.cxx11.tests.cpp: the exact ratio,
#                            the conversions of the oracle, the spot values,
#                            the refusals and the temperatures
# and the same files at C++14 and C++17, and at C++20 with
# LumexUnitsLookup.cxx20.tests.cpp (the char8_t overloads).
lumex_add_standard_suites(UnitsApi
    MODULE units
    LINK lumex::units
    SOURCES
        ../LumexUnitsTestSupport.hpp
        ../oracle/LumexUnitsOracleData.hpp
    VARIANT two_product DEFINITIONS LUMEX_UNITS_ROUTE=2
    VARIANT exact DEFINITIONS LUMEX_UNITS_ROUTE=3
    VARIANT unsafe_math DEFINITIONS LUMEX_UNITS_TEST_UNSAFE_MATH=1
    TARGETS_VAR _lumex_units_api_targets
)
lumex_units_unsafe_math_options(${_lumex_units_api_targets})
