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
 * @file LumexHazardPointerBatch.hpp
 * @brief `make_hazard_pointer_batch` and `clear_hazard_pointer_batch`, which
 * fill and empty many holders at once ([saferecl.hp.holder.nonmem], P3428R4).
 * @details `make_hazard_pointer_batch (hps)` gives every empty element of
 * `hps` a hazard slot and leaves the non-empty ones alone. It either succeeds
 * for all of them or throws `std::bad_alloc` with no effect. The module takes
 * all slots from the engine in one call, which is cheaper than one
 * `make_hazard_pointer` per element. `clear_hazard_pointer_batch (hps)` ends
 * the protection of every non-empty element and empties it; it cannot fail.
 *
 * The span is the module's `lumex::core::span::view::span`, which converts
 * from `std::span` from C++20.
 */
#ifndef LUMEX_CORE_HAZARD_POINTER_HOLDER_HAZARD_POINTER_BATCH_HPP
#define LUMEX_CORE_HAZARD_POINTER_HOLDER_HAZARD_POINTER_BATCH_HPP

#include <cstddef>
#include <utility>
#include <vector>

#include "lumex/core/hazard_pointer/engine/LumexHazardPointerEngine.hpp"
#include "lumex/core/hazard_pointer/holder/LumexHazardPointerHolder.hpp"
#include "lumex/core/span/LumexSpan"
#include "lumex/core/utility/macros/LumexKeywords.hpp"

namespace lumex
{
namespace core
{
namespace hazard_pointer
{
/**
 * @brief Gives every empty element a hazard slot, all or none.
 * @param[in,out] hps The holders; non-empty ones are not touched.
 * @throws std::bad_alloc when slots cannot be allocated; no element changes.
 */
inline void
make_hazard_pointer_batch (span::view::span<hazard_pointer> hps)
{
  std::size_t needed = 0;
  for (std::size_t i = 0; i < hps.size (); ++i)
    {
      if (hps[i].empty ())
        {
          ++needed;
        }
    }
  if (needed == 0)
    {
      return;
    }
  std::vector<engine::slot_t *> slots (needed);
  engine::acquire_slots (slots.data (), needed);
  std::size_t next = 0;
  for (std::size_t i = 0; i < hps.size (); ++i)
    {
      if (hps[i].empty ())
        {
          detail::holder_access::slot (hps[i]) = slots[next];
          ++next;
        }
    }
}

/**
 * @brief Ends the protection of every non-empty element and empties it.
 * @param[in,out] hps The holders.
 */
inline void
clear_hazard_pointer_batch (span::view::span<hazard_pointer> hps)
    LUMEX_NOEXCEPT
{
  for (std::size_t i = 0; i < hps.size (); ++i)
    {
      hps[i] = hazard_pointer ();
    }
}
} // namespace hazard_pointer
} // namespace core
} // namespace lumex

#endif // !LUMEX_CORE_HAZARD_POINTER_HOLDER_HAZARD_POINTER_BATCH_HPP
