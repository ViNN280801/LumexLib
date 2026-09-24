# lumex/core/generators/CMakeLists.txt

add_subdirectory(number_generator)

if(LUMEX_INSTALL)
    install(
        DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
        DESTINATION include/lumex/core/generators
        FILES_MATCHING 
            PATTERN "*.hpp"
            PATTERN "LumexGenerators"
    )
endif()
