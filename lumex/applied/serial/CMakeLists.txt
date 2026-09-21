# lumex/applied/serial/CMakeLists.txt

set(LUMEX_SERIAL_NAME LumexApplied_serial)
set(EXPORT_TO LumexAppliedTargets)

file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
add_library(${LUMEX_SERIAL_NAME} ${SOURCES})
add_library(lumex::serial ALIAS ${LUMEX_SERIAL_NAME})

if(BUILD_SHARED_LIBS AND WIN32)
    target_compile_definitions(${LUMEX_SERIAL_NAME} PRIVATE LUMEX_EXPORTS)
endif()

target_include_directories(${LUMEX_SERIAL_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(${LUMEX_SERIAL_NAME} PUBLIC
    lumex::utility
)

if(WIN32)
    target_link_libraries(${LUMEX_SERIAL_NAME} PRIVATE setupapi advapi32)
endif()

lumex_apply_library_versioning(${LUMEX_SERIAL_NAME} "Lumex Serial Port Library")

install(TARGETS ${LUMEX_SERIAL_NAME} EXPORT ${EXPORT_TO}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION include/lumex/applied/serial
    FILES_MATCHING
        PATTERN "*.hpp"
        PATTERN "LumexSerialPort"
)
