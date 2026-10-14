// The engine fallback of the variants, made visible. A variant that names an
// engine the build may not have (split_count_engine, lock_based_lumex, ...)
// runs the lock-based engine instead where the engine does not exist
// (LumexAtomicTestSupport.hpp). This directory's other file tests names only
// and never reaches an engine, so the visibility test of the directory lives
// here; the directories smart_ptr/ and sync/ carry the same test in their
// own files.

#include <gtest/gtest.h>

#include "lumex/tests/core/atomic/LumexAtomicTestSupport.hpp"

TEST (LumexAtomicEngineFallbackTest,
      GivenAVariantOfAnEngine_WhenTheBuildLacksIt_ThenTheRunIsSkippedVisibly)
{
  LUMEX_ATOMIC_TEST_SKIP_IF_ENGINE_FELL_BACK ();
}
