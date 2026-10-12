# lumex/tests/core/reflection/reflected_enum/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
# LumexReflectionReflectedEnumCxx11Tests builds LumexReflectedEnum.cxx11.tests.cpp
# and the Cxx14, Cxx17 and Cxx20 suites run the same file. It includes the
# umbrella without LUMEX_WITH_FIELD_REFLECTION and links lumex::reflection
# alone.
lumex_add_standard_suites(ReflectionReflectedEnum
    MODULE reflection
    LINK lumex::reflection
)
