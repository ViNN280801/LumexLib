# cmake/LumexNlohmannJson.cmake
#
# Vendored nlohmann/json 3.12.0 single-header under 3rdparty/nlohmann/.
# Called when LUMEX_WITH_FIELD_REFLECTION is ON, when logger JSON config
# is compiled, and when settings finds the header (LumexSettingsJSON).
# Creates an in-tree INTERFACE target that is NOT exported (exporting it
# would require a find_dependency in the package config). Include dirs
# are also copied onto lumex::reflection so the installed package export
# stays self-contained.

function(lumex_setup_nlohmann_json)
    set(_nlohmann_root "${CMAKE_SOURCE_DIR}/3rdparty")
    set(_nlohmann_hdr "${_nlohmann_root}/nlohmann/json.hpp")
    if(NOT EXISTS "${_nlohmann_hdr}")
        message(FATAL_ERROR
            "LUMEX_WITH_FIELD_REFLECTION=ON requires ${_nlohmann_hdr} "
            "(nlohmann/json 3.12.0 single-include).")
    endif()

    if(NOT TARGET nlohmann_json)
        add_library(nlohmann_json INTERFACE)
        add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json)
        target_include_directories(nlohmann_json INTERFACE
            $<BUILD_INTERFACE:${_nlohmann_root}>
        )
    endif()
endfunction()
