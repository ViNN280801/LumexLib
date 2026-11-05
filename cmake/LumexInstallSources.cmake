# cmake/LumexInstallSources.cmake
#
# The optional parts of the package that are sources, not binaries, under
# share/ (user, 2026-11-05):
#
#   share/examples/<dir>/    lumex/examples/<dir> of every module that is built,
#   share/examples/cmake/    the helper the example directories include,
#   share/benchmarks/<dir>/  benchmarks/<dir> of every module that is built.
#
# Each directory is a project of its own: cmake -S <dir> -B build
# -DLumexLib_DIR=<prefix>/lib/cmake/LumexLib builds it against the installed
# package. No binary of an example or a benchmark is installed, so nothing
# needs a RUNPATH or a DLL copy for them.
#
# The name of a directory decides which module it belongs to: <dir> is
# installed when LUMEX_BUILD_<DIR> is effective (cmake/LumexModules.cmake,
# lumex_effective_on). A directory without such an option stops the configure,
# so a new one cannot be forgotten (cmake.wiring_examples_standalone).
#
# The documentation (share/doc) is installed by the root CMakeLists.txt.

include_guard(GLOBAL)

# lumex_installable_source_dirs(<out> <root>)
#
# The names of the subdirectories of <root> that the options of this build
# select, sorted.
function(lumex_installable_source_dirs out_var root)
    file(GLOB _entries LIST_DIRECTORIES true RELATIVE "${root}" "${root}/*")
    list(SORT _entries)
    set(_selected "")
    foreach(_name IN LISTS _entries)
        if(NOT IS_DIRECTORY "${root}/${_name}")
            continue()
        endif()
        # A directory of helpers (examples/cmake) is not a module.
        if(NOT EXISTS "${root}/${_name}/CMakeLists.txt"
           AND NOT EXISTS "${root}/${_name}/README.md")
            continue()
        endif()
        if(_name STREQUAL "cmake")
            continue()
        endif()
        string(TOUPPER "${_name}" _upper)
        set(_option "LUMEX_BUILD_${_upper}")
        if(NOT DEFINED ${_option})
            message(FATAL_ERROR
                "${root}/${_name} has no option ${_option}: the install rule "
                "decides by the name of the directory (cmake/LumexInstallSources.cmake)")
        endif()
        lumex_effective_on(_on ${_option})
        if(NOT _on)
            continue()
        endif()
        # The transport benchmarks run the dialogue engine against the testing
        # doubles of the transport (root CMakeLists.txt builds them under the
        # same conditions).
        if(root MATCHES "/benchmarks$" AND _name STREQUAL "transport")
            lumex_effective_on(_protocol_on LUMEX_BUILD_PROTOCOL)
            lumex_effective_on(_framing_on LUMEX_BUILD_FRAMING)
            if(NOT _protocol_on OR NOT _framing_on OR NOT LUMEX_TRANSPORT_WITH_TESTING)
                continue()
            endif()
        endif()
        list(APPEND _selected "${_name}")
    endforeach()
    set(${out_var} "${_selected}" PARENT_SCOPE)
endfunction()

# lumex_install_example_sources()
#
# Under LUMEX_INSTALL_EXAMPLES: lumex/examples/<dir> to share/examples/<dir>
# for the modules that are built, with the shared helpers.
function(lumex_install_example_sources)
    set(_root "${LumexLib_SOURCE_DIR}/lumex/examples")
    lumex_installable_source_dirs(_dirs "${_root}")
    foreach(_name IN LISTS _dirs)
        # The udev rules of the transport go to share/LumexLib/udev, where the
        # permission hint of the library names them (applied/transport).
        install(
            DIRECTORY "${_root}/${_name}/"
            DESTINATION "${CMAKE_INSTALL_DATADIR}/examples/${_name}"
            PATTERN "udev" EXCLUDE
        )
    endforeach()
    install(
        DIRECTORY "${LumexLib_SOURCE_DIR}/lumex/examples/cmake/"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/examples/cmake"
    )
    install(
        FILES "${_root}/README.md"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/examples"
    )
endfunction()

# lumex_install_benchmark_sources()
#
# Under LUMEX_INSTALL_BENCHMARKS: benchmarks/<dir> to share/benchmarks/<dir>
# for the modules that are built: the sources, the CMakeLists.txt, the README,
# the plotting and run scripts and the recorded results. compile_time.cpp and
# compile_time.py compile with -I<checkout> and read the tests of the
# checkout, so they stay there; a directory that has nothing else (protocol)
# is installed as its README and its results.
function(lumex_install_benchmark_sources)
    set(_root "${LumexLib_SOURCE_DIR}/benchmarks")
    lumex_installable_source_dirs(_dirs "${_root}")
    foreach(_name IN LISTS _dirs)
        if(EXISTS "${_root}/${_name}/CMakeLists.txt")
            install(
                DIRECTORY "${_root}/${_name}/"
                DESTINATION "${CMAKE_INSTALL_DATADIR}/benchmarks/${_name}"
                PATTERN "compile_time.*" EXCLUDE
                PATTERN "__pycache__" EXCLUDE
            )
        else()
            # A directory with no program (the compile time of the protocol
            # tables): what it recorded, and how.
            install(
                FILES "${_root}/${_name}/README.md"
                DESTINATION "${CMAKE_INSTALL_DATADIR}/benchmarks/${_name}"
            )
            install(
                DIRECTORY "${_root}/${_name}/results/"
                DESTINATION "${CMAKE_INSTALL_DATADIR}/benchmarks/${_name}/results"
            )
        endif()
    endforeach()
    install(
        FILES "${_root}/README.md"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/benchmarks"
    )
endfunction()
