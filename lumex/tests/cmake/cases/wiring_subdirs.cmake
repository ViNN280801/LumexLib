# Subdirectory CMakeLists and export / test-suite lists must name every
# module option. A new module without these strings is a wiring hole.

include("${LUMEX_SOURCE_DIR}/cmake/LumexModules.cmake")

function(_require_text path needle)
    file(READ "${LUMEX_SOURCE_DIR}/${path}" _txt)
    string(FIND "${_txt}" "${needle}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR "${path} does not mention ${needle}")
    endif()
endfunction()

_require_text("cmake/LumexOptions.cmake" "option(LUMEX_BUILD_CORE ")
_require_text("cmake/LumexOptions.cmake" "option(LUMEX_BUILD_APPLIED ")
_require_text("cmake/LumexOptions.cmake" "option(LUMEX_BUILD_XML ")

_require_text("lumex/CMakeLists.txt" "lumex_add_subdirectory_if(LUMEX_BUILD_CORE core)")
_require_text("lumex/CMakeLists.txt" "lumex_add_subdirectory_if(LUMEX_BUILD_XML xml)")
_require_text("lumex/CMakeLists.txt" "lumex_add_subdirectory_if(LUMEX_BUILD_APPLIED applied)")
_require_text("lumex/tests/CMakeLists.txt" "add_subdirectory(cmake)")
_require_text("lumex/tests/CMakeLists.txt" "lumex_add_subdirectory_if(LUMEX_BUILD_CORE core)")
_require_text("lumex/tests/CMakeLists.txt" "lumex_add_subdirectory_if(LUMEX_BUILD_APPLIED applied)")
_require_text("lumex/tests/CMakeLists.txt" "lumex_add_subdirectory_if(LUMEX_BUILD_XML xml)")
_require_text("cmake/LumexOptions.cmake" "set(LUMEX_LOGGER_CONFIG_FORMAT ")
_require_text("lumex/applied/logger/CMakeLists.txt" "LUMEX_LOGGER_CONFIG_FORMAT_NORMALIZED")

foreach(_opt ${LUMEX_CORE_MODULE_OPTIONS})
    _require_text("cmake/LumexOptions.cmake" "option(${_opt} ")
    _require_text("lumex/core/CMakeLists.txt" "lumex_add_subdirectory_if(${_opt} ")
    _require_text("lumex/tests/core/CMakeLists.txt" "lumex_add_subdirectory_if(${_opt} ")
endforeach()

foreach(_opt ${LUMEX_APPLIED_MODULE_OPTIONS})
    _require_text("cmake/LumexOptions.cmake" "option(${_opt} ")
    _require_text("lumex/applied/CMakeLists.txt" "lumex_add_subdirectory_if(${_opt} ")
    _require_text("lumex/tests/applied/CMakeLists.txt" "lumex_add_subdirectory_if(${_opt} ")
endforeach()

file(READ "${LUMEX_SOURCE_DIR}/CMakeLists.txt" _root)
foreach(_tgt
        LumexCore_base64 LumexCore_circular_buffer LumexCore_crc
        LumexCore_environment LumexCore_exceptions LumexCore_expected
        LumexCore_filesystem LumexCore_number_generator LumexCore_math
        LumexCore_optional LumexCore_reflection LumexCore_string
        LumexCore_string_view LumexCore_temporary LumexCore_time
        LumexCore_utility
        LumexApplied_hardware LumexApplied_json LumexApplied_logger
        LumexApplied_logging
        LumexApplied_resource_monitor LumexApplied_serial LumexApplied_settings
        LumexXml)
    string(FIND "${_root}" "${_tgt}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR
            "CMakeLists.txt export candidates omit ${_tgt}")
    endif()
endforeach()

file(GLOB_RECURSE _test_cmakes "${LUMEX_SOURCE_DIR}/lumex/tests/*CMakeLists.txt")
set(_tests_txt "")
foreach(_f ${_test_cmakes})
    file(READ "${_f}" _chunk)
    string(APPEND _tests_txt "${_chunk}")
endforeach()
foreach(_suite
        LumexHardwareCapabilitiesTests LumexJsonTests LumexJsonCxx20Tests
        LumexCallbackSlotTests LumexLoggerTests LumexLoggingTests
        LumexResourceMonitorTests LumexSerialPortTests LumexSettingsTests
        LumexBase64Tests LumexCircularBufferTests LumexCrcTests
        LumexEnvironmentTests LumexExceptionsTests LumexExpectedTests
        LumexFilesystemTests LumexMathTests LumexNumberGeneratorTests
        LumexOptionalTests LumexReflectionTests LumexReflectionCxx20Tests LumexFieldReflectionTests
        LumexFieldReflectionGetTests LumexFieldReflectionNamesTests
        LumexStringifyTests LumexStringifyCxx20Tests
        LumexStringViewTests LumexTemporaryTests LumexTimeTests
        LumexTypeTraitsTests LumexSafeNumericComparatorCxx11Tests
        LumexUtilityTests LumexXmlTests)
    string(FIND "${_tests_txt}" "${_suite}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR
            "lumex/tests CMakeLists omit ${_suite}")
    endif()
endforeach()
