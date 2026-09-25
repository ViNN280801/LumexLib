# lumex/core/fmt/CMakeLists.txt

set(LUMEX_FMT_NAME LumexCore_fmt)
set(EXPORT_TO LumexCoreTargets)

add_library(${LUMEX_FMT_NAME} INTERFACE)
add_library(lumex::fmt ALIAS ${LUMEX_FMT_NAME})

target_link_libraries(${LUMEX_FMT_NAME} INTERFACE
    lumex::utility
)

target_include_directories(${LUMEX_FMT_NAME} INTERFACE
    $<BUILD_INTERFACE:${LumexLib_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

if(LUMEX_INSTALL)
    install(TARGETS ${LUMEX_FMT_NAME} EXPORT ${EXPORT_TO})

    install(
        DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
        DESTINATION include/lumex/core/fmt
        FILES_MATCHING 
            PATTERN "*.hpp"
            PATTERN "LumexFormat"
    )
endif()
