# lumex/tests/core/protocol/devices/CMakeLists.txt

# The test tables of the researched devices, one directory per device
# (device_a ... device_m), each with its own CMakeLists.txt and suites
# (lumex_add_standard_suites (ProtocolDevice<Letter> ...)). The protocols in
# these tables were intentionally altered for NDA reasons; they do not
# describe any real device. Every directory with a CMakeLists.txt is added,
# in name order, so a new device needs no edit here.
file(GLOB _lumex_protocol_devices LIST_DIRECTORIES true
     RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/*")
list(SORT _lumex_protocol_devices)
foreach(_lumex_device IN LISTS _lumex_protocol_devices)
    if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${_lumex_device}"
       AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_lumex_device}/CMakeLists.txt")
        add_subdirectory(${_lumex_device})
    endif()
endforeach()
