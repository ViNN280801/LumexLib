# Every open the enumeration performs must go through the bounded open:
# a paired-but-disconnected Bluetooth COM port blocks inside CreateFile in
# the RFCOMM connect, and the enumeration used to call CreateFileA / open
# directly in both the per-port classification and the COM1..COM255
# fallback. This case fails when an open reappears in the enumeration
# source, or when the bounded open loses the cancellation calls the
# "never wait past the deadline" promise rests on.

file(READ
    "${LUMEX_SOURCE_DIR}/lumex/applied/serial/enumeration/LumexSerialPortEnumeration.cpp"
    _enumeration)
file(READ
    "${LUMEX_SOURCE_DIR}/lumex/applied/serial/probe/detail/LumexSerialBoundedOpen.cpp"
    _bounded_open)

foreach(_forbidden "CreateFileA" "CreateFileW" "::open (")
    string(FIND "${_enumeration}" "${_forbidden}" _pos)
    if(_pos GREATER -1)
        message(FATAL_ERROR
            "LumexSerialPortEnumeration.cpp opens a port directly "
            "(\"${_forbidden}\" at offset ${_pos}); every open must go "
            "through bounded_open_serial_port")
    endif()
endforeach()

string(FIND "${_bounded_open}" "CancelSynchronousIo" _cancel_pos)
if(_cancel_pos EQUAL -1)
    message(FATAL_ERROR
        "LumexSerialBoundedOpen.cpp does not call CancelSynchronousIo; "
        "the Windows bounded open cannot cancel a stuck CreateFile "
        "without it")
endif()

file(READ
    "${LUMEX_SOURCE_DIR}/lumex/applied/serial/probe/LumexSerialProber.cpp"
    _prober)

string(FIND "${_prober}" "CancelIoEx" _io_cancel_pos)
if(_io_cancel_pos EQUAL -1)
    message(FATAL_ERROR
        "LumexSerialProber.cpp does not call CancelIoEx; the Windows "
        "transport cannot stop an overlapped read or write at the "
        "deadline without it")
endif()
