# Boost.Context 1.88.0 (and the Boost headers it needs)

- **Upstream:** https://www.boost.org, release 1.88.0
- **Archive:** `https://archives.boost.io/release/1.88.0/source/boost_1_88_0.tar.gz`, SHA256 `3621533e820dcab1e8012afd583c0c73cf0f77694952b81352bf38c1488f9cb4` (matches the published `boost_1_88_0.tar.gz.json`, Boost commit `199ef13d6034c85232431130142159af3adfce22`)
- **Vendored:** 21.10.2026
- **License:** Boost Software License 1.0 (`LICENSE_1_0.txt`)

## Taken

The text-level include closure (`CLOSURE.txt`, 256 files) of:

- `boost/context/fiber.hpp`, `fixedsize_stack.hpp`, `protected_fixedsize_stack.hpp`, `stack_context.hpp`, `stack_traits.hpp`;
- `libs/context/src/fiber.cpp` (the activation record of the `ucontext` and `winfib` fibers);
- `libs/context/src/posix/stack_traits.cpp`, `libs/context/src/windows/stack_traits.cpp`.

The closure follows every `#include` of a Boost header whatever condition surrounds it, and every string literal naming a Boost header (Boost selects `boost/config/...` and the ABI headers through macros such as `BOOST_COMPILER_CONFIG` and `BOOST_ABI_PREFIX`), so it is a superset of what each backend needs. Most of it is Boost.Config and Boost.Predef.

## Left out

The assembly sources (`libs/context/src/asm/`), `fcontext.cpp`, `continuation.cpp`, `dummy.cpp`, `untested.cpp`, everything else of Boost. LumexLib builds only the `winfib` (Windows) and `ucontext` (POSIX) backends, without assembly (transport specification, decision O1).

## Local changes

None.

## Relation to other copies

Every closure header that also exists in PeakExpertWeb's vendored Boost 1.88 headers (`BusinessLogic/PeakExpertNoGUI/3rdparty/boost`) is identical to it (222 files, line endings normalized, 21.10.2026); the 23 Boost.Context headers are not part of that copy.

## Checked

`fiber.hpp` with `BOOST_USE_WINFIB`, `fiber.cpp` and `windows/stack_traits.cpp` compile against this tree alone with MSVC (C++14, `/W4`, no warning) and a two-switch fiber test runs (21.10.2026). The `ucontext` backend is checked on Linux in the phase 0 gate.
