# lumex/tests/core/filesystem/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
#   LumexFilesystemCxx11Tests  LumexFilesystem.cxx11.tests.cpp and
#                              LumexFilesystemReplaceFileContent.cxx11.tests.cpp
#   LumexFilesystemCxx20Tests  the same files at C++20, the standard many
#                              consumers compile the headers with
lumex_add_standard_suites(Filesystem
    MODULE filesystem
    LINK
        lumex::filesystem
        lumex::utility
)
