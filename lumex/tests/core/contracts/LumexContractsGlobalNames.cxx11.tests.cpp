// The module declares its names only in lumex::core::contracts (and the
// LUMEX_CONTRACT* / LUMEX_CONTRACTS_* macros): a program with global classes
// and functions of the same names keeps compiling. These declarations come
// before the module header, so the translation unit stops compiling if the
// header declares any of the names at global scope.

#include <cstdint>

#include <gtest/gtest.h>

struct source_location
{
  int global_marker;
};

struct contract_violation
{
  int global_marker;
};

enum class assertion_kind
{
  global_marker
};

enum class evaluation_semantic
{
  global_marker
};

enum class detection_mode
{
  global_marker
};

inline int
set_violation_handler ()
{
  return 11;
}

inline int
get_violation_handler ()
{
  return 12;
}

inline int
invoke_violation_handler ()
{
  return 13;
}

inline int
invoke_default_violation_handler ()
{
  return 14;
}

struct scoped_violation_handler
{
  int global_marker;
};

inline int
to_string (int)
{
  return 15;
}

inline int
is_terminating (int)
{
  return 16;
}

namespace detail
{
struct observe_t
{
  int global_marker;
};
} // namespace detail

#include "lumex/core/contracts/LumexContracts"

namespace
{
TEST (ContractsGlobalNames, GlobalDeclarationsKeepTheirMeaning)
{
  EXPECT_EQ (source_location{ 1 }.global_marker, 1);
  EXPECT_EQ (contract_violation{ 2 }.global_marker, 2);
  EXPECT_EQ (set_violation_handler (), 11);
  EXPECT_EQ (get_violation_handler (), 12);
  EXPECT_EQ (invoke_violation_handler (), 13);
  EXPECT_EQ (invoke_default_violation_handler (), 14);
  EXPECT_EQ (to_string (0), 15);
  EXPECT_EQ (is_terminating (0), 16);
  EXPECT_EQ (scoped_violation_handler{ 3 }.global_marker, 3);
  EXPECT_EQ (detail::observe_t{ 4 }.global_marker, 4);
}

TEST (ContractsGlobalNames, TheMacroStillWorksNextToThem)
{
  LUMEX_CONTRACT_ASSERT (set_violation_handler () == 11);
  SUCCEED ();
}
} // namespace
