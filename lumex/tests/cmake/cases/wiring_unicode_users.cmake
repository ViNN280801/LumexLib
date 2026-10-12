# core/unicode users outside xml: filesystem (to_wide_string, from_wide_string,
# path::wstring), serial (two sources), hardware and resource_monitor (their
# Windows sources) convert text with lumex::core::unicode::convert::to_utf8 /
# to_wide instead of WideCharToMultiByte, MultiByteToWideChar, mbstowcs and
# wcstombs. unicode is header-only and the sources are the only users, so each
# module links lumex::unicode PRIVATE; the configure-time edge, the Conan
# requirement and the order of the subdirectories (unicode before filesystem)
# are pinned here. The dump header of core/utility keeps its own helper:
# unicode requires utility, so utility cannot include unicode.

function(_require_text path needle)
    file(READ "${LUMEX_SOURCE_DIR}/${path}" _txt)
    string(FIND "${_txt}" "${needle}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR "${path} does not mention ${needle}")
    endif()
endfunction()

function(_forbid_text path needle)
    file(READ "${LUMEX_SOURCE_DIR}/${path}" _txt)
    string(FIND "${_txt}" "${needle}" _pos)
    if(NOT _pos EQUAL -1)
        message(FATAL_ERROR "${path} mentions ${needle}")
    endif()
endfunction()

# The edges and the PRIVATE link of each user.
foreach(_user FILESYSTEM SERIAL HARDWARE RESOURCE_MONITOR)
    _require_text("cmake/LumexModules.cmake"
        "lumex_require_module(LUMEX_BUILD_${_user} LUMEX_BUILD_UNICODE)")
endforeach()
_require_text("lumex/core/filesystem/CMakeLists.txt"
    "target_link_libraries(\${LUMEX_FILESYSTEM_NAME} PRIVATE lumex::unicode)")
_require_text("lumex/applied/serial/CMakeLists.txt"
    "target_link_libraries(\${LUMEX_SERIAL_NAME} PRIVATE lumex::unicode)")
_require_text("lumex/applied/hardware/CMakeLists.txt"
    "target_link_libraries(\${LUMEX_HARDWARE_NAME} PRIVATE lumex::unicode)")
_require_text("lumex/applied/resource_monitor/CMakeLists.txt"
    "target_link_libraries(\${LUMEX_RESOURCE_MONITOR_NAME} PRIVATE lumex::unicode)")

# unicode is added before filesystem (the alias target must exist when
# filesystem links it); applied is added after core.
file(READ "${LUMEX_SOURCE_DIR}/lumex/core/CMakeLists.txt" _core)
string(FIND "${_core}" "lumex_add_subdirectory_if(LUMEX_BUILD_UNICODE unicode)" _unicode_at)
string(FIND "${_core}" "lumex_add_subdirectory_if(LUMEX_BUILD_FILESYSTEM filesystem)" _filesystem_at)
if(_unicode_at EQUAL -1 OR _filesystem_at EQUAL -1 OR _filesystem_at LESS _unicode_at)
    message(FATAL_ERROR "filesystem must be added after unicode, which it links")
endif()

# The Conan components list core_unicode among their requirements.
file(READ "${LUMEX_SOURCE_DIR}/conanfile.py" _conan)
foreach(_component "core_filesystem\", \"filesystem"
                   "applied_hardware\", \"hardware"
                   "applied_resource_monitor\", \"resource_monitor"
                   "applied_serial\", \"serial")
    string(REGEX MATCH "\"${_component}\"[^)]*\"core_unicode\"" _found "${_conan}")
    if(NOT _found)
        message(FATAL_ERROR
            "conanfile.py: ${_component} does not require core_unicode")
    endif()
endforeach()

# The sources use the module and keep no conversion of their own.
foreach(_source
        lumex/core/filesystem/fs/LumexFilesystem.cpp
        lumex/applied/serial/enumeration/LumexSerialPortEnumeration.cpp
        lumex/applied/serial/resolver/LumexPortProcessResolver.cpp
        lumex/applied/hardware/caps/LumexHardwareCapabilities.cpp
        lumex/applied/resource_monitor/process/LumexProcessMonitor.cpp)
    _require_text("${_source}" "core/unicode/convert/LumexUnicodeConvert.hpp")
    _forbid_text("${_source}" "WideCharToMultiByte")
    _forbid_text("${_source}" "MultiByteToWideChar")
    _forbid_text("${_source}" "mbstowcs")
    _forbid_text("${_source}" "wcstombs")
    _forbid_text("${_source}" "convert_wide_to_utf8")
endforeach()
_require_text("lumex/core/filesystem/fs/LumexFilesystem.cpp" "to_wide (str)")
_require_text("lumex/core/filesystem/fs/LumexFilesystem.cpp" "to_utf8 (wstr)")

# The dump header stays the exception: unicode requires utility, so the header
# of utility cannot include it. The comment says why.
_forbid_text("lumex/core/utility/dump/LumexCoreDumpGenerator.hpp"
    "#include \"lumex/core/unicode")
_require_text("lumex/core/utility/dump/LumexCoreDumpGenerator.hpp"
    "requires this module")
