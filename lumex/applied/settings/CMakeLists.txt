# lumex/applied/settings/CMakeLists.txt

set(LUMEX_SETTINGS_NAME LumexApplied_settings)
set(EXPORT_TO LumexAppliedTargets)

add_library(${LUMEX_SETTINGS_NAME}
    LumexSettingsFactory.cpp
    LumexSettingsINI.cpp
)
add_library(Lumex::settings ALIAS ${LUMEX_SETTINGS_NAME})

if(BUILD_SHARED_LIBS AND WIN32)
    target_compile_definitions(${LUMEX_SETTINGS_NAME} PRIVATE LUMEX_EXPORTS)
endif()

target_include_directories(${LUMEX_SETTINGS_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(${LUMEX_SETTINGS_NAME} PUBLIC
    Lumex::filesystem
)

lumex_apply_library_versioning(${LUMEX_SETTINGS_NAME} "Lumex Settings Library")

install(TARGETS ${LUMEX_SETTINGS_NAME} EXPORT ${EXPORT_TO}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION include/lumex/applied/settings
    FILES_MATCHING 
        PATTERN "*.hpp"
        PATTERN "LumexSettings"
)
