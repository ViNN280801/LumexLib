# lumex/core/base64/CMakeLists.txt

set(LUMEX_BASE64_NAME LumexCore_base64)
set(EXPORT_TO LumexCoreTargets)

add_library(${LUMEX_BASE64_NAME}
    Decoder.cpp
    Encoder.cpp
    Validator.cpp
)
add_library(Lumex::base64 ALIAS ${LUMEX_BASE64_NAME})

if(BUILD_SHARED_LIBS AND WIN32)
    target_compile_definitions(${LUMEX_BASE64_NAME} PRIVATE LUMEX_EXPORTS)
endif()

target_include_directories(${LUMEX_BASE64_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(${LUMEX_BASE64_NAME} PUBLIC
    Lumex::utility
)

install(TARGETS ${LUMEX_BASE64_NAME} EXPORT ${EXPORT_TO}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION include/lumex/core/base64
    FILES_MATCHING PATTERN "*.hpp"
)
