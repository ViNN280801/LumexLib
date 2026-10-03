# lumex/core/generators/CMakeLists.txt

add_subdirectory(number_generator)

if(LUMEX_INSTALL)
    # Only the umbrella: number_generator installs its own headers.
    install(
        FILES ${CMAKE_CURRENT_SOURCE_DIR}/LumexGenerators
        DESTINATION include/lumex/core/generators
    )
endif()
