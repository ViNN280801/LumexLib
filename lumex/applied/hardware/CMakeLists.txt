# lumex/applied/hardware/CMakeLists.txt

set(LUMEX_HARDWARE_NAME LumexApplied_hardware)
set(EXPORT_TO LumexAppliedTargets)

file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
add_library(${LUMEX_HARDWARE_NAME} ${SOURCES})
add_library(lumex::hardware ALIAS ${LUMEX_HARDWARE_NAME})

if(BUILD_SHARED_LIBS AND WIN32)
    target_compile_definitions(${LUMEX_HARDWARE_NAME} PRIVATE LUMEX_EXPORTS)
endif()

target_include_directories(${LUMEX_HARDWARE_NAME} PUBLIC
    $<BUILD_INTERFACE:${LumexLib_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(${LUMEX_HARDWARE_NAME} PUBLIC
    lumex::logging
    lumex::utility
)

# The Windows source converts the UTF-16 adapter name with
# lumex::core::unicode::convert (header-only; used by the source only).
target_link_libraries(${LUMEX_HARDWARE_NAME} PRIVATE lumex::unicode)

# The sources link these with #pragma comment(lib), which MSVC and clang-cl
# honour and GCC ignores; MinGW names them here.
if(MINGW)
    target_link_libraries(${LUMEX_HARDWARE_NAME} PRIVATE iphlpapi d3d11 dxgi)
endif()

lumex_apply_library_versioning(${LUMEX_HARDWARE_NAME} "Lumex Hardware Capabilities Library")

if(LUMEX_INSTALL)
    install(TARGETS ${LUMEX_HARDWARE_NAME} EXPORT ${EXPORT_TO}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

    install(
        DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
        DESTINATION include/lumex/applied/hardware
        FILES_MATCHING 
            PATTERN "*.hpp"
            PATTERN "LumexHardware"
    )
endif()
