# core/atomic and core/smart_ptr wiring: the soft edge. lumex::atomic links
# lumex::smart_ptr and defines LUMEX_ATOMIC_HAS_SMART_PTR only when that target
# exists, so the atomic module builds without the pointer family (and the
# split-count engine is then not declared). The configure checks run the whole
# root project as a sub-build with the family ON and then OFF, and read the
# interface properties of lumex::atomic at configure time.

function(_require_text path needle)
    file(READ "${LUMEX_SOURCE_DIR}/${path}" _txt)
    string(FIND "${_txt}" "${needle}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR "${path} does not mention ${needle}")
    endif()
endfunction()

# --- part 1: the sources ---------------------------------------------------
set(_atomic "lumex/core/atomic/CMakeLists.txt")
_require_text("${_atomic}" "if(TARGET lumex::smart_ptr)")
_require_text("${_atomic}"
    "target_link_libraries(\${LUMEX_ATOMIC_NAME} INTERFACE lumex::smart_ptr)")
_require_text("${_atomic}"
    "LUMEX_ATOMIC_HAS_SMART_PTR=1)")
_require_text("conanfile.py" "requires=[\"core_hazard_pointer\", \"core_smart_ptr\"]")
_require_text("conanfile.py" "atomic.defines.append(\"LUMEX_ATOMIC_HAS_SMART_PTR=1\")")
_require_text("lumex/core/atomic/smart_ptr/LumexAtomicSmartPtrConfig.hpp"
    "#include \"lumex/core/atomic/dwcas/LumexDwcasConfig.hpp\"")

# --- part 2: configure the root project with the family ON and OFF ----------
set(_work "${CMAKE_BINARY_DIR}/wiring_atomic_smart_ptr_edge")
file(REMOVE_RECURSE "${_work}")
file(MAKE_DIRECTORY "${_work}/probe")
# The probe adds the root project and writes the interface properties of the
# atomic target at configure time (bracket argument: nothing is expanded here).
file(WRITE "${_work}/probe/CMakeLists.txt" [=[
cmake_minimum_required(VERSION 3.16)
project(AtomicSmartPtrEdgeProbe CXX)
add_subdirectory("${LUMEX_EDGE_SOURCE}" lumex)
get_target_property(_defs LumexCore_atomic INTERFACE_COMPILE_DEFINITIONS)
get_target_property(_links LumexCore_atomic INTERFACE_LINK_LIBRARIES)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/edge.txt"
    "defs=${_defs}\nlinks=${_links}\n")
]=])

function(_configure name family out_defs out_links)
    set(_build "${_work}/${name}")
    execute_process(
        COMMAND ${CMAKE_COMMAND}
            -S "${_work}/probe"
            -B "${_build}"
            -DLUMEX_EDGE_SOURCE=${LUMEX_SOURCE_DIR}
            -DLUMEX_BUILD_TESTS=OFF
            -DLUMEX_BUILD_DOCUMENTATION=OFF
            -DLUMEX_BUILD_ATOMIC=ON
            -DLUMEX_BUILD_SMART_PTR=${family}
        RESULT_VARIABLE _rv
        OUTPUT_VARIABLE _out
        ERROR_VARIABLE _err)
    if(NOT _rv EQUAL 0)
        message(FATAL_ERROR
            "configure with LUMEX_BUILD_SMART_PTR=${family} failed (${_rv})\n${_out}${_err}")
    endif()
    file(READ "${_build}/edge.txt" _edge)
    string(REGEX MATCH "defs=([^\n]*)" _ "${_edge}")
    set(${out_defs} "${CMAKE_MATCH_1}" PARENT_SCOPE)
    string(REGEX MATCH "links=([^\n]*)" _ "${_edge}")
    set(${out_links} "${CMAKE_MATCH_1}" PARENT_SCOPE)
endfunction()

# (a) the family ON: the macro and the link are on lumex::atomic.
_configure(on ON _on_defs _on_links)
string(FIND "${_on_defs}" "LUMEX_ATOMIC_HAS_SMART_PTR=1" _pos)
if(_pos EQUAL -1)
    message(FATAL_ERROR "LUMEX_BUILD_SMART_PTR=ON: lumex::atomic lacks LUMEX_ATOMIC_HAS_SMART_PTR=1 (${_on_defs})")
endif()
string(FIND "${_on_links}" "lumex::smart_ptr" _pos)
if(_pos EQUAL -1)
    message(FATAL_ERROR "LUMEX_BUILD_SMART_PTR=ON: lumex::atomic does not link lumex::smart_ptr (${_on_links})")
endif()

# (b) the family OFF: the configure succeeds and has neither.
_configure(off OFF _off_defs _off_links)
string(FIND "${_off_defs}" "LUMEX_ATOMIC_HAS_SMART_PTR" _pos)
if(NOT _pos EQUAL -1)
    message(FATAL_ERROR "LUMEX_BUILD_SMART_PTR=OFF: lumex::atomic still defines LUMEX_ATOMIC_HAS_SMART_PTR (${_off_defs})")
endif()
string(FIND "${_off_links}" "lumex::smart_ptr" _pos)
if(NOT _pos EQUAL -1)
    message(FATAL_ERROR "LUMEX_BUILD_SMART_PTR=OFF: lumex::atomic still links lumex::smart_ptr (${_off_links})")
endif()
