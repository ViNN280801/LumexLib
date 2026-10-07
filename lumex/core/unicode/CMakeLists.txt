# lumex/core/unicode/CMakeLists.txt

set(LUMEX_UNICODE_NAME LumexCore_unicode)
set(EXPORT_TO LumexCoreTargets)

add_library(${LUMEX_UNICODE_NAME} INTERFACE)
add_library(lumex::unicode ALIAS ${LUMEX_UNICODE_NAME})

target_include_directories(${LUMEX_UNICODE_NAME} INTERFACE
    $<BUILD_INTERFACE:${LumexLib_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# LUMEX_ASSERT expands to lumex_assert_handler (LumexCore_utility), and
# utf/LumexUtf.hpp includes the byte swap of core/utility/bit.
target_link_libraries(${LUMEX_UNICODE_NAME} INTERFACE
    lumex::utility
)

if(LUMEX_INSTALL)
    install(TARGETS ${LUMEX_UNICODE_NAME} EXPORT ${EXPORT_TO})

    install(
        DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
        DESTINATION include/lumex/core/unicode
        FILES_MATCHING
        PATTERN "*.hpp"
        PATTERN "LumexUnicode"
    )
endif()
