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
 * @file LumexTransportProtocol.hpp
 * @brief The dialogue layer of the transport over the protocol tables of
 * `core/protocol`: the dialogue vocabulary, `frame_link` and
 * `dialogue_engine`.
 * @details Built with the protocol and the framing modules
 * (`LUMEX_BUILD_PROTOCOL`, `LUMEX_BUILD_FRAMING`): the transport library then
 * defines `LUMEX_TRANSPORT_WITH_PROTOCOL` for itself and its consumers, links
 * <tt>%lumex::protocol</tt> and `LumexTransport` includes this header.
 */
#ifndef LUMEX_APPLIED_TRANSPORT_PROTOCOL_TRANSPORT_PROTOCOL_HPP
#define LUMEX_APPLIED_TRANSPORT_PROTOCOL_TRANSPORT_PROTOCOL_HPP

#include "lumex/applied/transport/protocol/LumexDialogueTypes.hpp"
#include "lumex/applied/transport/protocol/dialogue/LumexDialogue.hpp"
#include "lumex/applied/transport/protocol/dsl/LumexDialogueVocabulary.hpp"
#include "lumex/applied/transport/protocol/link/LumexFrameLink.hpp"

#endif // !LUMEX_APPLIED_TRANSPORT_PROTOCOL_TRANSPORT_PROTOCOL_HPP
