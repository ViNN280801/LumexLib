# cmake/LumexHidapi.cmake
#
# hidapi 0.15.0, vendored under 3rdparty/hidapi (provenance:
# 3rdparty/hidapi/VENDORED.md). lumex_setup_hidapi() creates the in-tree STATIC
# C target lumex_3rdparty_hidapi (alias lumex::3rdparty_hidapi, not exported):
# Windows backend on Windows, IOKit on Apple systems (portable, not verified),
# and on other systems the backend of LUMEX_HIDAPI_BACKEND: hidraw (default,
# needs libudev) or libusb (needs libusb-1.0). LUMEX_HIDAPI_DIR replaces the
# vendored tree with another hidapi 0.15.0 source tree. LumexLib never calls
# hid_exit (DChannel's hidapi copy may share the process).
#
# lumex_setup_hidapi is a macro: hidapi is C, the root project enables CXX
# only, and enable_language() must run in file scope, which a function body is
# not. Call it from file scope of a CMakeLists.txt.

get_filename_component(_LUMEX_HIDAPI_DEFAULT_DIR
    "${CMAKE_CURRENT_LIST_DIR}/../3rdparty/hidapi" ABSOLUTE)
# A global property, not a variable: the setup function may be called from a
# consumer's directory, which does not see this file's directory scope.
set_property(GLOBAL PROPERTY LUMEX_HIDAPI_DEFAULT_DIR "${_LUMEX_HIDAPI_DEFAULT_DIR}")

macro(lumex_setup_hidapi)
    if(NOT TARGET lumex_3rdparty_hidapi)
        if(NOT CMAKE_C_COMPILER_LOADED)
            enable_language(C)
        endif()
        _lumex_setup_hidapi_target()
    endif()
endmacro()

function(_lumex_setup_hidapi_target)
    if(LUMEX_HIDAPI_DIR)
        get_filename_component(_dir "${LUMEX_HIDAPI_DIR}" ABSOLUTE)
    else()
        get_property(_dir GLOBAL PROPERTY LUMEX_HIDAPI_DEFAULT_DIR)
    endif()

    set(_header "${_dir}/hidapi/hidapi.h")
    lumex_third_party_read_define(_major "${_header}" HID_API_VERSION_MAJOR)
    lumex_third_party_read_define(_minor "${_header}" HID_API_VERSION_MINOR)
    lumex_third_party_read_define(_patch "${_header}" HID_API_VERSION_PATCH)
    if(NOT "${_major}.${_minor}.${_patch}" STREQUAL "0.15.0")
        message(FATAL_ERROR
            "LumexLib needs hidapi 0.15.0, ${_header} has "
            "${_major}.${_minor}.${_patch}. The sources DChannel compiles are "
            "0.15.0, and one process can hold both copies.")
    endif()

    set(_public_includes "${_dir}/hidapi")
    set(_link "")
    if(WIN32)
        set(_sources
            "${_dir}/windows/hid.c"
            "${_dir}/windows/hidapi_descriptor_reconstruct.c")
        list(APPEND _public_includes "${_dir}/windows")
        # HidD_* and CM_Get_* are resolved at link time by the Windows SDK
        # import libraries (DChannel v4 found them missing from upstream's CMake).
        set(_link setupapi hid cfgmgr32)
    elseif(APPLE)
        set(_sources "${_dir}/mac/hid.c")
        list(APPEND _public_includes "${_dir}/mac")
        set(_link "-framework IOKit" "-framework CoreFoundation" "-framework AppKit")
    elseif(LUMEX_HIDAPI_BACKEND STREQUAL "libusb")
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(LUMEX_LIBUSB REQUIRED IMPORTED_TARGET libusb-1.0)
        find_package(Threads REQUIRED)
        set(_sources "${_dir}/libusb/hid.c")
        list(APPEND _public_includes "${_dir}/libusb")
        set(_link PkgConfig::LUMEX_LIBUSB Threads::Threads)
    elseif(LUMEX_HIDAPI_BACKEND STREQUAL "hidraw")
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(LUMEX_LIBUDEV IMPORTED_TARGET libudev)
        if(NOT LUMEX_LIBUDEV_FOUND)
            message(FATAL_ERROR
                "LumexLib: the hidraw backend of hidapi needs libudev "
                "(package libudev-dev on Debian and Astra Linux, "
                "systemd-devel on Fedora), or set LUMEX_HIDAPI_BACKEND=libusb.")
        endif()
        set(_sources "${_dir}/linux/hid.c")
        set(_link PkgConfig::LUMEX_LIBUDEV)
    else()
        message(FATAL_ERROR
            "LumexLib: LUMEX_HIDAPI_BACKEND must be hidraw or libusb, "
            "not '${LUMEX_HIDAPI_BACKEND}'")
    endif()

    add_library(lumex_3rdparty_hidapi STATIC ${_sources})
    add_library(lumex::3rdparty_hidapi ALIAS lumex_3rdparty_hidapi)
    foreach(_inc IN LISTS _public_includes)
        target_include_directories(lumex_3rdparty_hidapi SYSTEM PUBLIC
            "$<BUILD_INTERFACE:${_inc}>")
    endforeach()
    target_link_libraries(lumex_3rdparty_hidapi PUBLIC ${_link})
    lumex_third_party_quiet_includes(lumex_3rdparty_hidapi)
    lumex_third_party_finish(lumex_3rdparty_hidapi)
endfunction()
