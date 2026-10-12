// The detection macros of the layer, the backend of the suite, the names the
// layer keeps out of the global namespace, and the headers on their own.
// The platform branches (AArch64, 32-bit, an unknown compiler, the disabling
// macro) are tested from the outside by the consumer fixture
// cmake.dwcas_compile_checks, which compiles the configuration header with
// simulated predefined macros.

#include <type_traits>

#include <gtest/gtest.h>

#include "lumex/core/atomic/dwcas/LumexDwcasWord.hpp"
#include "lumex/tests/core/atomic/dwcas/LumexDwcasTestSupport.hpp"

// The layer must not leak its names into the global namespace: declaring the
// same names there must still compile (a using-declaration at global scope
// would conflict).
struct dwcas_word
{
};

struct dwcas_value_t
{
};

inline bool
dwcas_supported ()
{
  return false;
}

inline void
require_dwcas ()
{
}

namespace
{
#if defined(LUMEX_TEST_DWCAS_BACKEND_EXPECTED)
int const k_expected_backend = LUMEX_TEST_DWCAS_BACKEND_EXPECTED;
#elif defined(__GNUC__) || defined(__clang__)
int const k_expected_backend = LUMEX_DWCAS_BACKEND_ASM;
#else
int const k_expected_backend = LUMEX_DWCAS_BACKEND_MSVC;
#endif
} // namespace

#if LUMEX_ATOMIC_HAS_DWCAS

namespace dwcas_ns = lumex::core::atomic::dwcas;

TEST (LumexDwcasConfigTest, GivenX8664_WhenDetected_ThenTheLayerExists)
{
#if defined(__x86_64__) || defined(_M_X64)
  EXPECT_EQ (LUMEX_ATOMIC_HAS_DWCAS, 1);
#endif
  EXPECT_EQ (LUMEX_DWCAS_BACKEND_NONE, 0);
  EXPECT_EQ (LUMEX_DWCAS_BACKEND_ASM, 1);
  EXPECT_EQ (LUMEX_DWCAS_BACKEND_MSVC, 2);
  EXPECT_EQ (LUMEX_DWCAS_BACKEND_BUILTIN, 3);
}

TEST (LumexDwcasConfigTest,
      GivenTheSuite_WhenTheBackendIsRead_ThenItIsTheExpectedOne)
{
  EXPECT_EQ (LUMEX_DWCAS_BACKEND, k_expected_backend);
}

TEST (
    LumexDwcasConfigTest,
    GivenTheBackend_WhenItsOperationsAreNamed_ThenTheyLiveInItsInlineNamespace)
{
  namespace detail = dwcas_ns::Detail;
#if LUMEX_DWCAS_BACKEND == LUMEX_DWCAS_BACKEND_ASM
  EXPECT_TRUE (
      (std::is_same<detail::backend_operations_t,
                    detail::backend_asm::backend_operations_t>::value));
#elif LUMEX_DWCAS_BACKEND == LUMEX_DWCAS_BACKEND_MSVC
  EXPECT_TRUE (
      (std::is_same<detail::backend_operations_t,
                    detail::backend_msvc::backend_operations_t>::value));
#elif LUMEX_DWCAS_BACKEND == LUMEX_DWCAS_BACKEND_BUILTIN
  EXPECT_TRUE (
      (std::is_same<detail::backend_operations_t,
                    detail::backend_builtin::backend_operations_t>::value));
#endif
}

TEST (LumexDwcasConfigTest,
      GivenGlobalNamesOfTheProgram_WhenNamed_ThenTheyAreNotTheLayersTypes)
{
  EXPECT_FALSE ((std::is_same<::dwcas_word, dwcas_ns::dwcas_word>::value));
  EXPECT_FALSE (
      (std::is_same<::dwcas_value_t, dwcas_ns::dwcas_value_t>::value));
  EXPECT_FALSE (::dwcas_supported ());
  ::require_dwcas ();
  EXPECT_TRUE (dwcas_ns::dwcas_supported ());
}

TEST (LumexDwcasConfigTest,
      GivenTheDisablingMacro_WhenTheSuiteIsBuilt_ThenItIsNotDefined)
{
#if defined(LUMEX_ATOMIC_DISABLE_DWCAS)
  FAIL () << "the suites are built with the layer";
#else
  SUCCEED ();
#endif
}

#else

TEST (LumexDwcasConfigTest,
      GivenTargetWithoutTheLayer_WhenDetected_ThenTheMacrosSayNo)
{
  EXPECT_EQ (LUMEX_ATOMIC_HAS_DWCAS, 0);
  EXPECT_EQ (LUMEX_DWCAS_BACKEND, LUMEX_DWCAS_BACKEND_NONE);
  EXPECT_EQ (k_expected_backend >= 0, true);
}

#endif // LUMEX_ATOMIC_HAS_DWCAS
