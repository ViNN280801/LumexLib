# lumex/tests/core/generators/number_generator/CMakeLists.txt

# generators.number_generator has one standard in
# lumex/tests/LumexTestStandards.cmake, C++11: LumexNumberGeneratorCxx11Tests
# builds every *.cxx11.tests.cpp of this directory.
#
# LumexNumberGenerator.cxx11.tests.cpp is not a GoogleTest binary: it has its
# own main () with RUN_TEST () / TEST_ASSERT () macros and runs its whole suite
# whatever its arguments. Test discovery would run it with
# --gtest_list_tests, which it does not understand, so PLAIN_EXECUTABLE
# registers it as one CTest test instead:
# generators.number_generator.LumexNumberGeneratorTests.cxx11.
lumex_add_standard_suites(NumberGenerator
    MODULE generators.number_generator
    LINK
        lumex::number_generator
    PLAIN_EXECUTABLE
)
