# core/optional wiring: the types header declares no global name, the global
# aliases live in their own header that the umbrella includes, and utility
# (LumexMemRead.hpp returns the optional of this library before C++17) requires
# the module: the configure-time edge, the link and the Conan requirement.

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

# LumexMemRead.hpp takes the types header: the umbrella would put optional and
# nullopt into every file that includes the utility umbrella.
_require_text("lumex/core/utility/mem/LumexMemRead.hpp"
    "#include \"lumex/core/optional/opt/LumexOptional.hpp\"")
_forbid_text("lumex/core/utility/mem/LumexMemRead.hpp"
    "#include \"lumex/core/optional/LumexOptional\"")

# utility requires optional.
_require_text("cmake/LumexModules.cmake"
    "lumex_require_module(LUMEX_BUILD_UTILITY LUMEX_BUILD_OPTIONAL)")
_require_text("lumex/core/utility/CMakeLists.txt" "lumex::optional")
_require_text("conanfile.py" "[\"core_math\", \"core_optional\", \"core_span\"]")
_require_text("lumex/core/CMakeLists.txt" "lumex_add_subdirectory_if(LUMEX_BUILD_OPTIONAL optional)")

# optional is added before utility (ALIAS targets must exist when utility
# links them).
file(READ "${LUMEX_SOURCE_DIR}/lumex/core/CMakeLists.txt" _core)
string(FIND "${_core}" "LUMEX_BUILD_OPTIONAL optional" _optional_pos)
string(FIND "${_core}" "LUMEX_BUILD_UTILITY utility" _utility_pos)
if(_optional_pos GREATER _utility_pos)
    message(FATAL_ERROR "optional must be added before utility in lumex/core/CMakeLists.txt")
endif()

# The test that keeps the types header's names out of the global namespace.
_require_text("lumex/tests/core/optional/opt/LumexOptionalGlobalNames.cxx11.tests.cpp"
    "lumex/core/optional/opt/LumexOptional.hpp")
