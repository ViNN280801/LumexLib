// LumexDemangle.tests.cpp
#include <map>
#include <string>
#include <typeinfo>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/utility/demangle/LumexDemangle.hpp"
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#endif

using namespace lumex::core::utility::demangle;

namespace
{
namespace SomeNamespace
{
class SomeClass
{
};
} // namespace SomeNamespace

struct Nested
{
  struct Inner
  {
  };
};
} // namespace

using lumex::core::utility::demangle::demangle_type_name;

TEST (LumexDemangleTest,
      GivenBuiltinType_WhenLumDemangle_ThenReturnsNonEmptyString)
{
  std::string const name = lumDemangle (int);
  EXPECT_FALSE (name.empty ());
}

TEST (LumexDemangleTest,
      GivenTemplateType_WhenLumDemangle_ThenReturnsReadableNameOnGnu)
{
  std::string const name = lumDemangle (std::vector<int>);
  EXPECT_FALSE (name.empty ());
#ifdef __GNUG__
  // On GCC/Clang, __cxa_demangle turns "St6vectorIiSaIiEE" into something
  // containing "vector" and "int" in readable form.
  EXPECT_NE (name.find ("vector"), std::string::npos);
#else
  // On non-GNU compilers (e.g. MSVC), lumDemangle does not attempt any
  // demangling - it falls back to typeid(...).name() unchanged, which on MSVC
  // is already human-readable.
  EXPECT_EQ (name, std::string (typeid (std::vector<int>).name ()));
#endif
}

TEST (LumexDemangleTest,
      GivenNamespacedType_WhenLumDemangle_ThenNameMentionsClass)
{
  std::string const name = lumDemangle (SomeNamespace::SomeClass);
  EXPECT_FALSE (name.empty ());
#ifdef __GNUG__
  EXPECT_NE (name.find ("SomeClass"), std::string::npos);
#endif
}

TEST (LumexDemangleTest,
      GivenNullptr_WhenDemangleTypeName_ThenReturnsEmptyString)
{
  EXPECT_EQ (demangle_type_name (static_cast<char const *> (nullptr)),
             std::string ());
}

TEST (LumexDemangleTest,
      GivenRealMangledName_WhenDemangleTypeName_ThenMatchesLumDemangleMacro)
{
  // demangle_type_name() demangles an already-mangled name (e.g. captured from
  // typeid(...).name() separately), whereas lumDemangle()
  // mangles-then-demangles a type in one step. Both must produce the same
  // result for the same type, since they share the same underlying mechanism.
  std::string const mangled = typeid (std::vector<int>).name ();
  std::string const demangledViaFunc = demangle_type_name (mangled);
  std::string const demangledViaMacro = lumDemangle (std::vector<int>);

  EXPECT_EQ (demangledViaFunc, demangledViaMacro);
}

TEST (LumexDemangleTest,
      GivenStdStringOverload_WhenDemangleTypeName_ThenMatchesCStringOverload)
{
  std::string const mangled = typeid (SomeNamespace::SomeClass).name ();

  EXPECT_EQ (demangle_type_name (mangled),
             demangle_type_name (mangled.c_str ()));
}

TEST (
    LumexDemangleTest,
    GivenGarbageInput_WhenDemangleTypeName_ThenReturnsInputUnchangedOnFailure)
{
  // Not a valid mangled name - __cxa_demangle must fail, and the function must
  // fall back to returning the input unchanged (rather than throwing or
  // crashing).
  std::string const garbage = "not_a_real_mangled_name_12345";
  EXPECT_EQ (demangle_type_name (garbage), garbage);
}

TEST (LumexDemangleTest,
      GivenEmptyString_WhenDemangleTypeName_ThenReturnsEmptyString)
{
  EXPECT_EQ (demangle_type_name (std::string ()), std::string ());
}

TEST (LumexDemangleTest,
      GivenVoidType_WhenLumDemangle_ThenReturnsNonEmptyString)
{
  std::string const name = lumDemangle (void);
  EXPECT_FALSE (name.empty ());
}

TEST (LumexDemangleTest,
      GivenPointerAndConstTypes_WhenLumDemangle_ThenReturnsNonEmpty)
{
  EXPECT_FALSE (lumDemangle (int *).empty ());
  EXPECT_FALSE (lumDemangle (int const).empty ());
  EXPECT_FALSE (lumDemangle (int const *).empty ());
  EXPECT_FALSE (lumDemangle (int const &).empty ());
}

TEST (LumexDemangleTest,
      GivenNestedClass_WhenLumDemangle_ThenMentionsInnerOnGnu)
{
  std::string const name = lumDemangle (Nested::Inner);
  EXPECT_FALSE (name.empty ());
#ifdef __GNUG__
  EXPECT_NE (name.find ("Inner"), std::string::npos);
#endif
}

TEST (LumexDemangleTest,
      GivenNestedTemplates_WhenLumDemangle_ThenStaysNonEmptyAndStable)
{
  using NestedMap = std::map<std::string, std::vector<int>>;
  std::string const first = lumDemangle (NestedMap);
  std::string const second = lumDemangle (NestedMap);
  EXPECT_FALSE (first.empty ());
  EXPECT_EQ (first, second);
#ifdef __GNUG__
  EXPECT_NE (first.find ("map"), std::string::npos);
#endif
}

TEST (LumexDemangleTest,
      GivenSameTypeTwice_WhenDemangleTypeName_ThenResultsAreIdentical)
{
  std::string const mangled = typeid (double).name ();
  EXPECT_EQ (demangle_type_name (mangled), demangle_type_name (mangled));
}

TEST (LumexDemangleTest,
      GivenWhitespaceOnly_WhenDemangleTypeName_ThenReturnsInputUnchanged)
{
  std::string const spaces = "   ";
  EXPECT_EQ (demangle_type_name (spaces), spaces);
}

TEST (LumexDemangleTest,
      GivenEmbeddedNulLooksLikeCStringEnd_WhenDemangleTypeName_ThenUsesCStr)
{
  std::string const with_nul ("abc\0def", 7);
  EXPECT_EQ (demangle_type_name (with_nul),
             demangle_type_name (with_nul.c_str ()));
}

TEST (LumexDemangleTest,
      GivenFunctionPointerType_WhenLumDemangle_ThenReturnsNonEmpty)
{
  using Fn = int (*) (int, double);
  EXPECT_FALSE (lumDemangle (Fn).empty ());
}

TEST (LumexDemangleTest,
      GivenBuiltinFamily_WhenLumDemangle_ThenEachNameIsNonEmpty)
{
  EXPECT_FALSE (lumDemangle (bool).empty ());
  EXPECT_FALSE (lumDemangle (char).empty ());
  EXPECT_FALSE (lumDemangle (float).empty ());
  EXPECT_FALSE (lumDemangle (double).empty ());
  EXPECT_FALSE (lumDemangle (long).empty ());
  EXPECT_FALSE (lumDemangle (unsigned).empty ());
}

#ifdef __GNUG__
TEST (LumexDemangleTest,
      GivenGnuIntType_WhenLumDemangle_ThenReadableNameContainsInt)
{
  std::string const name = lumDemangle (int);
  EXPECT_NE (name.find ("int"), std::string::npos);
}
#endif

TEST (LumexDemangleTest,
      GivenLongGarbage_WhenDemangleTypeName_ThenDoesNotThrowAndKeepsInput)
{
  std::string const long_garbage (4096, 'Z');
  EXPECT_EQ (demangle_type_name (long_garbage), long_garbage);
}
