# Field-reflection option and vendored nlohmann wiring. Boost.PFR must
# not appear in option text or in the reflection CMakeLists.

file(READ "${LUMEX_SOURCE_DIR}/cmake/LumexOptions.cmake" _opts)
string(FIND "${_opts}" "option(LUMEX_WITH_FIELD_REFLECTION " _pos)
if(_pos EQUAL -1)
    message(FATAL_ERROR "missing option(LUMEX_WITH_FIELD_REFLECTION")
endif()
if("${_opts}" MATCHES "boost::pfr")
    message(FATAL_ERROR "LumexOptions.cmake still mentions boost::pfr")
endif()
if("${_opts}" MATCHES "boost_pfr")
    message(FATAL_ERROR "LumexOptions.cmake still mentions boost_pfr")
endif()

if(NOT EXISTS "${LUMEX_SOURCE_DIR}/3rdparty/nlohmann/json.hpp")
    message(FATAL_ERROR "missing 3rdparty/nlohmann/json.hpp")
endif()
if(NOT EXISTS "${LUMEX_SOURCE_DIR}/cmake/LumexNlohmannJson.cmake")
    message(FATAL_ERROR "missing cmake/LumexNlohmannJson.cmake")
endif()
if(NOT EXISTS
        "${LUMEX_SOURCE_DIR}/lumex/core/reflection/field_reflection/LumexAggregateFields.hpp")
    message(FATAL_ERROR "missing LumexAggregateFields.hpp")
endif()

file(READ "${LUMEX_SOURCE_DIR}/CMakeLists.txt" _root)
string(FIND "${_root}" "include(cmake/LumexNlohmannJson.cmake)" _npos)
if(_npos EQUAL -1)
    message(FATAL_ERROR "root CMakeLists.txt does not include LumexNlohmannJson.cmake")
endif()

file(READ "${LUMEX_SOURCE_DIR}/cmake/LumexNlohmannJson.cmake" _nj)
foreach(_needle
        "lumex_setup_nlohmann_json"
        "nlohmann_json::nlohmann_json"
        "nlohmann/json.hpp"
        "FATAL_ERROR")
    string(FIND "${_nj}" "${_needle}" _p)
    if(_p EQUAL -1)
        message(FATAL_ERROR "LumexNlohmannJson.cmake omits ${_needle}")
    endif()
endforeach()
if("${_nj}" MATCHES "install\\(EXPORT")
    message(FATAL_ERROR "nlohmann target must not be exported")
endif()
