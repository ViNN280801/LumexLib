// LumexTypeTraits.tests.cpp
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#if __cplusplus >= 201703L
#include <optional>
#endif

#include <gtest/gtest.h>

#include "lumex/core/optional/LumexOptional"
#include "lumex/core/utility/assert/LumexAssert.hpp"
#include "lumex/core/utility/traits/LumexTypeTraits.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wswitch-enum"
#pragma clang diagnostic ignored "-Wnrvo"
#pragma clang diagnostic ignored "-Wheader-hygiene"
#pragma clang diagnostic ignored "-Wused-but-marked-unused"
#pragma clang diagnostic ignored "-Wundefined-var-template"
#pragma clang diagnostic ignored "-Wdeprecated-redundant-constexpr-static-def"
#pragma clang diagnostic ignored "-Wvariadic-macro-arguments-omitted"
#pragma clang diagnostic ignored "-Wunused-result"
#pragma clang diagnostic ignored "-Wextra-semi-stmt"
#pragma clang diagnostic ignored "-Wexpansion-to-defined"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wundefined-func-template"
#pragma clang diagnostic ignored "-Wfloat-equal"
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif

#if defined(__clang__)
#endif

using namespace lumex::core::utility::traits;

namespace
{
int
free_scale (int x)
{
  return x * 2;
}

struct Functor
{
  int
  operator() (int x) const
  {
    return x * 2;
  }
};

struct OtherFunctor
{
  std::string
  operator() (std::string const &s) const
  {
    return s;
  }
};

struct MyClass
{
  int member = 7;

  int
  method (int x) const
  {
    return x * 2;
  }

  int
  mutating (int x)
  {
    member = x;
    return x;
  }
};

struct DerivedClass : MyClass
{
};

struct NotDefaultConstructible
{
  explicit NotDefaultConstructible (int) {}
};

struct DefaultUser
{
  int value = 4;
};

struct AbstractBase
{
  virtual ~AbstractBase () {}
  virtual void run () = 0;
};

struct HasTypeMember
{
  using type = int;
};

struct NoTypeMember
{
};
} // namespace

// --- void_t / make_void / has_type
// -------------------------------------------------------------

TEST (LumexTypeTraitsTest, GivenTypes_WhenVoidT_ThenYieldsVoid)
{
  LUMEX_STATIC_ASSERT_MSG ((std::is_same<void_t<>, void>::value),
                           "void_t<> is void");
  LUMEX_STATIC_ASSERT_MSG (
      (std::is_same<void_t<int, std::string>, void>::value),
      "void_t<Ts...> is void");
  LUMEX_STATIC_ASSERT_MSG ((std::is_same<make_void<int>::type, void>::value),
                           "make_void<int>::type is void");
  SUCCEED ();
}

TEST (LumexTypeTraitsTest, GivenTypeMember_WhenHasType_ThenTrue)
{
  LUMEX_STATIC_ASSERT_MSG ((has_type<HasTypeMember>::value),
                           "HasTypeMember has ::type");
  EXPECT_TRUE (has_type<HasTypeMember>::value);
}

TEST (LumexTypeTraitsTest, GivenNoTypeMember_WhenHasType_ThenFalse)
{
  LUMEX_STATIC_ASSERT_MSG ((!has_type<NoTypeMember>::value),
                           "NoTypeMember has no ::type");
  EXPECT_FALSE (has_type<NoTypeMember>::value);
}

TEST (LumexTypeTraitsTest,
      GivenFreeFunctionSignature_WhenUsingAlias_ThenCallable)
{
  using free_ref_t = int (&) (int);
  EXPECT_TRUE ((is_callable_signature<free_ref_t (int)>::value));
  EXPECT_FALSE ((is_callable_signature<free_ref_t (std::string)>::value));
}

// --- invoke_result / result_of
// -----------------------------------------------------------------

TEST (LumexTypeTraitsTest, GivenFunctor_WhenInvokeResult_ThenInt)
{
  LUMEX_STATIC_ASSERT_MSG (
      (std::is_same<invoke_result_t<Functor, int>, int>::value),
      "Functor(int) returns int");
  LUMEX_STATIC_ASSERT_MSG (
      (std::is_same<result_of_t<Functor (int)>, int>::value),
      "result_of<Functor(int)> is int");
  SUCCEED ();
}

TEST (LumexTypeTraitsTest, GivenFreeFunction_WhenInvokeResult_ThenInt)
{
  LUMEX_STATIC_ASSERT_MSG (
      (std::is_same<invoke_result_t<int (&) (int), int>, int>::value),
      "free function invoke_result is int");
  SUCCEED ();
}

TEST (LumexTypeTraitsTest, GivenMemberFunction_WhenInvokeResult_ThenInt)
{
  using PMF = int (MyClass::*) (int) const;
  LUMEX_STATIC_ASSERT_MSG (
      (std::is_same<invoke_result_t<PMF, MyClass &, int>, int>::value),
      "member function invoke_result is int");
  SUCCEED ();
}

TEST (LumexTypeTraitsTest, GivenMemberData_WhenInvokeResult_ThenIntRef)
{
  using PMD = int MyClass::*;
  LUMEX_STATIC_ASSERT_MSG (
      (std::is_same<invoke_result_t<PMD, MyClass &>, int &>::value),
      "member data invoke_result is int&");
  SUCCEED ();
}

TEST (LumexTypeTraitsTest, GivenMismatchedArgs_WhenInvokeResult_ThenNoType)
{
  LUMEX_STATIC_ASSERT_MSG (
      (!has_type<invoke_result<Functor, std::string>>::value),
      "Functor(string) must not have ::type");
  LUMEX_STATIC_ASSERT_MSG (
      (!has_type<result_of<Functor (std::string)>>::value),
      "result_of<Functor(string)> must not have ::type");
  SUCCEED ();
}

// --- is_callable_signature (DChannel signature form)
// -------------------------------------------

TEST (LumexTypeTraitsTest,
      GivenFunctorSignature_WhenIsCallableSignature_ThenTrue)
{
  LUMEX_STATIC_ASSERT_MSG ((is_callable_signature<Functor (int)>::value),
                           "Functor(int) must be callable");
  EXPECT_TRUE (is_callable_signature<Functor (int)>::value);
}

TEST (LumexTypeTraitsTest,
      GivenMismatchedSignature_WhenIsCallableSignature_ThenFalse)
{
  LUMEX_STATIC_ASSERT_MSG (
      (!is_callable_signature<Functor (std::string)>::value),
      "Functor cannot be called with a std::string");
  EXPECT_FALSE (is_callable_signature<Functor (std::string)>::value);
}

TEST (LumexTypeTraitsTest,
      GivenFreeFunctionSignature_WhenIsCallableSignature_ThenTrue)
{
  using free_ref_t = int (&) (int);
  EXPECT_TRUE ((is_callable_signature<free_ref_t (int)>::value));
}

TEST (LumexTypeTraitsTest,
      GivenWrongAritySignature_WhenIsCallableSignature_ThenFalse)
{
  EXPECT_FALSE ((is_callable_signature<Functor ()>::value));
  EXPECT_FALSE ((is_callable_signature<Functor (int, int)>::value));
}

// --- is_callable / is_invocable (PeakExpert variadic form)
// -------------------------------------

TEST (LumexTypeTraitsTest, GivenFunctorAndArgs_WhenIsCallable_ThenTrue)
{
  LUMEX_STATIC_ASSERT_MSG ((is_callable<Functor, int>::value),
                           "Functor must be callable with an int");
  EXPECT_TRUE ((is_callable<Functor, int>::value));
  EXPECT_TRUE ((is_callable_v<Functor, int>));
  EXPECT_TRUE ((is_invocable<Functor, int>::value));
  EXPECT_TRUE ((is_invocable_v<Functor, int>));
}

TEST (LumexTypeTraitsTest, GivenFreeFunction_WhenIsCallable_ThenTrue)
{
  EXPECT_TRUE ((is_callable<int (&) (int), int>::value));
  EXPECT_TRUE ((is_callable<decltype (free_scale), int>::value));
  EXPECT_FALSE ((is_callable<decltype (free_scale), std::string>::value));
}

TEST (LumexTypeTraitsTest, GivenFunctionPointer_WhenIsCallable_ThenTrue)
{
  using FnPtr = int (*) (int);
  EXPECT_TRUE ((is_callable<FnPtr, int>::value));
  EXPECT_FALSE ((is_callable<FnPtr, std::string>::value));
}

TEST (LumexTypeTraitsTest, GivenMemberFunctionPointer_WhenIsCallable_ThenTrue)
{
  using PMF = int (MyClass::*) (int) const;
  LUMEX_STATIC_ASSERT_MSG ((is_callable<PMF, MyClass &, int>::value),
                           "member function pointer must be Callable");
  EXPECT_TRUE ((is_callable<PMF, MyClass &, int>::value));
  EXPECT_TRUE ((is_callable<PMF, MyClass *, int>::value));
  EXPECT_TRUE ((is_callable<PMF, DerivedClass &, int>::value));
  EXPECT_TRUE ((is_callable<PMF, DerivedClass *, int>::value));
  EXPECT_FALSE ((is_callable<PMF, MyClass &>::value));
  EXPECT_FALSE ((is_callable<PMF, int, int>::value));
}

TEST (LumexTypeTraitsTest,
      GivenMemberFunctionPointer_WhenCalledThroughReferenceWrapper_ThenTrue)
{
  using PMF = int (MyClass::*) (int) const;
  EXPECT_TRUE (
      (is_callable<PMF, std::reference_wrapper<MyClass>, int>::value));
  EXPECT_TRUE (
      (is_callable<PMF, std::reference_wrapper<MyClass const>, int>::value));
}

TEST (LumexTypeTraitsTest,
      GivenMemberFunctionPointer_WhenCalledThroughUniquePtr_ThenTrue)
{
  using PMF = int (MyClass::*) (int) const;
  EXPECT_TRUE ((is_callable<PMF, std::unique_ptr<MyClass>, int>::value));
}

TEST (LumexTypeTraitsTest, GivenNonConstMember_WhenConstObject_ThenFalse)
{
  using PMF = int (MyClass::*) (int);
  EXPECT_TRUE ((is_callable<PMF, MyClass &, int>::value));
  EXPECT_FALSE ((is_callable<PMF, MyClass const &, int>::value));
}

TEST (LumexTypeTraitsTest, GivenMemberDataPointer_WhenIsCallable_ThenTrue)
{
  using PMD = int MyClass::*;
  LUMEX_STATIC_ASSERT_MSG ((is_callable<PMD, MyClass &>::value),
                           "member data pointer is Callable per the standard");
  EXPECT_TRUE ((is_callable<PMD, MyClass &>::value));
  EXPECT_TRUE ((is_callable<PMD, MyClass *>::value));
  EXPECT_TRUE ((is_callable<PMD, DerivedClass &>::value));
  EXPECT_TRUE ((is_callable<PMD, std::reference_wrapper<MyClass>>::value));
  EXPECT_FALSE ((is_callable<PMD, int>::value));
}

TEST (LumexTypeTraitsTest, GivenPlainPointer_WhenIsCallable_ThenFalse)
{
  LUMEX_STATIC_ASSERT_MSG ((!is_callable<int *, int>::value),
                           "a plain pointer is not Callable");
  EXPECT_FALSE ((is_callable<int *, int>::value));
  EXPECT_FALSE ((is_invocable_v<int *, int>));
}

TEST (LumexTypeTraitsTest, GivenOtherFunctor_WhenWrongArgs_ThenFalse)
{
  EXPECT_TRUE ((is_callable<OtherFunctor, std::string>::value));
  EXPECT_FALSE ((is_callable<OtherFunctor, int>::value));
}

// --- CleanType / CleanTypeOf / RemovePtr / safe_cast
// -------------------------------------------

TEST (LumexTypeTraitsTest, GivenCvRef_WhenCleanType_ThenBareValueType)
{
  LUMEX_STATIC_ASSERT_MSG ((std::is_same<CleanType<int>, int>::value),
                           "int stays int");
  LUMEX_STATIC_ASSERT_MSG ((std::is_same<CleanType<int const>, int>::value),
                           "const int becomes int");
  LUMEX_STATIC_ASSERT_MSG ((std::is_same<CleanType<int &>, int>::value),
                           "int& becomes int");
  LUMEX_STATIC_ASSERT_MSG ((std::is_same<CleanType<int const &>, int>::value),
                           "int const& becomes int");
  LUMEX_STATIC_ASSERT_MSG ((std::is_same<CleanType<int &&>, int>::value),
                           "int&& becomes int");
  LUMEX_STATIC_ASSERT_MSG ((std::is_same<CleanType<int const &&>, int>::value),
                           "int const&& becomes int");
  LUMEX_STATIC_ASSERT_MSG (
      (std::is_same<CleanType<int volatile &>, int>::value),
      "volatile ref becomes int");
  LUMEX_STATIC_ASSERT_MSG (
      (std::is_same<CleanTypeOf<int const &>, int>::value),
      "CleanTypeOf matches CleanType");
#if __cplusplus >= 202002L
  LUMEX_STATIC_ASSERT_MSG (
      (std::is_same<CleanType<int const &>,
                    std::remove_cvref_t<int const &>>::value),
      "C++20 CleanType is remove_cvref_t");
#endif
  SUCCEED ();
}

TEST (LumexTypeTraitsTest,
      GivenPointerType_WhenRemovePtr_ThenYieldsPointeeType)
{
  LUMEX_STATIC_ASSERT_MSG ((std::is_same<RemovePtr<int *>, int>::value),
                           "RemovePtr<int*> must be int");
  LUMEX_STATIC_ASSERT_MSG ((std::is_same<RemovePtr<int>, int>::value),
                           "RemovePtr<int> must remain int");
  LUMEX_STATIC_ASSERT_MSG (
      (std::is_same<RemovePtr<int const *>, int const>::value),
      "RemovePtr does not strip pointee cv");
  LUMEX_STATIC_ASSERT_MSG ((std::is_same<RemovePtr<int **>, int *>::value),
                           "RemovePtr strips one pointer");
  SUCCEED ();
}

TEST (LumexTypeTraitsTest, GivenNumericValue_WhenSafeCast_ThenConverts)
{
  double const as_double = safe_cast<double> (3);
  EXPECT_DOUBLE_EQ (as_double, 3.0);
  int const as_int = safe_cast<int> (4.9);
  EXPECT_EQ (as_int, 4);
}

TEST (LumexTypeTraitsTest, GivenPointer_WhenSafeCast_ThenRebinds)
{
  int value = 9;
  int const *const src = &value;
  int const *const dst = safe_cast<int const *> (src);
  EXPECT_EQ (dst, src);
}

// --- default_return<T>
// -----------------------------------------------------------------------

TEST (LumexTypeTraitsTest, GivenIntType_WhenDefaultReturnValue_ThenReturnsZero)
{
  EXPECT_EQ (default_return<int>::value (), 0);
}

TEST (LumexTypeTraitsTest,
      GivenPointerType_WhenDefaultReturnValue_ThenReturnsNullptr)
{
  EXPECT_EQ (default_return<int *>::value (), nullptr);
  EXPECT_EQ (default_return<MyClass *>::value (), nullptr);
}

TEST (LumexTypeTraitsTest, GivenVoidType_WhenDefaultReturnValue_ThenCompiles)
{
  default_return<void>::value ();
  SUCCEED ();
}

TEST (LumexTypeTraitsTest,
      GivenStringType_WhenDefaultReturnValue_ThenReturnsEmptyString)
{
  EXPECT_EQ (default_return<std::string>::value (), std::string ());
}

TEST (LumexTypeTraitsTest,
      GivenBoolAndDouble_WhenDefaultReturnValue_ThenZeroInitialized)
{
  EXPECT_EQ (default_return<bool>::value (), false);
  EXPECT_DOUBLE_EQ (default_return<double>::value (), 0.0);
}

TEST (LumexTypeTraitsTest,
      GivenUserType_WhenDefaultReturnValue_ThenValueInitializes)
{
  EXPECT_EQ (default_return<DefaultUser>::value ().value, 4);
}

// --- is_optional / is_optional_v
// ----------------------------------------------------------------

TEST (LumexTypeTraitsTest, GivenLumexOptional_WhenIsOptional_ThenTrue)
{
  LUMEX_STATIC_ASSERT_MSG ((is_optional<optional<int>>::value),
                           "optional<int> must be recognized");
  EXPECT_TRUE ((is_optional_v<optional<int>>));
  EXPECT_TRUE ((is_optional_v<optional<int> const>));
  EXPECT_FALSE ((is_optional_v<optional<int> *>));
}

TEST (LumexTypeTraitsTest, GivenNonOptionalType_WhenIsOptional_ThenFalse)
{
  LUMEX_STATIC_ASSERT_MSG ((!is_optional<int>::value),
                           "int is not an optional");
  EXPECT_FALSE (is_optional_v<int>);
  EXPECT_FALSE (is_optional_v<int const>);
  EXPECT_FALSE ((is_optional_v<std::string>));
  EXPECT_FALSE ((is_optional_v<MyClass>));
}

#if __cplusplus >= 201703L
TEST (LumexTypeTraitsTest, GivenStdOptional_WhenIsOptional_ThenTrue)
{
  LUMEX_STATIC_ASSERT_MSG ((is_optional<std::optional<int>>::value),
                           "std::optional<int> must be recognized");
  EXPECT_TRUE (is_optional_v<std::optional<int>>);
  EXPECT_TRUE (is_optional_v<std::optional<int> const>);
  EXPECT_FALSE (is_optional_v<std::optional<int> *>);
}
#endif

// --- LUMEX_DEFINE_ENUM_TRAITS / EnumTraits
// ------------------------------------------------------

#if __cplusplus >= 202002L
// Must be invoked at global scope: EnumTraits<T> (see LumexTypeTraits.hpp) is
// declared in the global namespace, and [temp.expl.spec] requires explicit
// specializations to live in a namespace enclosing the primary template's
// namespace - so this cannot be nested in an anonymous namespace.
LUMEX_DEFINE_ENUM_TRAITS (LumexTypeTraitsTestColor, unsigned char, Red, Green,
                          Blue);

LUMEX_DEFINE_ENUM_TRAITS (LumexTypeTraitsTestSingle, int, Only);

TEST (LumexTypeTraitsTest,
      GivenReflectedEnum_WhenUsingEnumTraits_ThenValuesFirstLastSizeAreCorrect)
{
  using enum LumexTypeTraitsTestColor;

  EXPECT_EQ (EnumTraits<LumexTypeTraitsTestColor>::size, 3U);
  EXPECT_EQ (EnumTraits<LumexTypeTraitsTestColor>::first, Red);
  EXPECT_EQ (EnumTraits<LumexTypeTraitsTestColor>::last, Blue);
  EXPECT_EQ (EnumTraits<LumexTypeTraitsTestColor>::values[1], Green);
}

TEST (LumexTypeTraitsTest,
      GivenSingleEnumerator_WhenUsingEnumTraits_ThenFirstEqualsLast)
{
  using enum LumexTypeTraitsTestSingle;
  EXPECT_EQ (EnumTraits<LumexTypeTraitsTestSingle>::size, 1U);
  EXPECT_EQ (EnumTraits<LumexTypeTraitsTestSingle>::first, Only);
  EXPECT_EQ (EnumTraits<LumexTypeTraitsTestSingle>::last, Only);
  EXPECT_EQ (EnumTraits<LumexTypeTraitsTestSingle>::values[0], Only);
}

TEST (LumexTypeTraitsTest,
      GivenReflectedEnum_WhenIteratingValues_ThenVisitsEveryEnumerator)
{
  using enum LumexTypeTraitsTestColor;
  std::size_t count = 0;
  bool saw_red = false;
  bool saw_green = false;
  bool saw_blue = false;
  for (auto const value : EnumTraits<LumexTypeTraitsTestColor>::values)
    {
      ++count;
      if (value == Red)
        saw_red = true;
      if (value == Green)
        saw_green = true;
      if (value == Blue)
        saw_blue = true;
    }
  EXPECT_EQ (count, 3U);
  EXPECT_TRUE (saw_red);
  EXPECT_TRUE (saw_green);
  EXPECT_TRUE (saw_blue);
}
#endif

TEST (LumexTypeTraitsTest, GivenLambda_WhenIsCallable_ThenMatchesArity)
{
  auto add_one = [] (int x) { return x + 1; };
  EXPECT_TRUE ((is_callable<decltype (add_one), int>::value));
  EXPECT_TRUE ((is_invocable_v<decltype (add_one), int>));
  EXPECT_FALSE ((is_callable<decltype (add_one), std::string>::value));
  EXPECT_FALSE ((is_callable<decltype (add_one)>::value));
}

TEST (LumexTypeTraitsTest, GivenLambda_WhenInvokeResult_ThenInt)
{
  auto add_one = [] (int x) { return x + 1; };
  LUMEX_STATIC_ASSERT_MSG (
      (std::is_same<invoke_result_t<decltype (add_one), int>, int>::value),
      "lambda(int) returns int");
  SUCCEED ();
}

TEST (LumexTypeTraitsTest,
      GivenStdFunction_WhenIsCallable_ThenMatchesSignature)
{
  using fn_t = std::function<int (int)>;
  EXPECT_TRUE ((is_callable<fn_t, int>::value));
  EXPECT_FALSE ((is_callable<fn_t, std::string>::value));
  EXPECT_TRUE ((is_callable_signature<fn_t (int)>::value));
  EXPECT_FALSE ((is_callable_signature<fn_t (std::string)>::value));
}

TEST (LumexTypeTraitsTest, GivenPointerTypes_WhenCleanType_ThenStripsCvFromPtr)
{
  LUMEX_STATIC_ASSERT_MSG ((std::is_same<CleanType<int *>, int *>::value),
                           "int* stays int*");
  LUMEX_STATIC_ASSERT_MSG ((std::is_same<CleanType<int *const>, int *>::value),
                           "int* const becomes int*");
  LUMEX_STATIC_ASSERT_MSG (
      (std::is_same<CleanType<int *const &>, int *>::value),
      "int* const& becomes int*");
  LUMEX_STATIC_ASSERT_MSG (
      (std::is_same<CleanType<int const *>, int const *>::value),
      "pointee const is preserved");
  SUCCEED ();
}

TEST (LumexTypeTraitsTest, GivenUniquePtr_WhenSafeCastMove_ThenTransfers)
{
  std::unique_ptr<int> src (new int (5));
  int *const raw = src.get ();
  std::unique_ptr<int> dst = safe_cast<std::unique_ptr<int>> (std::move (src));
  EXPECT_EQ (dst.get (), raw);
  EXPECT_EQ (src.get (), static_cast<int *> (nullptr));
  EXPECT_EQ (*dst, 5);
}

TEST (LumexTypeTraitsTest, GivenVector_WhenSafeCastCopy_ThenCopiesElements)
{
  std::vector<int> src;
  src.push_back (1);
  src.push_back (2);
  std::vector<int> const dst = safe_cast<std::vector<int>> (src);
  ASSERT_EQ (dst.size (), 2u);
  EXPECT_EQ (dst[0], 1);
  EXPECT_EQ (dst[1], 2);
  EXPECT_EQ (src.size (), 2u);
}

TEST (LumexTypeTraitsTest,
      GivenUniquePtrType_WhenDefaultReturnValue_ThenReturnsEmpty)
{
  std::unique_ptr<int> const empty
      = default_return<std::unique_ptr<int>>::value ();
  EXPECT_EQ (empty.get (), static_cast<int *> (nullptr));
}

TEST (LumexTypeTraitsTest,
      GivenVectorType_WhenDefaultReturnValue_ThenReturnsEmpty)
{
  EXPECT_TRUE (default_return<std::vector<int>>::value ().empty ());
}

TEST (LumexTypeTraitsTest,
      GivenLumexOptionalCvVariants_WhenIsOptional_ThenStripsCvNotRef)
{
  EXPECT_TRUE ((is_optional_v<optional<int> volatile>));
  EXPECT_TRUE ((is_optional_v<optional<int> const volatile>));
  EXPECT_FALSE ((is_optional_v<optional<int> &>));
  EXPECT_FALSE ((is_optional_v<optional<int> const &>));
}

TEST (LumexTypeTraitsTest, GivenVoidTPack_WhenAppliedToManyTypes_ThenVoid)
{
  LUMEX_STATIC_ASSERT_MSG (
      (std::is_same<void_t<int, char, double, std::string, Functor>,
                    void>::value),
      "void_t of many types is void");
  SUCCEED ();
}

TEST (LumexTypeTraitsTest, GivenMatchingInvokeResult_WhenHasType_ThenTrue)
{
  LUMEX_STATIC_ASSERT_MSG ((has_type<invoke_result<Functor, int>>::value),
                           "Functor(int) has ::type");
  SUCCEED ();
}
