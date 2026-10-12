# lumex/core/contracts/CMakeLists.txt

set(LUMEX_CONTRACTS_NAME LumexCore_contracts)
set(EXPORT_TO LumexCoreTargets)

file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
add_library(${LUMEX_CONTRACTS_NAME} ${SOURCES})
add_library(lumex::contracts ALIAS ${LUMEX_CONTRACTS_NAME})

if(BUILD_SHARED_LIBS AND WIN32)
    target_compile_definitions(${LUMEX_CONTRACTS_NAME} PRIVATE LUMEX_EXPORTS)
endif()

target_include_directories(${LUMEX_CONTRACTS_NAME} PUBLIC
    $<BUILD_INTERFACE:${LumexLib_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# The macros are header-only; the handler slot and the default handler are
# compiled here, so that an executable and every shared library it loads see
# one violation handler. utility provides the callback slot and the keyword
# and attribute macros.
target_link_libraries(${LUMEX_CONTRACTS_NAME} PUBLIC
    lumex::utility
)

# The evaluation semantic of LUMEX_CONTRACT_ASSERT for the build (the cache
# variable LUMEX_CONTRACTS_SEMANTIC, normalized to a lower-case word by
# lumex_check_module_dependencies). Every translation unit that links this
# target gets it, installed packages included; a translation unit may still
# define LUMEX_CONTRACTS_SEMANTIC itself before the first include.
target_compile_definitions(${LUMEX_CONTRACTS_NAME} PUBLIC
    LUMEX_CONTRACTS_BUILD_SEMANTIC=${LUMEX_CONTRACTS_SEMANTIC_NORMALIZED}
)

# The library is compiled as C++11 on purpose, like core/crc and
# core/hazard_pointer: its exported functions do not depend on the standard,
# and nothing in it needs more than C++11, so it is the floor the module is
# checked at.
set_target_properties(${LUMEX_CONTRACTS_NAME} PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

lumex_apply_library_versioning(${LUMEX_CONTRACTS_NAME} "Lumex Contracts Library")

if(LUMEX_INSTALL)
    install(TARGETS ${LUMEX_CONTRACTS_NAME} EXPORT ${EXPORT_TO}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

    install(
        DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
        DESTINATION include/lumex/core/contracts
        FILES_MATCHING
        PATTERN "*.hpp"
        PATTERN "LumexContracts"
    )
endif()
