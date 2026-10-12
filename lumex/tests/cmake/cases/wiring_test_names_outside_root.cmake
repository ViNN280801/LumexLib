# Expected to fail: a directory outside the root has no derived CTest name
# prefix. A silent empty prefix would register a test without its module
# name, so lumex_test_name_prefix stops the configure instead.

include("${LUMEX_SOURCE_DIR}/cmake/LumexTestNames.cmake")

lumex_test_name_prefix(_prefix
    "${LUMEX_SOURCE_DIR}/lumex/examples/atomic"
    "${LUMEX_SOURCE_DIR}/lumex/tests")
message(STATUS "unexpected prefix '${_prefix}'")
