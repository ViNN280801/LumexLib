# The default of LUMEX_TRANSPORT_WITH_HID and the configure message that goes
# with an OFF: the libusb backend without libusb-1.0 (or the hidraw backend
# without libudev, or without pkg-config) no longer drops HID silently.

include("${LUMEX_SOURCE_DIR}/cmake/LumexHidDefault.cmake")

# <backend> <own backend> <pkg-config found> <library found> -> <default> and
# the words the message must contain (none: no message).
function(_expect backend own pkg library expected_default)
    lumex_hid_default_decision(_default _message "${backend}" ${own} ${pkg}
        ${library})
    if(NOT "${_default}" STREQUAL "${expected_default}")
        message(FATAL_ERROR
            "${backend} own=${own} pkg=${pkg} library=${library}: default "
            "${_default}, expected ${expected_default}")
    endif()
    set(_expected_words ${ARGN})
    if(NOT _expected_words AND NOT "${_message}" STREQUAL "")
        message(FATAL_ERROR
            "${backend} own=${own} pkg=${pkg} library=${library}: "
            "unexpected message: ${_message}")
    endif()
    foreach(_word IN LISTS _expected_words)
        string(FIND "${_message}" "${_word}" _pos)
        if(_pos EQUAL -1)
            message(FATAL_ERROR
                "${backend} own=${own} pkg=${pkg} library=${library}: the "
                "message does not contain '${_word}': ${_message}")
        endif()
    endforeach()
endfunction()

# Windows and Apple carry their own backend: ON, nothing to say.
_expect(hidraw ON OFF OFF ON)
_expect(libusb ON OFF OFF ON)
# The library is there: ON, nothing to say.
_expect(hidraw OFF ON ON ON)
_expect(libusb OFF ON ON ON)
# The library is missing: OFF, and the message names the backend, the library,
# the package and the way out.
_expect(libusb OFF ON OFF OFF
    "libusb backend" "libusb-1.0" "libusb-1.0-0-dev" "LUMEX_TRANSPORT_WITH_HID=ON")
_expect(hidraw OFF ON OFF OFF
    "hidraw backend" "libudev" "libudev-dev" "LUMEX_TRANSPORT_WITH_HID=ON")
# No pkg-config: the library cannot be looked for; OFF, and the message says so.
_expect(libusb OFF OFF OFF OFF "pkg-config was not found" "libusb-1.0")
_expect(hidraw OFF OFF OFF OFF "pkg-config was not found" "libudev")

# LumexOptions.cmake takes its default from the decision and reports it: a
# warning for the explicitly chosen libusb backend, a status line for hidraw.
file(READ "${LUMEX_SOURCE_DIR}/cmake/LumexOptions.cmake" _options)
foreach(_needle
        "LumexHidDefault.cmake"
        "lumex_hid_default_decision(_lumex_hid_default _lumex_hid_message"
        "message(WARNING \"\${_lumex_hid_message}\")"
        "message(STATUS \"\${_lumex_hid_message}\")")
    string(FIND "${_options}" "${_needle}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR "LumexOptions.cmake does not contain: ${_needle}")
    endif()
endforeach()
