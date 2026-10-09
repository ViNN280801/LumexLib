# Export of the contracts library on Windows. The violation handler (the slot
# and the default handler) must be one per process, so it lives in one library
# and every other DLL imports it: LUMEX_CONTRACTS_API is dllexport only where
# LumexCore_contracts_EXPORTS is defined (the library itself) and dllimport
# elsewhere, and only free functions carry it (a dllimport class with inline
# members would need the __imp_ symbols of inline members).
#
# Part 1 reads the sources. Part 2 compiles with a MinGW compiler (when one is
# installed) the way the DLL is built and reads the export directives
# (.drectve) of the object file: the library exports exactly the four free
# functions of the handler, and a translation unit of another DLL that includes
# the umbrella and uses the macros exports none of them.

function(_require_text path needle)
    file(READ "${LUMEX_SOURCE_DIR}/${path}" _txt)
    string(FIND "${_txt}" "${needle}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR "${path} does not mention ${needle}")
    endif()
endfunction()

# --- part 1: the sources ---------------------------------------------------
_require_text("lumex/LumexExport.hpp"
    "#if defined(LumexCore_contracts_EXPORTS)\n#define LUMEX_CONTRACTS_API __declspec (dllexport)\n#else\n#define LUMEX_CONTRACTS_API __declspec (dllimport)")
_require_text("lumex/LumexExport.hpp"
    "#elif defined(LumexCore_contracts_EXPORTS)\n#define LUMEX_CONTRACTS_API __attribute__ ((visibility (\"default\")))")
# The declarations, white space normalized (clang-format breaks them freely).
file(READ "${LUMEX_SOURCE_DIR}/lumex/core/contracts/handler/LumexContractsHandler.hpp" _handler)
string(REGEX REPLACE "[ \t\r\n]+" " " _handler "${_handler}")
foreach(_function
        "LUMEX_CONTRACTS_API violation_handler_type set_violation_handler (violation_handler_type handler) LUMEX_NOEXCEPT;"
        "LUMEX_CONTRACTS_API violation_handler_type get_violation_handler () LUMEX_NOEXCEPT;"
        "LUMEX_CONTRACTS_API void invoke_violation_handler (contract_violation const &violation);"
        "LUMEX_CONTRACTS_API void invoke_default_violation_handler ( contract_violation const &violation) LUMEX_NOEXCEPT;")
    string(FIND "${_handler}" "${_function}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR "the handler header does not declare: ${_function}")
    endif()
endforeach()
# Free functions only: no class carries an export macro, the generic one is
# not used (it would export the module from every DLL built with
# LUMEX_EXPORTS).
file(GLOB_RECURSE _headers "${LUMEX_SOURCE_DIR}/lumex/core/contracts/*.hpp")
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
# The slot is touched from the compiled source only (a header-only slot would
# be one copy per DLL on Windows).
file(GLOB_RECURSE _all_headers "${LUMEX_SOURCE_DIR}/lumex/core/contracts/*.hpp"
                               "${LUMEX_SOURCE_DIR}/lumex/core/contracts/LumexContracts")
foreach(_header IN LISTS _all_headers)
    file(READ "${_header}" _txt)
    string(FIND "${_txt}" "lumex_callback_slot<" _slot)
    if(NOT _slot EQUAL -1)
        message(FATAL_ERROR "${_header} instantiates the callback slot; only the library source may")
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

set(_work "${CMAKE_BINARY_DIR}/contracts_export")
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

# The library: exactly the four handler functions.
_exports(_library lumex/core/contracts/handler/LumexContractsHandler.cpp
         -DLumexCore_contracts_EXPORTS)
set(_expected set_violation_handler get_violation_handler
              invoke_violation_handler invoke_default_violation_handler)
foreach(_function IN LISTS _expected)
    set(_found FALSE)
    foreach(_directive IN LISTS _library)
        if(_directive MATCHES "contracts[0-9A-Za-z_]*${_function}")
            set(_found TRUE)
        endif()
    endforeach()
    if(NOT _found)
        message(FATAL_ERROR "the contracts library does not export ${_function}:\n${_library}")
    endif()
endforeach()
list(LENGTH _library _exported)
list(LENGTH _expected _expected_count)
if(NOT _exported EQUAL _expected_count)
    message(FATAL_ERROR
        "the contracts library exports ${_exported} symbols, expected "
        "${_expected_count} free functions:\n${_library}")
endif()

# Another DLL that includes the umbrella and uses the macros: no contracts
# symbol is exported from it.
set(_user "${_work}/User.cpp")
file(WRITE "${_user}" [=[
#include "lumex/core/contracts/LumexContracts"

namespace
{
void
handler (lumex::core::contracts::contract_violation const &)
{
}
}

int
user_of_contracts (int value)
{
  lumex::core::contracts::set_violation_handler (&handler);
  LUMEX_CONTRACT_ASSERT_OBSERVE (value > 0);
  LUMEX_CONTRACT_ASSERT_IGNORE (value > 1);
  LUMEX_CONTRACT_ASSERT (value > -5);
  lumex::core::contracts::scoped_violation_handler const scope (nullptr);
  return value;
}
]=])
_exports(_user_exports "${_user}")
foreach(_directive IN LISTS _user_exports)
    if(_directive MATCHES "contracts")
        message(FATAL_ERROR "a user DLL exports contracts symbols: ${_directive}")
    endif()
endforeach()
