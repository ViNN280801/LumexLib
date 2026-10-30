# lumex/tests/applied/transport/protocol/CMakeLists.txt

# The dialogue layer over the protocol tables (lumex/applied/transport/
# protocol): its vocabulary (dsl/), header-only over the protocol module, and
# the engine (link/: frame_link, dialogue/: dialogue), which the transport
# compiles only with the framing module too. ../CMakeLists.txt adds this
# directory only when the protocol module is built.
add_subdirectory(dsl)
lumex_effective_on(_transport_tests_with_dialogue LUMEX_BUILD_FRAMING)
if(_transport_tests_with_dialogue)
    add_subdirectory(dialogue)
    add_subdirectory(link)
endif()
