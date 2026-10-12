# lumex/tests/core/base64/CMakeLists.txt

# The tests follow the source tree (lumex/core/base64): one directory and one
# suite per standard (lumex/tests/LumexTestStandards.cmake) for each of encode,
# decode and validate. LumexBase64TestFixtures.hpp holds the fixtures the three
# directories share.
add_subdirectory(decode)
add_subdirectory(encode)
add_subdirectory(validate)
