# lumex/core/time/CMakeLists.txt

set(LUMEX_TIME_NAME LumexCore_time)
set(EXPORT_TO LumexCoreTargets)

add_library(${LUMEX_TIME_NAME} LumexTime.cpp)
add_library(Lumex::time ALIAS ${LUMEX_TIME_NAME})

if(WIN32 AND BUILD_SHARED_LIBS)
    target_compile_definitions(${LUMEX_TIME_NAME} PRIVATE LUMEX_EXPORTS)
endif()

target_include_directories(${LUMEX_TIME_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

lumex_apply_library_versioning(${LUMEX_TIME_NAME} "Lumex Time Library")

install(TARGETS ${LUMEX_TIME_NAME} EXPORT ${EXPORT_TO}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION include/lumex/core/time
    FILES_MATCHING 
        PATTERN "*.hpp"
        PATTERN "LumexTime"
)
