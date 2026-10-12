# include(GNUInstallDirs) comes before add_subdirectory(lumex) in the root
# CMakeLists.txt. The install rules of the modules (RUNTIME DESTINATION
# ${CMAKE_INSTALL_BINDIR}) read the variable when their directory is processed.
# Behind add_subdirectory, a configure that gave only -DCMAKE_INSTALL_LIBDIR
# (create_release.sh does) left BINDIR empty and CMake dropped the RUNTIME
# rule, so the install of a Windows or MinGW build had no DLL.

file(READ "${LUMEX_SOURCE_DIR}/CMakeLists.txt" _root)
# Whole lines: a comment of the file also names add_subdirectory(lumex).
string(FIND "${_root}" "\ninclude(GNUInstallDirs)\n" _dirs)
string(FIND "${_root}" "\nadd_subdirectory(lumex)\n" _modules)
if(_dirs EQUAL -1 OR _modules EQUAL -1)
    message(FATAL_ERROR "CMakeLists.txt lost include(GNUInstallDirs) or add_subdirectory(lumex)")
endif()
if(_dirs GREATER _modules)
    message(FATAL_ERROR
        "include(GNUInstallDirs) must come before add_subdirectory(lumex)")
endif()
