# LumexContracts: `contract_assert` from C++11 {#lumex_contracts}

`lumex::core::contracts` (target `lumex::contracts`, a compiled library whose macros are header-only) provides `LUMEX_CONTRACT_ASSERT (predicate);`, the contract assertion of C++26 (`contract_assert`, P2900), on every standard from C++11, with an installable violation handler and the types that describe a violation (`contract_violation`, `source_location`, `assertion_kind`, `evaluation_semantic`, `detection_mode`). The classes are the module's own on every standard and toolchain (no aliases of the `std` ones, which no library ships yet); their API shape is the one of `<contracts>` of the C++26 draft ([support.contract]).

```cpp
#include "lumex/core/contracts/LumexContracts"

namespace contracts = lumex::core::contracts;

int divide (int numerator, int denominator)
{
  LUMEX_CONTRACT_ASSERT (denominator != 0);   // C++26: contract_assert (denominator != 0);
  return numerator / denominator;
}

void log_it (contracts::contract_violation const &violation)
{
  // violation.comment () is "denominator != 0"; violation.location () names the place
}

// contracts::set_violation_handler (&log_it);
```

`LUMEX_ASSERT` of the utility module is a different, older facility and is not touched: it is always active, has no semantics and one fixed handler. A contract assertion states a property of the program that must hold; what happens when it does not is a decision of the build, not of the line.

## Evaluation semantics

Four semantics follow [basic.contract.eval]; the fifth hands the decision to the compiler.

| Semantic | Predicate evaluated | On a false predicate |
| --- | --- | --- |
| `ignore` | never (still compiled: names and types are checked) | nothing |
| `observe` | once | the handler is called, then the program continues |
| `enforce` (default) | once | the handler is called, then `std::abort ()`; a handler that throws lets its exception out instead |
| `quick_enforce` | once | the program stops at once with a trap instruction (`__builtin_trap`, `__fastfail` on MSVC); no handler is called |
| `p2900` | the compiler's choice | the compiler's own `contract_assert` where `__cpp_contracts` is 202502L or more, otherwise `enforce` |

The semantic is chosen per build by the CMake option `LUMEX_CONTRACTS_SEMANTIC` (`IGNORE`, `OBSERVE`, `ENFORCE`, `QUICK_ENFORCE`, `P2900`; default `ENFORCE`), which `lumex::contracts` hands to every translation unit that links it (`LUMEX_CONTRACTS_BUILD_SEMANTIC`, also from an installed package). A translation unit may override it: define `LUMEX_CONTRACTS_SEMANTIC` to one of the words `ignore`, `observe`, `enforce`, `quick_enforce`, `p2900` before the first include of the module (`#define LUMEX_CONTRACTS_SEMANTIC observe`, or `-DLUMEX_CONTRACTS_SEMANTIC=observe`); any other word is an `#error`. Precedence: the macro of the translation unit, then the build, then `enforce`. `LUMEX_CONTRACTS_EFFECTIVE_SEMANTIC` is the number in force (1 ignore, 2 observe, 3 enforce, 4 quick_enforce, 5 the compiler's own keyword); `LUMEX_CONTRACT_ASSERT_IGNORE`, `_OBSERVE`, `_ENFORCE`, `_QUICK_ENFORCE` and `_P2900` fix the semantic of one assertion whatever the choice above.

`NDEBUG` has no effect: a contract assertion is not `assert`. To drop the checks of a release build choose `ignore` for it. The default is `enforce` because [basic.contract.eval] recommends it.

## The real `contract_assert`

`__cpp_contracts` of 202502L (P2900) or more means the compiler has the keyword. No installed toolchain here has it, so the native path is tested with a mock (`contract_assert` defined as a macro in a test translation unit). `__cpp_contracts` of 201906L is the withdrawn contracts of the C++20 draft (GCC `-fcontracts`, `[[assert: ...]]`), which have no `contract_assert`; it does not count. The native keyword is used only for the semantic `p2900`: with any other semantic the emulation of this module runs, so the library handler is the one that is called. With the keyword, the compiler's own flags and `handle_contract_violation` decide, and the handler of this module is not consulted. `contract_violation` has an explicit constructor from any object with the accessors of `std::contracts::contract_violation`, so a handler of the compiler can forward into the same code.

## Macro hygiene

- The predicate is evaluated exactly once with `observe`, `enforce` and `quick_enforce`, and zero times with `ignore`; never twice. It is contextually converted to `bool` (`static_cast<bool>`), so a type with an explicit conversion works and a type without one does not compile, also with `ignore`.
- The macro is variadic: a comma in a template argument list needs no parentheses (`LUMEX_CONTRACT_ASSERT (std::is_same<A, B>::value)`), the same idea as `LUMEX_STATIC_ASSERT_MSG`. The `comment ()` of a violation is the text as written, before macro expansion of the predicate.
- Outside C++26 the macro is an expression of type `void`: it works in `if (...) ...; else ...` without braces and in a `constexpr` function body from C++11 (`return (LUMEX_CONTRACT_ASSERT (v >= 0), v * v);`). A holding assertion is a constant expression; a failing one in constant evaluation does not compile. Write it as a statement (`LUMEX_CONTRACT_ASSERT (x);`), as C++26 requires.
- An exception that leaves the predicate propagates as any exception would. [basic.contract.eval] calls it a violation (detection mode `evaluation_exception`); defining `LUMEX_CONTRACTS_CATCH_EXCEPTIONS` to 1 before the first include does that, with a lambda in every expansion (so no use in `constexpr` functions before C++17); the handler is then called from inside the `catch` of the original exception and can rethrow it with `throw;`.
- The violation path is not `noexcept`: the handler may throw. In a `noexcept` function a throwing handler ends the program, as [basic.contract.eval] says.
- Location: file, function and line (and the column where the compiler has `__builtin_COLUMN`, for example Clang) of the assertion; the function name is `__PRETTY_FUNCTION__` / `__FUNCSIG__`.

## The violation handler

A handler is a plain function pointer, `void (*) (contract_violation const &)`, kept in a process-wide `lumex_callback_slot` inside the compiled library; only free functions are exported (`set_violation_handler`, `get_violation_handler`, `invoke_violation_handler`, `invoke_default_violation_handler`, `LUMEX_CONTRACTS_API`), so an executable and all shared libraries it loads share one handler on every platform, Windows included. `set_violation_handler` returns the previous handler (`nullptr` selects the default one); `scoped_violation_handler` restores it on scope exit. Both are thread-safe (an atomic pointer); the handler itself must be safe to call from any thread. A handler may throw (unlike `lumex_callback_slot::invoke_or`, the library never catches it). A violation reported from inside a handler of the same thread goes to the default handler, so a faulty handler cannot recurse.

The default handler writes one line to standard error (`file:line:column: function: contract violation: predicate [kind: assert, semantic: observe, detection: predicate_false]`) and returns, as [basic.contract.handler] recommends; `enforce` then aborts, `observe` goes on. It is not rate-limited.

## Differences from P2900

- No `pre` and `post`: only the assertion statement. `assertion_kind::pre` and `post` exist for completeness and for violations converted from a compiler.
- `contract_violation` is a copyable value with a public constructor (the macros build it); the standard class is created by the implementation only.
- The handler is installed at run time (`set_violation_handler`); in the standard `handle_contract_violation` is a replaceable global function chosen at link time.
- The predicate is evaluated once with a checking semantic ([basic.contract.eval-5] leaves it unspecified), and never repeated ([basic.contract.eval-18] allows repetition).
- Exceptions from the predicate are violations only on request (`LUMEX_CONTRACTS_CATCH_EXCEPTIONS`).
- `source_location` is the module's own class (accessors of `std::source_location`, constructible from four values, comparable); a `std::source_location` converts into it implicitly (C++20), the other direction does not exist.

## What was taken from Boost

Boost.Assert (`BOOST_ASSERT`, `BOOST_ASSERT_MSG`, `BOOST_VERIFY`, `BOOST_DISABLE_ASSERTS`, `BOOST_ENABLE_ASSERT_HANDLER`, `BOOST_ENABLE_ASSERT_DEBUG_HANDLER`, `boost::assertion_failed` / `assertion_failed_msg`, `boost::source_location`) and Boost.Contract (`BOOST_CONTRACT_ASSERT`, `boost::contract::set_*_failure`, `BOOST_CONTRACT_NO_*`) were read as design references (Boost Software License 1.0; ideas only, no code; see `THIRD-PARTY-NOTICES.md`).

| Idea in Boost | Here |
| --- | --- |
| `BOOST_ASSERT (expr)` as a conditional expression (`cond ? void : fail`), usable in `if`/`else` without braces | the same shape for the emulated semantics |
| The failure carries the stringized expression, the function, the file and the line | `contract_violation::comment ()` and `source_location` |
| `BOOST_DISABLE_ASSERTS`, `BOOST_CONTRACT_NO_ALL`: the check is switched off at compile time | the semantic `ignore` (the predicate is still compiled, as in the standard) |
| `BOOST_ENABLE_ASSERT_HANDLER`: the failure goes to a user function defined at link time (`assertion_failed`) | a handler installed at run time, as Boost.Contract does |
| `BOOST_ENABLE_ASSERT_DEBUG_HANDLER`, `NDEBUG` selects the variant | not taken: `NDEBUG` does not change a contract assertion |
| `set_pre_failure`, `set_post_failure`, ... (a handler per kind, with locked and unlocked variants; default: print and terminate) | one handler (there is one kind); an atomic pointer instead of a lock; the default prints and returns, the semantic terminates |
| A handler may throw (`throw_on_failure`) | the same, and it propagates |
| `BOOST_VERIFY` (the expression is always evaluated) | not taken: it contradicts `ignore` |
| `BOOST_CONTRACT_ASSERT_AUDIT` / `_AXIOM` (levels of cost) | not taken; a possible later extension |
| `boost::source_location` constructible from `std::source_location` | `source_location` converts from `std::source_location` |
| `BOOST_CONTRACT_NO_PRECONDITIONS` ... (a switch per kind) | one macro per assertion (`LUMEX_CONTRACT_ASSERT_IGNORE` ...) and one choice per translation unit |

## Where it lives

- `assert/LumexContractsAssert.hpp`: the macros and the choice of the semantic.
- `handler/LumexContractsHandler.hpp` and `.cpp`: the handler (the one compiled source).
- `violation/LumexContractsViolation.hpp`: the enumerations and `contract_violation`.
- `location/LumexContractsSourceLocation.hpp`: `source_location`.

The library is compiled as C++11 and its exported signatures do not depend on the standard. Tests run at C++11, 14, 17 and 20 (`lumex/tests/core/contracts/`); the examples are `lumex/examples/contracts/`.
