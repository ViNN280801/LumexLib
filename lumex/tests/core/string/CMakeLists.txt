# lumex/tests/core/string/CMakeLists.txt

# The tests follow the source tree (lumex/core/string): text/ tests join,
# quote, the text constraints and the case conversion, utility/ tests
# stringify. Each directory has one suite per standard of the module
# (lumex/tests/LumexTestStandards.cmake).
# LumexStringTestFixtures.hpp holds the LumexStringifyTest fixture and the
# types the stringify and the case conversion tests share.
add_subdirectory(text)
add_subdirectory(utility)
