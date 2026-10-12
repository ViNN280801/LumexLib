# Before glibc 2.34, pthread_* and dladdr live in libpthread and libdl, and
# every shared library linked with -Wl,--no-undefined (LumexXml) and every
# executable must name them on its link line. Inline code in the utility
# headers calls them (dump: std::thread, pthread_sigmask; debug: dladdr), so
# lumex::utility carries both as PUBLIC dependencies, the stack-trace modules
# link libdl privately, and the installed package and the Conan recipe repeat
# the same dependencies for consumers.

function(_require_regex path regex what)
    file(READ "${LUMEX_SOURCE_DIR}/${path}" _txt)
    string(REGEX MATCH "${regex}" _match "${_txt}")
    if(_match STREQUAL "")
        message(FATAL_ERROR "${path}: ${what}")
    endif()
endfunction()

function(_require_text path needle)
    file(READ "${LUMEX_SOURCE_DIR}/${path}" _txt)
    string(FIND "${_txt}" "${needle}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR "${path} does not mention ${needle}")
    endif()
endfunction()

set(_utility "lumex/core/utility/CMakeLists.txt")
_require_regex("${_utility}" "find_package\\(Threads REQUIRED\\)"
    "the utility library does not find Threads")
_require_regex("${_utility}"
    "target_link_libraries\\(\\$\\{LUMEX_UTILITY_NAME\\} PUBLIC[^)]*Threads::Threads"
    "Threads::Threads is not a PUBLIC dependency of the utility library")
_require_regex("${_utility}"
    "target_link_libraries\\(\\$\\{LUMEX_UTILITY_NAME\\} PUBLIC[^)]*\\$\\{CMAKE_DL_LIBS\\}"
    "CMAKE_DL_LIBS is not a PUBLIC dependency of the utility library")

_require_regex("lumex/core/exceptions/CMakeLists.txt"
    "target_link_libraries\\(\\$\\{LUMEX_EXCEPTIONS_NAME\\} PRIVATE \\$\\{CMAKE_DL_LIBS\\}\\)"
    "the exceptions library does not link CMAKE_DL_LIBS")
_require_regex("lumex/applied/logger/CMakeLists.txt"
    "target_link_libraries\\(\\$\\{LUMEX_LOGGER_NAME\\} PRIVATE \\$\\{CMAKE_DL_LIBS\\}\\)"
    "the logger library does not link CMAKE_DL_LIBS")

# The package config must find Threads before it imports the targets that
# name Threads::Threads in their link interface.
file(READ "${LUMEX_SOURCE_DIR}/cmake/LumexLibConfig.cmake.in" _config)
string(FIND "${_config}" "find_dependency(Threads)" _find_pos)
string(FIND "${_config}" "Targets.cmake" _targets_pos)
if(_find_pos EQUAL -1)
    message(FATAL_ERROR
        "cmake/LumexLibConfig.cmake.in does not call find_dependency(Threads)")
endif()
if(_targets_pos EQUAL -1 OR NOT _find_pos LESS _targets_pos)
    message(FATAL_ERROR
        "cmake/LumexLibConfig.cmake.in must call find_dependency(Threads) "
        "before it includes the exported targets")
endif()

_require_text("conanfile.py" "utility.system_libs.append(\"pthread\")")
_require_text("conanfile.py" "utility.system_libs.append(\"dl\")")
_require_text("conanfile.py" "exceptions.system_libs.append(\"dl\")")
_require_text("conanfile.py" "logger.system_libs.append(\"dl\")")

# An example that starts std::thread itself must link Threads::Threads (the
# logger workflow failed with undefined pthread_create on glibc 2.28), and
# the example helper finds Threads when a LINK list names it.
_require_regex("lumex/examples/cmake/LumexExampleHelpers.cmake"
    "if\\(\"Threads::Threads\" IN_LIST LEX_LINK AND NOT TARGET Threads::Threads\\)[ \n]*find_package\\(Threads REQUIRED\\)"
    "the example helper does not find Threads for a LINK list that names it")
file(GLOB_RECURSE _example_sources "${LUMEX_SOURCE_DIR}/lumex/examples/*.cpp")
set(_threaded 0)
foreach(_source IN LISTS _example_sources)
    file(READ "${_source}" _text)
    string(FIND "${_text}" "#include <thread>" _uses_thread)
    if(_uses_thread EQUAL -1)
        continue()
    endif()
    math(EXPR _threaded "${_threaded} + 1")
    get_filename_component(_dir "${_source}" DIRECTORY)
    get_filename_component(_name "${_source}" NAME)
    file(READ "${_dir}/CMakeLists.txt" _lists)
    string(REGEX MATCH "lumex_example_executable\\([^)]*SOURCE ${_name}[^)]*\\)"
        _call "${_lists}")
    string(FIND "${_call}" "Threads::Threads" _links_threads)
    if(_call STREQUAL "" OR _links_threads EQUAL -1)
        message(FATAL_ERROR
            "${_source} includes <thread>, but its lumex_example_executable "
            "call does not LINK Threads::Threads")
    endif()
endforeach()
if(_threaded EQUAL 0)
    message(FATAL_ERROR
        "no example includes <thread>: the logger workflow example is gone "
        "or the scan no longer finds it")
endif()
