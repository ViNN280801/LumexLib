# lumex/core/span/CMakeLists.txt

set(LUMEX_SPAN_NAME LumexCore_span)
set(EXPORT_TO LumexCoreTargets)

add_library(${LUMEX_SPAN_NAME} INTERFACE)
add_library(lumex::span ALIAS ${LUMEX_SPAN_NAME})

target_include_directories(${LUMEX_SPAN_NAME} INTERFACE
    $<BUILD_INTERFACE:${LumexLib_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

if(LUMEX_INSTALL)
    install(TARGETS ${LUMEX_SPAN_NAME} EXPORT ${EXPORT_TO})

    install(
        DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
        DESTINATION include/lumex/core/span
        FILES_MATCHING 
            PATTERN "*.hpp"
            PATTERN "LumexSpan"
    )
endif()
