# lumex/core/utility/CMakeLists.txt

set(LUMEX_UTILITY_NAME LumexCore_utility)
set(EXPORT_TO LumexCoreTargets)

add_library(${LUMEX_UTILITY_NAME} LumexAssert.cpp)
add_library(Lumex::utility ALIAS ${LUMEX_UTILITY_NAME})

if(WIN32 AND BUILD_SHARED_LIBS)
    target_compile_definitions(${LUMEX_UTILITY_NAME} PRIVATE LUMEX_EXPORTS)
endif()

target_include_directories(${LUMEX_UTILITY_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

lumex_apply_library_versioning(${LUMEX_UTILITY_NAME} "Lumex Utility Library")

install(TARGETS ${LUMEX_UTILITY_NAME} EXPORT ${EXPORT_TO}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION include/lumex/core/utility
    FILES_MATCHING
    PATTERN "*.hpp"
    PATTERN "LumexUtility"
)
