# Third-party notices

LumexLib is released under the MIT license in [LICENSE](LICENSE). It contains code derived from, or ships the sources of, the projects below. Their notices are reproduced or referenced here as their licenses require.

## pugixml

The XML module (`lumex/xml/`, the `lumex::xml` target) is derived from pugixml (https://github.com/zeux/pugixml): the DOM classes, the parser and writers, and the XPath implementation with its allocator. The XML code is compiled into the LumexXml library, so this notice goes with every copy of LumexLib.

The utility code that was moved out of the XML module into other modules is Lumex's own: all its functions (`byte_swap` and `is_little_endian` in `lumex/core/utility/bit/`, the transcoding functions and `to_utf8` / `to_wide` in the `unicode` module `lumex/core/unicode/`) are written for Lumex. Only the structure of its classes is taken from pugixml: the layout of the transcoding policy classes of the `unicode` module (counters, writers, decoders) and of `iterator_range` in `lumex/core/utility/ranges/`. The notice below is kept for that borrowed structure.

```
MIT License

Copyright (c) 2006-2026 Arseny Kapoulkine

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
```

## JSON for Modern C++ (nlohmann/json) 3.12.0

Vendored as a single header in `3rdparty/nlohmann/json.hpp` (MIT, Copyright (c) 2013-2025 Niels Lohmann); the notice is in the header itself. It is used by field reflection, the JSON logger configuration and `LumexSettingsJSON`, and is not installed with LumexLib: a consumer supplies its own copy.

## GoogleTest 1.12.1 and 1.18.0

Vendored sources in `3rdparty/googletest-1.12.1/` and `3rdparty/googletest-1.18.0/` (BSD 3-Clause, Copyright 2008, Google Inc.); the license text is in the `LICENSE` file of each directory. They build LumexLib's own tests only and are never installed.

## Hazard pointers (Folly, libc++ pull request 218218, libcds)

The hazard pointer module (`lumex/core/hazard_pointer/`, the `lumex::hazard_pointer` target) implements the interface of the C++26 `<hazard_pointer>` (P2530R3, P3428R4). Its engine follows the design of the hazard pointer implementation in Folly (https://github.com/facebook/folly, `folly/synchronization/Hazptr*`, Apache License 2.0, Copyright (c) Meta Platforms, Inc. and affiliates): the record and retired-list structure, the reclamation thresholds and the asymmetric fence protocol; and of the libc++ pull request llvm/llvm-project#218218 by Nikita Grivin (Apache License 2.0 with LLVM exceptions): the single immortal domain, the per-thread record cache and its thread-exit handling. The memory model argument is Maged Michael's. libcds (https://github.com/khizmax/libcds, Boost Software License 1.0, Copyright (c) 2006-2018 Maxim Khizhinsky) was a reference for the scan variants and the explicit-registration alternative. No source text of these projects is copied; the code is LumexLib's own, and nothing of those licenses applies to it. The asymmetric fence protocol is not implemented yet: the engine issues the portable sequentially consistent fences on both sides.

## Contract assertions (Boost.Assert and Boost.Contract)

The contracts module (`lumex/core/contracts/`, the `lumex::contracts` target) implements `contract_assert` of C++26 (P2900) as the macro `LUMEX_CONTRACT_ASSERT`. Its design takes ideas from Boost.Assert and Boost.Contract (https://www.boost.org/doc/libs/release/libs/assert/ and https://www.boost.org/doc/libs/release/libs/contract/, Boost Software License 1.0, Copyright (c) Peter Dimov, Beman Dawes, Ion Gaztanaga, Lorenzo Caminiti and the Boost contributors): the macro as a conditional expression that works in `if`/`else` without braces, the text of the condition and the file, function and line of the place as the report, configuration macros that switch the checks off or route a failure to a user handler, a handler installed at run time whose default prints a message and ends the program, and a source location class that converts from `std::source_location`. No source text of Boost is copied; the code is LumexLib's own, and nothing of that license applies to it.

## Lock-free atomic smart pointers (Williams, Anderson and Blelloch, Michael)

The lock-free engine of `atomic_shared_ptr` and `atomic_weak_ptr` (`lumex/core/atomic/smart_ptr/`, `atomic_shared_ptr_lock_free`) combines ideas from published work; no source text is copied, and nothing of any license applies to the code, which is LumexLib's own. Maged M. Michael's hazard pointers (2004) protect the heap box that holds the value. The announce-validate load of an atomic reference counted pointer, the one-hazard-slot-per-operation structure and the snapshot idea come from Daniel Anderson, Guy E. Blelloch and Yuanhao Wei, "Concurrent Deferred Reference Counting with Constant-Time Overhead" (PLDI 2021) and the repository https://github.com/DanielLiamAnderson/atomic_shared_ptr by Daniel Anderson; the split reference count that LumexLib's own libc++ implementation uses for its double-width method, and which this engine does not use, was introduced by Anthony Williams ("C++ Concurrency in Action", just::thread). The engine keeps the value in an immutable box published through one pointer, so that the user-visible type stays `std::shared_ptr`.

## Smart pointers (Anthony Williams, Folly)

The smart pointer module (`lumex/core/smart_ptr/`, the `lumex::smart_ptr` target) has its own control block, designed for a split-count atomic shared pointer. The idea of the split reference count, a strong count with a separate external counter that a reader of an atomic pointer pays back, is Anthony Williams' (the lock-free `atomic<shared_ptr>` proof of concept and the split reference count chapters of "C++ Concurrency in Action", Manning); the packing of the strong count and the external count into one 64-bit word follows the design of Folly's `folly/concurrency/AtomicSharedPtr.h` and `AtomicSharedPtr-detail.h` (Apache License 2.0, Copyright (c) Meta Platforms, Inc. and affiliates). The interface is that of the C++17 `std::shared_ptr` family, and the single allocation of `make_shared` and the aliasing constructor are the standard libraries' published behavior. The two corrections to those designs (a debt counter that is never turned into a strong reference, and the order "transfer, then drop") come from the model checking done for this library. No source text of these projects is copied; the code is LumexLib's own, and nothing of those licenses applies to it.
