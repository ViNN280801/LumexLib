// LumexStreamTraits.cxx14.tests.cpp
// The variable templates of the stream traits (C++14). The suites from C++14
// up compile this file together with LumexStreamTraits.cxx11.tests.cpp.
#include <string>

#include <gtest/gtest.h>

#include "lumex/core/utility/traits/LumexTypeTraits.hpp"

#include "lumex/tests/core/utility/LumexStreamTraitsTestTypes.hpp"

using lumex::core::utility::traits::stream::all_streamable_v;
using lumex::core::utility::traits::stream::is_streamable_v;
using lumex_stream_traits_test::custom_streamable_t;
using lumex_stream_traits_test::not_streamable_t;

TEST (LumexStreamTraitsSfinaeTest, GivenVariableTemplates_ThenMatchTraits)
{
  EXPECT_TRUE (is_streamable_v<int>);
  EXPECT_TRUE (is_streamable_v<std::string const &>);
  EXPECT_TRUE (is_streamable_v<custom_streamable_t &&>);
  EXPECT_FALSE (is_streamable_v<not_streamable_t const &>);
  EXPECT_TRUE ((all_streamable_v<>));
  EXPECT_TRUE ((all_streamable_v<int, std::string const &, double>));
  EXPECT_FALSE ((all_streamable_v<int, not_streamable_t>));
}
