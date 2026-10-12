# lumex/core/span/CMakeLists.txt

set(LUMEX_SPAN_NAME LumexCore_span)
set(EXPORT_TO LumexCoreTargets)

# Header-only, no dependency edge. The precondition checks (LUMEX_SPAN_CHECKS,
# on by default without NDEBUG) call an inline failure handler of
# view/LumexSpanChecks.hpp, so a program that links only this target builds
# and aborts on a violation.
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
