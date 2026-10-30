# The HID transport (lumex/applied/transport/hid) is wired by its option
# LUMEX_TRANSPORT_WITH_HID: the transport's CMakeLists sets up the vendored
# hidapi and compiles hid/ only with it, links hidapi privately (no public
# header includes hidapi.h) and defines LUMEX_TRANSPORT_WITH_HID only then,
# installs the hidapi library for the consumers' link and leaves the headers
# out without it; the umbrella includes the HID headers only under the macro;
# the tests are added only with the option; no other transport source needs
# hid/; the Windows backend compiles hid.c alone (it includes
# hidapi_descriptor_reconstruct.c, a second compile defines its functions
# twice); nothing calls hid_exit; the conan component defines the macro and
# requires the hidapi component.

file(READ "${LUMEX_SOURCE_DIR}/lumex/applied/transport/CMakeLists.txt" _cmake)
if(NOT _cmake MATCHES "if\\(LUMEX_TRANSPORT_WITH_HID\\)[ \t\r\n]*lumex_setup_hidapi\\(\\)[ \t\r\n]*else\\(\\)[ \t\r\n]*list\\(FILTER SOURCES EXCLUDE REGEX \"\\^hid/\"\\)")
    message(FATAL_ERROR "the transport does not set up hidapi and compile hid/ only with LUMEX_TRANSPORT_WITH_HID")
endif()
if(NOT _cmake MATCHES "if\\(LUMEX_TRANSPORT_WITH_HID\\)[^\n]*\n([ \t]*#[^\n]*\n)*[ \t]*target_link_libraries\\(\\$\\{LUMEX_TRANSPORT_NAME\\} PRIVATE lumex::3rdparty_hidapi\\)[ \t\r\n]*target_compile_definitions\\(\\$\\{LUMEX_TRANSPORT_NAME\\} PUBLIC[ \t\r\n]*LUMEX_TRANSPORT_WITH_HID=1\\)")
    message(FATAL_ERROR "the transport does not link hidapi privately and define LUMEX_TRANSPORT_WITH_HID under the option")
endif()
string(REGEX MATCHALL "lumex::3rdparty_hidapi" _mentions "${_cmake}")
list(LENGTH _mentions _mention_count)
if(NOT _mention_count EQUAL 1)
    message(FATAL_ERROR "lumex::3rdparty_hidapi is named ${_mention_count} times in the transport's CMakeLists, expected only the conditional private link")
endif()
if(NOT _cmake MATCHES "if\\(NOT LUMEX_TRANSPORT_WITH_HID\\)[ \t\r\n]*list\\(APPEND _transport_install_excludes PATTERN \"hid\" EXCLUDE\\)")
    message(FATAL_ERROR "the install does not leave the HID headers out without the option")
endif()
if(NOT _cmake MATCHES "if\\(LUMEX_TRANSPORT_WITH_HID\\)[ \t\r\n]*lumex_third_party_install\\(lumex_3rdparty_hidapi \\$\\{EXPORT_TO\\}\\)")
    message(FATAL_ERROR "the install does not export the hidapi library with the transport")
endif()

file(READ "${LUMEX_SOURCE_DIR}/lumex/applied/transport/LumexTransport" _umbrella)
if(NOT _umbrella MATCHES "#if defined\\(LUMEX_TRANSPORT_WITH_HID\\)[ \t\r\n]*#include \"hid/LumexHidDevice.hpp\"[ \t\r\n]*#include \"hid/LumexHidReportDescriptor.hpp\"[ \t\r\n]*#include \"hid/LumexHidSelectors.hpp\"[ \t\r\n]*#endif")
    message(FATAL_ERROR "the umbrella does not include the HID headers under LUMEX_TRANSPORT_WITH_HID")
endif()

file(READ "${LUMEX_SOURCE_DIR}/lumex/tests/applied/transport/CMakeLists.txt" _tests)
if(NOT _tests MATCHES "if\\(LUMEX_TRANSPORT_WITH_HID\\)[ \t\r\n]*add_subdirectory\\(hid\\)")
    message(FATAL_ERROR "the HID tests are not added only with LUMEX_TRANSPORT_WITH_HID")
endif()

# No transport source outside hid/ (the umbrella aside) needs the HID files:
# the transport builds without them. The device watcher watches HID selectors
# too, so its two files include them, and only after
# `#if defined(LUMEX_TRANSPORT_WITH_HID)`.
file(GLOB_RECURSE _transport_files
    "${LUMEX_SOURCE_DIR}/lumex/applied/transport/*.hpp"
    "${LUMEX_SOURCE_DIR}/lumex/applied/transport/*.cpp")
foreach(_file IN LISTS _transport_files)
    if(_file MATCHES "/transport/hid/")
        continue()
    endif()
    file(READ "${_file}" _text)
    if(_text MATCHES "transport/hid/")
        if(NOT _file MATCHES "/transport/session/LumexDeviceWatcher\\.[ch]pp$")
            message(FATAL_ERROR "${_file} includes a HID file: the transport would not build without LUMEX_TRANSPORT_WITH_HID")
        endif()
        # The first include must come after the first guard.
        string(FIND "${_text}" "#if defined(LUMEX_TRANSPORT_WITH_HID)" _guard)
        string(FIND "${_text}" "transport/hid/" _include)
        if(_guard EQUAL -1 OR _guard GREATER _include)
            message(FATAL_ERROR "${_file} includes a HID file outside #if defined(LUMEX_TRANSPORT_WITH_HID)")
        endif()
    endif()
endforeach()

# LumexLib never calls hid_exit (another library's hidapi copy may share
# the process): no call in the library's sources.
file(GLOB_RECURSE _hid_files
    "${LUMEX_SOURCE_DIR}/lumex/applied/transport/hid/*.hpp"
    "${LUMEX_SOURCE_DIR}/lumex/applied/transport/hid/*.cpp")
if(NOT _hid_files)
    message(FATAL_ERROR "the HID transport has no source")
endif()
foreach(_file IN LISTS _hid_files)
    file(READ "${_file}" _text)
    if(_text MATCHES "hid_exit[ \t]*\\(")
        message(FATAL_ERROR "${_file} calls hid_exit")
    endif()
endforeach()

file(READ "${LUMEX_SOURCE_DIR}/cmake/LumexHidapi.cmake" _hidapi)
if(NOT _hidapi MATCHES "set\\(_sources \"\\$\\{_dir\\}/windows/hid.c\"\\)")
    message(FATAL_ERROR "the Windows backend must compile hid.c alone (it includes hidapi_descriptor_reconstruct.c)")
endif()

file(READ "${LUMEX_SOURCE_DIR}/cmake/LumexOptions.cmake" _options)
if(NOT _options MATCHES "option\\(LUMEX_TRANSPORT_WITH_HID ")
    message(FATAL_ERROR "LumexOptions.cmake has no LUMEX_TRANSPORT_WITH_HID option")
endif()

file(READ "${LUMEX_SOURCE_DIR}/conanfile.py" _conan)
if(NOT _conan MATCHES "\"LUMEX_TRANSPORT_WITH_HID=1\"")
    message(FATAL_ERROR "the conan component does not define LUMEX_TRANSPORT_WITH_HID")
endif()
if(NOT _conan MATCHES "\"applied_transport\", \"transport\", \\[\"LumexApplied_transport\"\],[ \t\r\n]*\\[[^]]*\"thirdparty_hidapi\"")
    message(FATAL_ERROR "the conan component of the transport does not require thirdparty_hidapi")
endif()
