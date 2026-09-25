# The root project must configure without a resource compiler outside
# Windows: project() enables CXX only, and RC is enabled under if(WIN32).

file(READ "${LUMEX_SOURCE_DIR}/CMakeLists.txt" _root)

string(REGEX MATCH "project\\(LumexLib[^)]*\\)" _project "${_root}")
if(_project STREQUAL "")
    message(FATAL_ERROR "CMakeLists.txt: no project(LumexLib ...) call")
endif()
if(_project MATCHES "RC")
    message(FATAL_ERROR
        "project(LumexLib) enables RC on every platform:\n${_project}")
endif()

string(REGEX MATCH "if\\(WIN32\\)[ \t\r\n]*enable_language\\(RC\\)" _rc
       "${_root}")
if(_rc STREQUAL "")
    message(FATAL_ERROR
        "CMakeLists.txt: enable_language(RC) is not guarded by if(WIN32)")
endif()
