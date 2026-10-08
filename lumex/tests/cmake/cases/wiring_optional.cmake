# core/optional wiring: the types header declares no global name and the global
# aliases live in their own header that the umbrella includes.

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

set(_types "lumex/core/optional/opt/LumexOptional.hpp")
set(_globals "lumex/core/optional/opt/LumexOptionalGlobals.hpp")

# The types header puts nothing at global scope; the globals header does, and
# the umbrella (what every other module includes) includes the globals header.
_forbid_text("${_types}" "using lumex::core::optional::opt::nullopt;")
_forbid_text("${_types}" "using lumex::core::optional::opt::make_optional;")
_forbid_text("${_types}" "using lumex::core::optional::opt::lumex_bad_optional_access;")
_forbid_text("${_types}" "template <typename T> using optional =")
_require_text("${_globals}" "using lumex::core::optional::opt::nullopt;")
_require_text("${_globals}" "using lumex::core::optional::opt::make_optional;")
_require_text("${_globals}" "using lumex::core::optional::opt::lumex_bad_optional_access;")
_require_text("${_globals}" "template <typename T> using optional = lumex::core::optional::opt::optional<T>;")
_require_text("${_globals}" "#include \"lumex/core/optional/opt/LumexOptional.hpp\"")
_require_text("lumex/core/optional/LumexOptional" "#include \"opt/LumexOptionalGlobals.hpp\"")
_forbid_text("lumex/core/optional/LumexOptional" "#include \"opt/LumexOptional.hpp\"")

# The test that keeps the types header's names out of the global namespace.
_require_text("lumex/tests/core/optional/opt/LumexOptionalGlobalNames.cxx11.tests.cpp"
    "lumex/core/optional/opt/LumexOptional.hpp")
