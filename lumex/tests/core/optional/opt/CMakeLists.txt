# lumex/tests/core/optional/opt/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake),
# each built from every *.cxx11.tests.cpp of this directory (and, from the
# standard in their name, the *.cxx17.tests.cpp ones). C++14 has a suite of its
# own: the relaxed constexpr rules (reset, nullopt assignment, non-const
# access in a constant expression) are checked there.
lumex_add_standard_suites(OptionalOpt
    MODULE optional
    LINK
        lumex::optional
)
