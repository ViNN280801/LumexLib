# lumex/tests/core/filesystem/fs/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
#   LumexFilesystemFsCxx11Tests  LumexFilesystem.cxx11.tests.cpp and
#                                LumexFilesystemReplaceFileContent.cxx11.tests.cpp
#   LumexFilesystemFsCxx17Tests  the same files at C++17 and
#                                LumexFilesystemStdPath.cxx17.tests.cpp: the
#                                conversions between lumex::path and
#                                std::filesystem::path
#   LumexFilesystemFsCxx20Tests  the same files at C++20, the standard many
#                                consumers compile the headers with
lumex_add_standard_suites(FilesystemFs
    MODULE filesystem
    LINK
        lumex::filesystem
        lumex::utility
    TARGETS_VAR _filesystem_suites
)

# The suites of C++17 and later call std::filesystem: before GCC 9 and LLVM 9
# it lives in a separate static archive, which link_std_filesystem finds (it
# adds nothing where the standard library has it all). The Clang standard
# library option of the suite goes into the probe.
foreach(_suite IN LISTS _filesystem_suites)
    get_target_property(_standard ${_suite} CXX_STANDARD)
    if(_standard AND _standard GREATER_EQUAL 17)
        get_target_property(_options ${_suite} COMPILE_OPTIONS)
        set(_stdlib_options "")
        foreach(_option IN LISTS _options)
            if(_option MATCHES "^-stdlib=")
                list(APPEND _stdlib_options "${_option}")
            endif()
        endforeach()
        link_std_filesystem(${_suite} PRIVATE
            COMPILE_OPTIONS ${_stdlib_options})
    endif()
endforeach()
