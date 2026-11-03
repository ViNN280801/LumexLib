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
 * @file LumexProtocolScenario.hpp
 * @brief The executor side of `LUMEX_PROTOCOL_SCENARIO` (the description is in
 * `core/protocol/dsl/LumexProtocolScenario.hpp`):
 * `async_run_scenario<Scenario> (engine, token)` runs a scenario on a
 * `dialogue_engine`.
 * @details The token gets `void (std::error_code, std::size_t step)`: on
 * success an empty error and the number of steps, otherwise the error and the
 * index of the step that failed. See `dialogue_engine::async_run_scenario` for
 * how the steps run. A row of the description is found in the index of the
 * engine by its identifier; a row the index does not have fails its step with
 * `not_supported`.
 */
#ifndef LUMEX_APPLIED_TRANSPORT_PROTOCOL_DSL_SCENARIO_HPP
#define LUMEX_APPLIED_TRANSPORT_PROTOCOL_DSL_SCENARIO_HPP

#include <utility>

#include "lumex/applied/transport/protocol/LumexDialogueTypes.hpp"
#include "lumex/applied/transport/protocol/dialogue/LumexDialogue.hpp"
#include "lumex/applied/transport/protocol/dialogue/LumexDialogueFeatures.hpp"
#include "lumex/core/protocol/dsl/LumexProtocolScenario.hpp"

namespace lumex
{
namespace applied
{
namespace transport
{
namespace protocol
{
/**
 * @brief Runs `Scenario` (a type of `LUMEX_PROTOCOL_SCENARIO`) on `engine`;
 * the token gets `void (std::error_code, std::size_t step)`.
 */
template <typename Scenario, typename Token>
auto
async_run_scenario (dialogue_engine &engine, Token &&token)
    -> decltype (engine.async_run_scenario (
        Scenario::make (),
        dialogue::features_of<typename Scenario::protocol_type> (),
        std::forward<Token> (token)))
{
  return engine.async_run_scenario (
      Scenario::make (),
      dialogue::features_of<typename Scenario::protocol_type> (),
      std::forward<Token> (token));
}
} // namespace protocol
} // namespace transport
} // namespace applied
} // namespace lumex

#endif // !LUMEX_APPLIED_TRANSPORT_PROTOCOL_DSL_SCENARIO_HPP
