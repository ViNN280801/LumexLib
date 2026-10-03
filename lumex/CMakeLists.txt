# lumex/CMakeLists.txt
#
# core before applied: applied modules link lumex::filesystem / time /
# utility / environment / string (ALIAS targets must already exist).

# The core/utility headers (relative to this directory) that the header-only
# modules string, math, generators, optional and atomic include, directly or
# through each other. Those modules have no configure-time dependency on
# utility (utility itself requires math, so the edge would be a cycle). Every
# header here is header-only: LumexAssert.hpp declares the compiled assert
# handler, but the modules only use LUMEX_STATIC_ASSERT_MSG. Keep the list
# equal to the include closure of the five modules' headers.
set(LUMEX_UTILITY_HEADERS_FOR_HEADER_ONLY_MODULES
    core/utility/assert/LumexAssert.hpp
    core/utility/attr/LumexAttributes.hpp
    core/utility/compiler/LumexCheckFeatures.hpp
    core/utility/macros/LumexConstantMacros.hpp
    core/utility/macros/LumexExceptionMacros.hpp
    core/utility/macros/LumexKeywords.hpp
    core/utility/traits/LumexTypeTraits.hpp
)

lumex_add_subdirectory_if(LUMEX_BUILD_CORE core)
# xml after core (needs utility), before applied: WITH_XML makes logger
# PRIVATE-link lumex::xml at configure time.
lumex_add_subdirectory_if(LUMEX_BUILD_XML xml)
lumex_add_subdirectory_if(LUMEX_BUILD_APPLIED applied)

if(LUMEX_INSTALL)
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/LumexExport.hpp" DESTINATION include/lumex)

    # Installed whatever LUMEX_BUILD_UTILITY says, so the header-only modules
    # compile from an install prefix without the utility module. When utility
    # is built, its own rule installs its whole header tree, these files
    # included, and a second rule would only list them twice in the manifest.
    lumex_effective_on(_lumex_utility_on LUMEX_BUILD_UTILITY)
    if(NOT _lumex_utility_on)
        foreach(_lumex_header IN LISTS LUMEX_UTILITY_HEADERS_FOR_HEADER_ONLY_MODULES)
            get_filename_component(_lumex_header_dir "${_lumex_header}" DIRECTORY)
            install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/${_lumex_header}"
                DESTINATION "include/lumex/${_lumex_header_dir}")
        endforeach()
        unset(_lumex_header)
        unset(_lumex_header_dir)
    endif()
    unset(_lumex_utility_on)
endif()
