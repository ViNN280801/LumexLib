# lumex/core/string_view/CMakeLists.txt

set(LUMEX_STRING_VIEW_NAME LumexCore_string_view)
set(EXPORT_TO LumexCoreTargets)

add_library(${LUMEX_STRING_VIEW_NAME}
    LumexStringView.cpp
    LumexWStringView.cpp
)
add_library(Lumex::string_view ALIAS ${LUMEX_STRING_VIEW_NAME})

if(WIN32 AND BUILD_SHARED_LIBS)
    target_compile_definitions(${LUMEX_STRING_VIEW_NAME} PRIVATE LUMEX_EXPORTS)
endif()

target_include_directories(${LUMEX_STRING_VIEW_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(${LUMEX_STRING_VIEW_NAME} PUBLIC
    Lumex::utility
)

install(TARGETS ${LUMEX_STRING_VIEW_NAME} EXPORT ${EXPORT_TO}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION include/lumex/core/string_view
    FILES_MATCHING PATTERN "*.hpp"
)
