# lumex/core/hazard_pointer/CMakeLists.txt

set(LUMEX_HAZARD_POINTER_NAME LumexCore_hazard_pointer)
set(EXPORT_TO LumexCoreTargets)

file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
add_library(${LUMEX_HAZARD_POINTER_NAME} ${SOURCES})
add_library(lumex::hazard_pointer ALIAS ${LUMEX_HAZARD_POINTER_NAME})

if(BUILD_SHARED_LIBS AND WIN32)
    target_compile_definitions(${LUMEX_HAZARD_POINTER_NAME} PRIVATE LUMEX_EXPORTS)
endif()

target_include_directories(${LUMEX_HAZARD_POINTER_NAME} PUBLIC
    $<BUILD_INTERFACE:${LumexLib_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# The reader side is header-only; the engine (slot pool, thread cache, retired
# lists) is compiled here. The batch functions take the span of the library;
# pthread keys (the thread-exit hook of the slot cache) need libpthread before
# glibc 2.34, which Threads::Threads provides.
find_package(Threads REQUIRED)

target_link_libraries(${LUMEX_HAZARD_POINTER_NAME} PUBLIC
    lumex::utility
    lumex::span
    Threads::Threads
)

# The library is compiled as C++11 on purpose, like core/crc: its exported
# functions do not depend on the standard, and nothing in it needs more than
# C++11, so it is the floor the module is checked at.
set_target_properties(${LUMEX_HAZARD_POINTER_NAME} PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# The slot cache of a thread is handed back by a pthread key destructor that
# lives in this library; unloading the library while threads that used it are
# alive would leave the destructor pointing at unmapped code. An ELF library
# that is linked with -z nodelete stays mapped until the process ends (the
# Windows build pins the module when the first cache is created).
if(BUILD_SHARED_LIBS AND UNIX AND NOT APPLE)
    target_link_options(${LUMEX_HAZARD_POINTER_NAME} PRIVATE LINKER:-z,nodelete)
endif()

lumex_apply_library_versioning(${LUMEX_HAZARD_POINTER_NAME} "Lumex Hazard Pointer Library")

if(LUMEX_INSTALL)
    install(TARGETS ${LUMEX_HAZARD_POINTER_NAME} EXPORT ${EXPORT_TO}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

    install(
        DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
        DESTINATION include/lumex/core/hazard_pointer
        FILES_MATCHING
        PATTERN "*.hpp"
        PATTERN "LumexHazardPointer"
    )
endif()
