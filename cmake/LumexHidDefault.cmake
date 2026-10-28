# cmake/LumexHidDefault.cmake
#
# lumex_hid_default_decision(<default var> <message var> <backend> <system has
# its own backend> <pkg-config found> <library found>): the pure decision, so
# that a test can walk every combination without the libraries.
function(lumex_hid_default_decision default_var message_var backend own_backend
         pkg_config_found library_found)
    set(_default OFF)
    set(_message "")
    if(own_backend)
        set(_default ON)
    else()
        if(backend STREQUAL "libusb")
            set(_library "libusb-1.0")
            set(_package "libusb-1.0-0-dev on Debian and Astra Linux")
        else()
            set(_library "libudev")
            set(_package "libudev-dev on Debian and Astra Linux")
        endif()
        if(NOT pkg_config_found)
            set(_message
                "LumexLib: pkg-config was not found, so ${_library} (needed "
                "by the ${backend} backend of hidapi) cannot be looked for: "
                "the HID transport (hid_device) is OFF. Install pkg-config "
                "and ${_library} (${_package}), or set "
                "-DLUMEX_TRANSPORT_WITH_HID=ON to build it anyway.")
        elseif(NOT library_found)
            set(_message
                "LumexLib: the ${backend} backend of hidapi needs "
                "${_library} (${_package}), which pkg-config does not find: "
                "the HID transport (hid_device) is OFF. Install it, choose "
                "another -DLUMEX_HIDAPI_BACKEND, or set "
                "-DLUMEX_TRANSPORT_WITH_HID=ON to get the build error.")
        else()
            set(_default ON)
        endif()
    endif()
    string(CONCAT _message ${_message})
    set(${default_var} "${_default}" PARENT_SCOPE)
    set(${message_var} "${_message}" PARENT_SCOPE)
endfunction()

