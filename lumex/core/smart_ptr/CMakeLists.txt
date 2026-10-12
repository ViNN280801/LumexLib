# lumex/core/smart_ptr/CMakeLists.txt
#
# Header-only. The headers include the macro and assertion headers of
# core/utility (LUMEX_SMART_PTR_DEBUG_ASSERT calls lumex_assert_handler, which lives in
# the compiled utility library), so the target links lumex::utility.

set(LUMEX_SMART_PTR_NAME LumexCore_smart_ptr)
set(EXPORT_TO LumexCoreTargets)

add_library(${LUMEX_SMART_PTR_NAME} INTERFACE)
add_library(lumex::smart_ptr ALIAS ${LUMEX_SMART_PTR_NAME})

target_include_directories(${LUMEX_SMART_PTR_NAME} INTERFACE
    $<BUILD_INTERFACE:${LumexLib_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(${LUMEX_SMART_PTR_NAME} INTERFACE
    lumex::utility
)

if(LUMEX_INSTALL)
    install(TARGETS ${LUMEX_SMART_PTR_NAME} EXPORT ${EXPORT_TO})

    install(
        DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
        DESTINATION include/lumex/core/smart_ptr
        FILES_MATCHING
            PATTERN "*.hpp"
            PATTERN "LumexSmartPtr"
    )
endif()
