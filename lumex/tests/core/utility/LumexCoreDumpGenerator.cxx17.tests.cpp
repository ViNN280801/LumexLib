// LumexCoreDumpGenerator.cxx17.tests.cpp
// DumpFactory::getEstimatedSize against the CoreDumpGenerator::KB_* size
// constants. The constants are static constexpr data members without a
// definition at namespace scope, so only from C++17, where such a member is
// an inline variable, may a test bind them to a reference (EXPECT_EQ does);
// below C++17 the reference fails to link. The suites from C++17 up compile
// this file together with LumexCoreDumpGenerator.cxx11.tests.cpp.
#include <gtest/gtest.h>

#include "lumex/core/utility/dump/LumexCoreDumpGenerator.hpp"

using namespace lumex::core::utility::dump;

TEST (LumexCoreDumpGeneratorFactoryTest,
      GivenKnownTypes_WhenGetEstimatedSize_ThenMatchesCatalog)
{
  EXPECT_EQ (DumpFactory::getEstimatedSize (DumpType::MINI_DUMP_NORMAL),
             CoreDumpGenerator::KB_64);
  EXPECT_EQ (DumpFactory::getEstimatedSize (DumpType::KERNEL_SMALL_DUMP),
             CoreDumpGenerator::KB_64);
  EXPECT_EQ (DumpFactory::getEstimatedSize (DumpType::DEFAULT_AUTO), 0u);
  EXPECT_EQ (DumpFactory::getEstimatedSize (DumpType::CORE_DUMP_FULL), 0u);
}
