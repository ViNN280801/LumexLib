/*
 * Copyright (c) 2026 Vladislav Semykin <vladislav.semykin@gmail.com>
 *
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of
 * charge, to any person obtaining a copy
 * of this software and associated
 * documentation files (the "Software"), to deal
 * in the Software without
 * restriction, including without limitation the rights
 * to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the
 * Software, and to permit persons to whom the Software is
 * furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice
 * and this permission notice shall be included in
 * all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT
 * WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO
 * THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH
 * THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/**
 * @file LumexHazardPointerEngine.hpp
 * @brief The engine behind `hazard_pointer`: hazard slots, retired nodes, the
 * entry points compiled into `lumex::hazard_pointer`, the reader fence and the
 * contract checks.
 * @details One process-wide domain owns every hazard slot and every retired
 * node. A slot (`slot_t`) is a single atomic word that one thread writes and
 * every thread may read; a node (`node_t`) is the two-word record that a
 * retired object carries until its deleter runs. The reader side
 * (`hazard_pointer::try_protect`) is header-only and touches a slot with plain
 * atomic stores and one fence. Everything else is compiled: the slot pool,
 * the per-thread slot cache, the retired lists and the reclamation pass. Only
 * the free functions below are exported from the library, with signatures that
 * do not depend on the C++ standard.
 *
 * The protocol is the classic one of Maged M. Michael. A reader announces the
 * node it is about to use in its slot, issues a sequentially consistent
 * fence, and re-reads the source: if the source still holds the same pointer,
 * the node cannot be reclaimed. A reclamation pass takes the retired nodes,
 * issues a sequentially consistent fence, reads every slot and reclaims the
 * nodes that no slot names. The two fences are totally ordered, so either the
 * pass sees the announcement or the reader sees the removal that preceded the
 * retirement ([atomics.fences]).
 *
 * `reader_fence ()` is the only place that chooses the reader's fence. It is a
 * plain `seq_cst` thread fence today; an asymmetric variant (a compiler
 * barrier on the reader, a process-wide barrier in the pass) can replace both
 * fences later without touching any other code.
 *
 * `LUMEX_HAZARD_POINTER_DEBUG_ASSERT` checks the preconditions of the
 * reader's operations (a hazard pointer that owns no slot is not used). It is
 * active when `NDEBUG` is not defined, or when
 * `LUMEX_HAZARD_POINTER_DEBUG_CHECKS` is defined to 1, and compiled out
 * otherwise; `retire_node` checks its precondition (an object is retired at
 * most once) in every build with `LUMEX_ASSERT`.
 * `LUMEX_HAZARD_POINTER_TEST_POINT (id)` is empty unless
 * `LUMEX_HAZARD_POINTER_TEST_HOOKS` is defined; tests that define it build the
 * engine themselves and stall threads at named points to make a race
 * deterministic.
 */
#ifndef LUMEX_CORE_HAZARD_POINTER_ENGINE_HPP
#define LUMEX_CORE_HAZARD_POINTER_ENGINE_HPP

#include "lumex/LumexExport.hpp"

#include <atomic>
#include <cstddef>

#include "lumex/core/utility/assert/LumexAssert.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

#if !defined(LUMEX_HAZARD_POINTER_DEBUG_CHECKS)
#if !defined(NDEBUG)
#define LUMEX_HAZARD_POINTER_DEBUG_CHECKS 1
#else
#define LUMEX_HAZARD_POINTER_DEBUG_CHECKS 0
#endif
#endif

#if LUMEX_HAZARD_POINTER_DEBUG_CHECKS
#define LUMEX_HAZARD_POINTER_DEBUG_ASSERT(cond) LUMEX_ASSERT (cond)
#else
#define LUMEX_HAZARD_POINTER_DEBUG_ASSERT(cond) static_cast<void> (0)
#endif

#if defined(LUMEX_HAZARD_POINTER_TEST_HOOKS)
#define LUMEX_HAZARD_POINTER_TEST_POINT(id)                                   \
  ::lumex::core::hazard_pointer::engine::test_point (                         \
      ::lumex::core::hazard_pointer::engine::id)
#else
#define LUMEX_HAZARD_POINTER_TEST_POINT(id) static_cast<void> (0)
#endif

namespace lumex
{
namespace core
{
namespace hazard_pointer
{
namespace engine
{
/**
 * @brief One hazard slot: the address of the node a thread protects, or null.
 * @details The owning thread stores, every thread reads. A slot lives for the
 * whole process and is reused; the library hands out `slot_t *` and takes it
 * back with `release_slot`.
 */
struct slot_t
{
  std::atomic<void const *> value;
};

/**
 * @brief The record a retired object carries: the next node of the retired
 * list and the function that reclaims the object.
 * @details `next == this` marks a node that is not retired (a fresh object).
 * `reclaim` is set before `retire_node` and runs on whichever thread finds
 * the node unprotected; it must not throw.
 */
struct node_t
{
  node_t *next;
  void (*reclaim) (node_t *);
};

/**
 * @brief Counters of the domain, for tests and diagnostics.
 * @details All values are snapshots of relaxed counters. `records` is the
 * number of hazard slots ever created (they are never freed), `pooled_records`
 * the number that sit in the shared pool (not counting the per-thread caches),
 * `retired` and `reclaimed` the number of nodes ever retired and reclaimed,
 * and `passes` the number of reclamation passes that took at least one node.
 */
struct statistics_t
{
  std::size_t records;
  std::size_t pooled_records;
  std::size_t retired;
  std::size_t reclaimed;
  std::size_t passes;
};

/**
 * @brief Named places where a test may stall a thread
 * (`LUMEX_HAZARD_POINTER_TEST_HOOKS` builds only).
 */
enum test_point_id_t
{
  /// A reader stored its announcement and issued the fence, before it re-reads
  /// the source.
  reader_announced = 1,
  /// A reclamation pass took its nodes and issued the fence, before it reads
  /// the slots.
  pass_fenced = 2,
  /// A pass read every slot, before it reclaims.
  pass_scanned = 3,
  /// `retire_node` pushed a node.
  retire_pushed = 4,
  /// A thread-exit hook is about to hand the cache back.
  cache_evicting = 5,
  /// The reader's fence is about to be issued.
  reader_fenced = 6
};

#if defined(LUMEX_HAZARD_POINTER_TEST_HOOKS)
/**
 * @brief The handler type of `set_test_handler`.
 */
typedef void (*test_handler_t) (int);

/**
 * @brief Calls the installed handler with a test point id, if any.
 */
LUMEX_HAZARD_POINTER_API void test_point (int id) LUMEX_NOEXCEPT;

/**
 * @brief Installs (or with null removes) the handler of the test points.
 */
LUMEX_HAZARD_POINTER_API void
set_test_handler (test_handler_t handler) LUMEX_NOEXCEPT;

/**
 * @brief Makes the reclamation pass match nodes against the slots one by one
 * instead of through a hash set, as it does when the set cannot be allocated.
 */
LUMEX_HAZARD_POINTER_API void
set_test_linear_scan (bool enabled) LUMEX_NOEXCEPT;
#endif
/**
 * @brief Marks a node as not retired.
 */
inline void
init_node (node_t &node) LUMEX_NOEXCEPT
{
  node.next = &node;
  node.reclaim = nullptr;
}

/**
 * @brief The fence between a reader's announcement and its re-read of the
 * source.
 * @details Sequentially consistent today. This is the single place that
 * decides the reader's side of the protocol; the pass's matching fence is in
 * the engine source.
 */
inline void
reader_fence () LUMEX_NOEXCEPT
{
  LUMEX_HAZARD_POINTER_TEST_POINT (reader_fenced);
  std::atomic_thread_fence (std::memory_order_seq_cst);
}

/**
 * @brief Takes a hazard slot for the calling thread.
 * @return A slot whose value is null, owned by the caller until
 * `release_slot`.
 * @throws std::bad_alloc when a new block of slots cannot be allocated.
 * @details Takes it from the per-thread cache, then from the shared pool,
 * then from a new block; the cache hit is wait-free.
 */
LUMEX_HAZARD_POINTER_API slot_t *acquire_slot ();

/**
 * @brief Takes `count` hazard slots at once, all or none.
 * @param[out] out Receives `count` slots.
 * @param[in] count Number of slots.
 * @throws std::bad_alloc when a new block cannot be allocated; the slots
 * taken so far are returned to the pool and `out` is not meaningful.
 */
LUMEX_HAZARD_POINTER_API void acquire_slots (slot_t **out, std::size_t count);

/**
 * @brief Clears a slot (ending the protection) and gives it back.
 * @param[in] slot A slot from `acquire_slot`.
 * @note Safe to call from a thread-exit hook and during static destruction.
 */
LUMEX_HAZARD_POINTER_API void release_slot (slot_t *slot) LUMEX_NOEXCEPT;

/**
 * @brief Hands a retired object to the domain.
 * @param[in] node The node of an object whose `reclaim` is set and whose
 * `next` still points at itself.
 * @details The object is reclaimed once no slot names its node. The call may
 * run a reclamation pass on the calling thread, and then deleters run here.
 * Retiring a node twice aborts through `LUMEX_ASSERT`.
 */
LUMEX_HAZARD_POINTER_API void retire_node (node_t *node) LUMEX_NOEXCEPT;

/**
 * @brief Runs a full reclamation pass now and waits for a running one.
 * @details Every retired node that no slot protects is reclaimed before the
 * call returns, unless the call comes from a deleter of the pass itself (it
 * then returns at once). Meant for tests, leak checkers and orderly
 * shutdown.
 */
LUMEX_HAZARD_POINTER_API void clean_up () LUMEX_NOEXCEPT;

/**
 * @brief Reads the counters of the domain.
 */
LUMEX_HAZARD_POINTER_API statistics_t statistics () LUMEX_NOEXCEPT;

} // namespace engine
} // namespace hazard_pointer
} // namespace core
} // namespace lumex

#endif // !LUMEX_CORE_HAZARD_POINTER_ENGINE_HPP
