// Tests of make_hazard_pointer_batch and clear_hazard_pointer_batch
// ([saferecl.hp.holder.nonmem], P3428R4): only empty elements are filled, a
// filled holder protects like any other, clearing ends every protection.

#include <atomic>
#include <cstddef>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/tests/core/hazard_pointer/LumexHazardPointerTestSupport.hpp"

namespace
{
using lumex_hp_test::counted_node;
using lumex_hp_test::counters_t;
namespace hp = lumex::core::hazard_pointer;
using lumex::core::span::view::span;
} // namespace

TEST (LumexHazardPointerBatchTest,
      GivenEmptyHolders_WhenMadeInABatch_ThenAllOwnASlot)
{
  hp::hazard_pointer holders[5];
  hp::make_hazard_pointer_batch (span<hp::hazard_pointer> (holders, 5));
  for (hp::hazard_pointer const &holder : holders)
    {
      EXPECT_FALSE (holder.empty ());
    }
}

TEST (LumexHazardPointerBatchTest,
      GivenAnEmptySpan_WhenMade_ThenNothingHappens)
{
  hp::make_hazard_pointer_batch (span<hp::hazard_pointer> ());
  hp::clear_hazard_pointer_batch (span<hp::hazard_pointer> ());
  SUCCEED ();
}

TEST (LumexHazardPointerBatchTest,
      GivenAMixOfHolders_WhenMadeInABatch_ThenOnlyTheEmptyOnesAreFilled)
{
  counters_t counters;
  counted_node *node = new counted_node (counters, 1);
  std::atomic<counted_node *> source (node);
  std::vector<hp::hazard_pointer> holders (4);
  holders[1] = hp::make_hazard_pointer ();
  ASSERT_EQ (holders[1].protect (source), node);
  hp::make_hazard_pointer_batch (
      span<hp::hazard_pointer> (holders.data (), holders.size ()));
  for (hp::hazard_pointer const &holder : holders)
    {
      EXPECT_FALSE (holder.empty ());
    }
  source.store (nullptr);
  node->retire ();
  hp::clean_up ();
  EXPECT_EQ (counters.deleted.load (), 0)
      << "the non-empty holder kept its protection";
  holders[1].reset_protection ();
  hp::clean_up ();
  EXPECT_EQ (counters.deleted.load (), 1);
}

TEST (LumexHazardPointerBatchTest,
      GivenBatchHolders_WhenProtecting_ThenEachProtectsItsOwnObject)
{
  counters_t counters;
  std::size_t const count = 70;
  std::vector<hp::hazard_pointer> holders (count);
  hp::make_hazard_pointer_batch (
      span<hp::hazard_pointer> (holders.data (), holders.size ()));
  std::vector<std::atomic<counted_node *>> sources (count);
  std::vector<counted_node *> nodes (count);
  for (std::size_t i = 0; i < count; ++i)
    {
      nodes[i] = new counted_node (counters, static_cast<int> (i));
      sources[i].store (nodes[i]);
      ASSERT_EQ (holders[i].protect (sources[i]), nodes[i]);
      sources[i].store (nullptr);
      nodes[i]->retire ();
    }
  hp::clean_up ();
  EXPECT_EQ (counters.deleted.load (), 0);
  hp::clear_hazard_pointer_batch (
      span<hp::hazard_pointer> (holders.data (), holders.size ()));
  for (hp::hazard_pointer const &holder : holders)
    {
      EXPECT_TRUE (holder.empty ());
    }
  hp::clean_up ();
  EXPECT_EQ (counters.deleted.load (), static_cast<int> (count));
}

TEST (LumexHazardPointerBatchTest,
      GivenClearedHolders_WhenClearedAgain_ThenStillEmpty)
{
  hp::hazard_pointer holders[3];
  hp::make_hazard_pointer_batch (span<hp::hazard_pointer> (holders, 3));
  hp::clear_hazard_pointer_batch (span<hp::hazard_pointer> (holders, 3));
  hp::clear_hazard_pointer_batch (span<hp::hazard_pointer> (holders, 3));
  for (hp::hazard_pointer const &holder : holders)
    {
      EXPECT_TRUE (holder.empty ());
    }
}

TEST (LumexHazardPointerBatchTest,
      GivenABatchLargerThanTheCache_WhenMade_ThenSlotsComeFromTheEngine)
{
  std::size_t const count = 1000;
  std::size_t const records_before = hp::engine::statistics ().records;
  std::vector<hp::hazard_pointer> holders (count);
  hp::make_hazard_pointer_batch (
      span<hp::hazard_pointer> (holders.data (), holders.size ()));
  EXPECT_GE (hp::engine::statistics ().records, records_before);
  hp::clear_hazard_pointer_batch (
      span<hp::hazard_pointer> (holders.data (), holders.size ()));
  // The slots are back: a second batch of the same size creates no new slot.
  std::size_t const records_after = hp::engine::statistics ().records;
  hp::make_hazard_pointer_batch (
      span<hp::hazard_pointer> (holders.data (), holders.size ()));
  EXPECT_EQ (hp::engine::statistics ().records, records_after);
}
