# lumex/tests/core/circular_buffer/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
#   LumexCircularBufferCxx11Tests  CircularBuffer.cxx11.tests.cpp and
#                                  CircularBufferHeader.cxx11.tests.cpp
#   LumexCircularBufferCxx20Tests  the same files at C++20, where
#                                  CircularBuffer.hpp takes its C++20 branches
#                                  (<=> and concepts)
lumex_add_standard_suites(CircularBuffer
    MODULE circular_buffer
    LINK
        lumex::circular_buffer
)
