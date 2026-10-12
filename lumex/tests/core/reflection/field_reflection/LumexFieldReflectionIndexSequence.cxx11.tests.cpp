// LumexFieldReflectionIndexSequence.cxx11.tests.cpp
//
// Field reflection counts the fields of an aggregate with an index sequence
// and takes it from utility (lumex/core/utility/sequence): the detail names
// are the shared ones, the class of utility in every standard, not a copy of
// its own and not std::index_sequence. Built when
// LUMEX_WITH_FIELD_REFLECTION is ON, like the rest of the directory.
#if defined(LUMEX_WITH_FIELD_REFLECTION)

#include <cstddef>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#include "lumex/core/reflection/LumexReflection"
#include "lumex/core/utility/sequence/LumexIndexSequence.hpp"

namespace detail = lumex::core::reflection::field_reflection::detail;
namespace sequence = lumex::core::utility::sequence;

TEST (LumexFieldReflectionIndexSequenceTest,
      GivenDetailNames_WhenCompared_ThenTheSharedIndexSequence)
{
  static_assert (std::is_same<detail::index_sequence<0, 1, 2>,
                              sequence::index_sequence<0, 1, 2>>::value,
                 "index_sequence is the one of utility");
  static_assert (std::is_same<detail::make_index_sequence<0>::type,
                              sequence::index_sequence<>>::value,
                 "make_index_sequence<0> is the empty list");
  static_assert (std::is_same<detail::make_index_sequence<4>::type,
                              sequence::make_index_sequence<4>>::value,
                 "make_index_sequence<N>::type is the one of utility");
  static_assert (std::is_same<detail::make_index_sequence<4>::type,
                              sequence::index_sequence<0, 1, 2, 3>>::value,
                 "the list is 0 .. N - 1");
#if __cplusplus >= 201402L
  static_assert (!std::is_same<detail::make_index_sequence<3>::type,
                               std::make_index_sequence<3>>::value,
                 "from C++14 the list is still the one of utility");
  static_assert (std::is_convertible<detail::make_index_sequence<3>::type,
                                     std::make_index_sequence<3>>::value,
                 "and converts to the standard one of the same list");
#endif
  SUCCEED ();
}

#endif // LUMEX_WITH_FIELD_REFLECTION
