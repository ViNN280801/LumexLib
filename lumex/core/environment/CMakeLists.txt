# lumex/core/environment/CMakeLists.txt

set(LUMEX_ENVIRONMENT_NAME LumexCore_environment)
set(EXPORT_TO LumexCoreTargets)

add_library(${LUMEX_ENVIRONMENT_NAME} LumexEnvironment.cpp)
add_library(Lumex::environment ALIAS ${LUMEX_ENVIRONMENT_NAME})

if(BUILD_SHARED_LIBS AND WIN32)
    target_compile_definitions(${LUMEX_ENVIRONMENT_NAME} PRIVATE LUMEX_EXPORTS)
endif()

target_include_directories(${LUMEX_ENVIRONMENT_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(${LUMEX_ENVIRONMENT_NAME} PUBLIC
    Lumex::utility
)

lumex_apply_library_versioning(${LUMEX_ENVIRONMENT_NAME} "Lumex Environment Library")

install(TARGETS ${LUMEX_ENVIRONMENT_NAME} EXPORT ${EXPORT_TO}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION include/lumex/core/environment
    FILES_MATCHING 
        PATTERN "*.hpp"
        PATTERN "LumexEnvironment"
)
