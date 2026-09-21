# lumex/CMakeLists.txt
#
# core before applied: applied modules link lumex::filesystem / time /
# utility / environment / string (ALIAS targets must already exist).

lumex_add_subdirectory_if(LUMEX_BUILD_CORE core)
# xml after core (needs utility), before applied: WITH_XML makes logger
# PRIVATE-link lumex::xml at configure time.
lumex_add_subdirectory_if(LUMEX_BUILD_XML xml)
lumex_add_subdirectory_if(LUMEX_BUILD_APPLIED applied)

install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/LumexExport.hpp" DESTINATION include/lumex)
