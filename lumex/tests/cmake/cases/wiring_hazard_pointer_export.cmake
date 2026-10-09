# Export of the hazard pointer library on Windows. The engine (the slot pool,
# the thread caches, the retired lists) must be one per process, so it lives in
# one library and every other DLL imports it: LUMEX_HAZARD_POINTER_API is
# dllexport only where LumexCore_hazard_pointer_EXPORTS is defined (the library
# itself) and dllimport elsewhere, and only free functions carry it (a dllimport
# class with inline members would need the __imp_ symbols of inline members).
#
# Part 1 reads the sources. Part 2 compiles with a MinGW compiler (when one is
# installed) the way the DLL is built and reads the export directives
# (.drectve) of the object file: the library exports exactly the engine's free
# functions, and a translation unit of another DLL that includes the umbrella
# exports none of them.

function(_require_text path needle)
    file(READ "${LUMEX_SOURCE_DIR}/${path}" _txt)
    string(FIND "${_txt}" "${needle}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR "${path} does not mention ${needle}")
    endif()
endfunction()

function(_forbid_text path needle)
    file(READ "${LUMEX_SOURCE_DIR}/${path}" _txt)
    string(FIND "${_txt}" "${needle}" _pos)
    if(NOT _pos EQUAL -1)
        message(FATAL_ERROR "${path} mentions ${needle}")
    endif()
endfunction()

# --- part 1: the sources ---------------------------------------------------
_require_text("lumex/LumexExport.hpp"
    "#if defined(LumexCore_hazard_pointer_EXPORTS)\n#define LUMEX_HAZARD_POINTER_API __declspec (dllexport)\n#else\n#define LUMEX_HAZARD_POINTER_API __declspec (dllimport)")
_require_text("lumex/LumexExport.hpp"
    "#elif defined(LumexCore_hazard_pointer_EXPORTS)\n#define LUMEX_HAZARD_POINTER_API __attribute__ ((visibility (\"default\")))")
set(_engine "lumex/core/hazard_pointer/engine/LumexHazardPointerEngine.hpp")
foreach(_function
        "LUMEX_HAZARD_POINTER_API slot_t *acquire_slot ();"
        "LUMEX_HAZARD_POINTER_API void acquire_slots (slot_t **out, std::size_t count);"
        "LUMEX_HAZARD_POINTER_API void release_slot (slot_t *slot) LUMEX_NOEXCEPT;"
        "LUMEX_HAZARD_POINTER_API void retire_node (node_t *node) LUMEX_NOEXCEPT;"
        "LUMEX_HAZARD_POINTER_API bool reclaim_or_retire (node_t *node) LUMEX_NOEXCEPT;"
        "LUMEX_HAZARD_POINTER_API void clean_up () LUMEX_NOEXCEPT;"
        "LUMEX_HAZARD_POINTER_API statistics_t statistics () LUMEX_NOEXCEPT;")
    _require_text("${_engine}" "${_function}")
endforeach()
# Free functions only: no class carries an export macro, the generic one is
# not used (it would export the module from every DLL built with
# LUMEX_EXPORTS).
file(GLOB_RECURSE _headers "${LUMEX_SOURCE_DIR}/lumex/core/hazard_pointer/*.hpp")
foreach(_header IN LISTS _headers)
    file(READ "${_header}" _txt)
    if(_txt MATCHES "class[ \t\n]+LUMEX_[A-Z_]*API" OR _txt MATCHES "struct[ \t\n]+LUMEX_[A-Z_]*API")
        message(FATAL_ERROR "${_header}: a class carries an export macro")
    endif()
    string(FIND "${_txt}" "LUMEX_API" _generic)
    if(NOT _generic EQUAL -1)
        message(FATAL_ERROR "${_header} uses the generic LUMEX_API")
    endif()
endforeach()

# --- part 2: the object files of a MinGW build -----------------------------
find_program(_mingw NAMES x86_64-w64-mingw32-g++-posix
                          x86_64-w64-mingw32-g++-win32
                          x86_64-w64-mingw32-g++)
find_program(_strings NAMES strings)
if(NOT _mingw OR NOT _strings)
    message(STATUS "no MinGW compiler or strings: only the sources were checked")
    return()
endif()

set(_work "${CMAKE_BINARY_DIR}/hazard_pointer_export")
file(MAKE_DIRECTORY "${_work}")

function(_exports out_var source)
    get_filename_component(_name "${source}" NAME_WE)
    set(_object "${_work}/${_name}.o")
    if(IS_ABSOLUTE "${source}")
        set(_source_path "${source}")
    else()
        set(_source_path "${LUMEX_SOURCE_DIR}/${source}")
    endif()
    execute_process(
        COMMAND "${_mingw}" -std=c++11 -c -DLUMEX_EXPORTS -DNOMINMAX
            -DWIN32_LEAN_AND_MEAN "-I${LUMEX_SOURCE_DIR}"
            ${ARGN} "${_source_path}" -o "${_object}"
        RESULT_VARIABLE _rv
        ERROR_VARIABLE _err)
    if(NOT _rv EQUAL 0)
        message(FATAL_ERROR "${source} does not compile with ${_mingw}:\n${_err}")
    endif()
    execute_process(COMMAND "${_strings}" -a "${_object}"
        OUTPUT_VARIABLE _text)
    string(REGEX MATCHALL "-export:\"[^\"]*\"" _all "${_text}")
    set(${out_var} "${_all}" PARENT_SCOPE)
endfunction()

# The library: exactly the seven engine functions.
_exports(_library lumex/core/hazard_pointer/engine/LumexHazardPointerDomain.cpp
         -DLumexCore_hazard_pointer_EXPORTS)
set(_expected acquire_slot acquire_slots release_slot retire_node reclaim_or_retire clean_up statistics)
foreach(_function IN LISTS _expected)
    set(_found FALSE)
    foreach(_directive IN LISTS _library)
        if(_directive MATCHES "engine[0-9A-Za-z_]*${_function}")
            set(_found TRUE)
        endif()
    endforeach()
    if(NOT _found)
        message(FATAL_ERROR "the hazard pointer library does not export ${_function}:\n${_library}")
    endif()
endforeach()
list(LENGTH _library _exported)
list(LENGTH _expected _expected_count)
if(NOT _exported EQUAL _expected_count)
    message(FATAL_ERROR
        "the hazard pointer library exports ${_exported} symbols, expected "
        "${_expected_count} free functions:\n${_library}")
endif()

# Another DLL that includes the umbrella and uses the classes: no hazard
# pointer symbol is exported from it.
set(_user "${_work}/User.cpp")
file(WRITE "${_user}" [=[
#include <atomic>
#include "lumex/core/hazard_pointer/LumexHazardPointer"

namespace
{
struct item : lumex::core::hazard_pointer::hazard_pointer_obj_base<item>
{
};
}

int
user_of_hazard_pointers ()
{
  lumex::core::hazard_pointer::hazard_pointer holder
      = lumex::core::hazard_pointer::make_hazard_pointer ();
  item *object = new item;
  std::atomic<item *> source (object);
  item *got = holder.protect (source);
  source.store (nullptr);
  got->retire ();
  lumex::core::hazard_pointer::clean_up ();
  return 0;
}
]=])
_exports(_user_exports "${_user}")
foreach(_directive IN LISTS _user_exports)
    if(_directive MATCHES "hazard_pointer")
        message(FATAL_ERROR "a user DLL exports hazard pointer symbols: ${_directive}")
    endif()
endforeach()
