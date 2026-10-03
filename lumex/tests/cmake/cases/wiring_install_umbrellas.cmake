# Every module umbrella (an extensionless header under lumex/) must be named
# by the install rule of its own directory, and no header may be installed
# by two rules: a directory that installs its whole subtree with "*.hpp"
# must not contain another module directory that installs its own headers.

file(GLOB_RECURSE _files RELATIVE "${LUMEX_SOURCE_DIR}"
     "${LUMEX_SOURCE_DIR}/lumex/*")

set(_umbrellas "")
set(_subtree_dirs "")
foreach(_file IN LISTS _files)
    if(_file MATCHES "^lumex/(tests|examples)/")
        continue()
    endif()
    get_filename_component(_name "${_file}" NAME)
    get_filename_component(_dir "${_file}" DIRECTORY)
    if(_name STREQUAL "CMakeLists.txt")
        file(READ "${LUMEX_SOURCE_DIR}/${_file}" _text)
        if(_text MATCHES "install\\([ \t\r\n]*DIRECTORY[ \t\r\n]+\\\${CMAKE_CURRENT_SOURCE_DIR}/"
           AND _text MATCHES "PATTERN[ \t]+\"\\*\\.hpp\"")
            list(APPEND _subtree_dirs "${_dir}")
        endif()
    elseif(NOT _name MATCHES "\\." AND NOT _name MATCHES "^\\.")
        list(APPEND _umbrellas "${_file}")
    endif()
endforeach()

list(LENGTH _umbrellas _count)
if(_count LESS 20)
    message(FATAL_ERROR "found only ${_count} module umbrellas under lumex/")
endif()

set(_errors "")
foreach(_umbrella IN LISTS _umbrellas)
    get_filename_component(_name "${_umbrella}" NAME)
    get_filename_component(_dir "${_umbrella}" DIRECTORY)
    set(_cmake "${LUMEX_SOURCE_DIR}/${_dir}/CMakeLists.txt")
    if(NOT EXISTS "${_cmake}")
        string(APPEND _errors "\n  ${_umbrella}: no CMakeLists.txt next to it")
        continue()
    endif()
    file(READ "${_cmake}" _text)
    string(FIND "${_text}" "\"${_name}\"" _quoted)
    string(FIND "${_text}" "/${_name}" _path)
    if(_quoted EQUAL -1 AND _path EQUAL -1)
        string(APPEND _errors
               "\n  ${_umbrella}: ${_dir}/CMakeLists.txt does not install it")
    endif()
endforeach()

foreach(_outer IN LISTS _subtree_dirs)
    foreach(_inner IN LISTS _subtree_dirs)
        if(NOT _inner STREQUAL _outer AND _inner MATCHES "^${_outer}/")
            string(APPEND _errors
                   "\n  ${_inner}: its headers are also installed by the"
                   " \"*.hpp\" subtree rule of ${_outer}")
        endif()
    endforeach()
endforeach()

if(NOT _errors STREQUAL "")
    message(FATAL_ERROR "install rules of the module umbrellas:${_errors}")
endif()
