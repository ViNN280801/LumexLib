# Every semantic of LUMEX_CONTRACTS_SEMANTIC is accepted, in any letter case,
# and is handed to the module as the lower-case word the header spells.

include("${LUMEX_SOURCE_DIR}/lumex/tests/cmake/setup_all_on.cmake")
include("${LUMEX_SOURCE_DIR}/cmake/LumexModules.cmake")

foreach(_pair
        "IGNORE=ignore" "OBSERVE=observe" "ENFORCE=enforce"
        "QUICK_ENFORCE=quick_enforce" "P2900=p2900"
        "ignore=ignore" "Quick_Enforce=quick_enforce" "p2900=p2900")
    string(REPLACE "=" ";" _parts "${_pair}")
    list(GET _parts 0 _given)
    list(GET _parts 1 _expected)
    set(LUMEX_CONTRACTS_SEMANTIC "${_given}")
    set(LUMEX_CONTRACTS_SEMANTIC_NORMALIZED "stale")
    lumex_check_module_dependencies()
    if(NOT "${LUMEX_CONTRACTS_SEMANTIC_NORMALIZED}" STREQUAL "${_expected}")
        message(FATAL_ERROR
            "LUMEX_CONTRACTS_SEMANTIC=${_given} normalized to "
            "'${LUMEX_CONTRACTS_SEMANTIC_NORMALIZED}', expected '${_expected}'")
    endif()
endforeach()
