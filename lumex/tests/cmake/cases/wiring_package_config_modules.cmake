# Every module of cmake/LumexModules.cmake reaches an installed consumer: the
# package config (cmake/LumexLibConfig.cmake.in) defines the short alias
# lumex::<module> and lists <module> in the umbrella lumex::Lumex. A new module
# added to the option lists but not to the config would install, and no
# find_package (LumexLib) consumer could name it (framing was missed this way
# in 2.0.3.0).

include("${LUMEX_SOURCE_DIR}/cmake/LumexModules.cmake")
file(READ "${LUMEX_SOURCE_DIR}/cmake/LumexLibConfig.cmake.in" _config)

# The umbrella list: the words between "foreach(_c" and the closing ")".
string(FIND "${_config}" "foreach(_c" _start)
if(_start EQUAL -1)
    message(FATAL_ERROR "LumexLibConfig.cmake.in has no umbrella foreach(_c ...)")
endif()
string(SUBSTRING "${_config}" ${_start} -1 _tail)
string(FIND "${_tail}" ")" _end)
string(SUBSTRING "${_tail}" 0 ${_end} _umbrella)
string(REGEX REPLACE "[ \t\r\n]+" ";" _umbrella "${_umbrella}")

# Option name -> module name; generators is a group whose module is
# number_generator.
foreach(_opt ${LUMEX_CORE_MODULE_OPTIONS} ${LUMEX_APPLIED_MODULE_OPTIONS})
    string(REGEX REPLACE "^LUMEX_BUILD_" "" _name "${_opt}")
    string(TOLOWER "${_name}" _name)
    if(_name STREQUAL "generators")
        set(_name number_generator)
    endif()
    string(FIND "${_config}" "add_library(lumex::${_name} ALIAS" _alias)
    if(_alias EQUAL -1)
        message(FATAL_ERROR
            "LumexLibConfig.cmake.in defines no alias lumex::${_name} (${_opt})")
    endif()
    if(NOT _name IN_LIST _umbrella)
        message(FATAL_ERROR
            "the umbrella lumex::Lumex of LumexLibConfig.cmake.in omits ${_name} (${_opt})")
    endif()
endforeach()
