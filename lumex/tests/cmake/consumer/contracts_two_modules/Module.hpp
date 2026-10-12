// The interface between the executable and the library of the fixture.
#ifndef LUMEX_TESTS_CONTRACTS_TWO_MODULES_HPP
#define LUMEX_TESTS_CONTRACTS_TWO_MODULES_HPP

#include "lumex/core/contracts/LumexContracts"

#if defined(_WIN32)
#if defined(CONTRACTS_FIXTURE_BUILDING)
#define CONTRACTS_FIXTURE_API __declspec (dllexport)
#else
#define CONTRACTS_FIXTURE_API __declspec (dllimport)
#endif
#else
#define CONTRACTS_FIXTURE_API __attribute__ ((visibility ("default")))
#endif

namespace fixture
{
// Violates one observed assertion inside the library.
CONTRACTS_FIXTURE_API void violate_in_module ();
// Installs the library's own handler; returns it for comparison.
CONTRACTS_FIXTURE_API lumex::core::contracts::violation_handler_type
install_module_handler ();
// How many violations the library's own handler saw.
CONTRACTS_FIXTURE_API int module_handler_calls ();
} // namespace fixture

#endif // !LUMEX_TESTS_CONTRACTS_TWO_MODULES_HPP
