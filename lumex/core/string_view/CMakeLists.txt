# lumex/core/string_view/CMakeLists.txt

# Header-only: every view is a class template (lumex_basic_string_view) and
# its members are inline, so there is no library to build or to link. The
# interface target carries the include directory.
set(LUMEX_STRING_VIEW_NAME LumexCore_string_view)
set(EXPORT_TO LumexCoreTargets)

add_library(${LUMEX_STRING_VIEW_NAME} INTERFACE)
add_library(lumex::string_view ALIAS ${LUMEX_STRING_VIEW_NAME})

target_include_directories(${LUMEX_STRING_VIEW_NAME} INTERFACE
    $<BUILD_INTERFACE:${LumexLib_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

if(LUMEX_INSTALL)
    install(TARGETS ${LUMEX_STRING_VIEW_NAME} EXPORT ${EXPORT_TO})

    install(
        DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
        DESTINATION include/lumex/core/string_view
        FILES_MATCHING 
            PATTERN "*.hpp"
            PATTERN "LumexStringView"
    )
endif()
