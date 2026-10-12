# lumex/tests/core/reflection/var_info/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
# LumexReflectionVarInfoCxx11Tests builds LumexVarInfo.cxx11.tests.cpp and the
# Cxx14, Cxx17 and Cxx20 suites run the same file. It includes the umbrella
# without LUMEX_WITH_FIELD_REFLECTION and links lumex::reflection alone.
# STANDARDS narrows the module's row to 11 17 20 for this directory. Nothing in
# its tests or in the sources they test differs at C++14 from the next lower
# standard (the text of the directory's own files after preprocessing is the
# same, no __cplusplus threshold or LUMEX_HAS_* feature of those standards is
# used there; "Narrowing the row of one directory" in
# lumex/tests/LumexTestStandards.cmake), so the dropped suites would run the
# same code again; the suite names above for them do not exist.
lumex_add_standard_suites(ReflectionVarInfo
    MODULE reflection
    LINK lumex::reflection
    STANDARDS 11 17 20
)
