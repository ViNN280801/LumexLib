// Tests of the extension overloads of hazard_pointer::try_protect and
// protect: a callable source (so the validating reload may be seq_cst) and a
// filter that strips tag bits. The standard's hazard_pointer has no such
// overloads, so the tests name the module's own class.

#include <atomic>
#include <cstddef>
#include <cstdint>

#include <gtest/gtest.h>

#include "lumex/tests/core/hazard_pointer/LumexHazardPointerTestSupport.hpp"

namespace
{
using lumex_hp_test::counted_node;
using lumex_hp_test::counters_t;
namespace hp = lumex::core::hazard_pointer;

// A word that carries a node pointer in its high bits and a tag in bit 0.
struct tagged_word
{
  std::atomic<std::uintptr_t> word;

  tagged_word () : word (0) {}

  void
  store (counted_node *node, bool tag)
  {
    word.store (reinterpret_cast<std::uintptr_t> (node) | (tag ? 1u : 0u));
  }
};

struct strip_tag
{
  counted_node *
  operator() (std::uintptr_t value) const
  {
    return reinterpret_cast<counted_node *> (value & ~std::uintptr_t (1));
  }
};

struct load_seq_cst
{
  tagged_word const *source;

  std::uintptr_t
  operator() () const
  {
    return source->word.load (std::memory_order_seq_cst);
  }
};
} // namespace

TEST (
    LumexHazardPointerExtensionTest,
    GivenATaggedWord_WhenProtectingThroughAFilter_ThenTheUntaggedObjectIsProtected)
{
  counters_t counters;
  tagged_word shared;
  counted_node *node = new counted_node (counters, 9);
  shared.store (node, true);
  load_seq_cst load = { &shared };
  hp::hazard_pointer holder = hp::make_hazard_pointer ();
  counted_node *got = holder.protect (load, strip_tag ());
  EXPECT_EQ (got, node);
  shared.store (nullptr, false);
  node->retire ();
  hp::clean_up ();
  EXPECT_EQ (counters.deleted.load (), 0) << "protected through the filter";
  holder.reset_protection ();
  hp::clean_up ();
  EXPECT_EQ (counters.deleted.load (), 1);
}

TEST (LumexHazardPointerExtensionTest,
      GivenAChangedWord_WhenTryProtectingThroughAFilter_ThenFalseAndPtrUpdated)
{
  counters_t counters;
  tagged_word shared;
  counted_node *stale = new counted_node (counters, 1);
  counted_node *fresh = new counted_node (counters, 2);
  shared.store (fresh, false);
  load_seq_cst load = { &shared };
  hp::hazard_pointer holder = hp::make_hazard_pointer ();
  counted_node *ptr = stale;
  EXPECT_FALSE (holder.try_protect (ptr, load, strip_tag ()));
  EXPECT_EQ (ptr, fresh);
  stale->retire ();
  hp::clean_up ();
  EXPECT_EQ (counters.deleted.load (), 1) << "the stale object is free";
  shared.store (nullptr, false);
  fresh->retire ();
  hp::clean_up ();
  EXPECT_EQ (counters.deleted.load (), 2);
}

TEST (LumexHazardPointerExtensionTest,
      GivenOnlyATagChange_WhenTryProtecting_ThenTheFilteredPointerStillMatches)
{
  counters_t counters;
  tagged_word shared;
  counted_node *node = new counted_node (counters, 1);
  shared.store (node, false);
  load_seq_cst load = { &shared };
  hp::hazard_pointer holder = hp::make_hazard_pointer ();
  counted_node *ptr = node;
  shared.store (node, true);
  EXPECT_TRUE (holder.try_protect (ptr, load, strip_tag ()));
  EXPECT_EQ (ptr, node);
  shared.store (nullptr, false);
  node->retire ();
  holder.reset_protection ();
  hp::clean_up ();
  EXPECT_EQ (counters.deleted.load (), 1);
}
