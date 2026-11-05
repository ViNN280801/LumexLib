# The package config includes the exported target files in the order of their
# dependencies: the applied modules (settings) link lumex::LumexXml, and an
# imported target that names a target not yet defined makes the include of its
# file fail ("The following imported targets are referenced, but are missing"),
# so find_package (LumexLib) of a full install failed with Applied before Xml.
# Core first, then Xml, then Applied.

file(READ "${LUMEX_SOURCE_DIR}/cmake/LumexLibConfig.cmake.in" _config)

set(_previous -1)
set(_previous_name "")
foreach(_name LumexCoreTargets LumexXmlTargets LumexAppliedTargets)
    string(FIND "${_config}" "include(\"\${CMAKE_CURRENT_LIST_DIR}/${_name}.cmake\")" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR "LumexLibConfig.cmake.in does not include ${_name}.cmake")
    endif()
    if(_pos LESS _previous)
        message(FATAL_ERROR
            "LumexLibConfig.cmake.in includes ${_name}.cmake before ${_previous_name}.cmake: "
            "the applied targets link lumex::LumexXml, so the order is Core, Xml, Applied")
    endif()
    set(_previous ${_pos})
    set(_previous_name ${_name})
endforeach()
