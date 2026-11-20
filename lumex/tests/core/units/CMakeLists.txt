# lumex/tests/core/units/CMakeLists.txt

# The tests follow the source tree (lumex/core/units):
#   data/    the catalog as a table: constant expressions at C++11, the shape
#            of every row, and every row against the oracle
#   api/     the lookup and the formatting of symbols, the exact ratio and the
#            conversions of values
#   oracle/  oracle_units.py, an independent transcription of the standards,
#            and LumexUnitsOracleData.hpp, the C++ data it writes (registered
#            as the CTest units.oracle_up_to_date in lumex/tests/CMakeLists.txt,
#            which regenerates it with Python 3 and fails on a difference)
#   this one LumexUnitsGlobalNames.cxx11.tests.cpp, the test of the module
#            umbrella, and the support header that every directory uses
# The module is header-only C++11; the suites run at 11, 14, 17 and 20
# (lumex/tests/LumexTestStandards.cmake). CTest names carry the prefix of the
# directory ("units.", "units.data.", "units.api.") and the standard as the
# last part.
#
# The suites of the variant unsafe_math (LUMEX_UNITS_TEST_UNSAFE_MATH) are
# compiled with the flags that let the compiler reassociate a floating-point
# expression (GCC and clang: -funsafe-math-optimizations, which includes
# -fassociative-math; cl.exe and clang-cl: /fp:fast). The flags are for the
# compilation only: the link line has none, so the start-up code that clears
# the denormals (crtfastmath) is not linked. The route of the product must
# stay right under them (the header takes the exact route for /fp:fast and
# where the arithmetic has excess precision, and the cores of the other routes
# switch the reassociation off for themselves: an attribute of GCC, a pragma
# of clang 14 and later).
function(lumex_units_unsafe_math_options)
    foreach(_target IN LISTS ARGN)
        if(_target MATCHES "UnsafeMath")
            if(MSVC)
                target_compile_options(${_target} PRIVATE /fp:fast)
            else()
                target_compile_options(${_target} PRIVATE
                    -funsafe-math-optimizations)
            endif()
        endif()
    endforeach()
endfunction()

lumex_add_standard_suites(Units
    MODULE units
    LINK lumex::units
    SOURCES
        LumexUnitsTestSupport.hpp
        oracle/LumexUnitsOracleData.hpp
    VARIANT two_product DEFINITIONS LUMEX_UNITS_ROUTE=2
    VARIANT exact DEFINITIONS LUMEX_UNITS_ROUTE=3
    VARIANT unsafe_math DEFINITIONS LUMEX_UNITS_TEST_UNSAFE_MATH=1
    TARGETS_VAR _lumex_units_targets
)
lumex_units_unsafe_math_options(${_lumex_units_targets})

add_subdirectory(data)
add_subdirectory(api)
