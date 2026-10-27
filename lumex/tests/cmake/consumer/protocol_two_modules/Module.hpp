// The interface between the executable and the library of the fixture.
#ifndef LUMEX_TESTS_PROTOCOL_TWO_MODULES_HPP
#define LUMEX_TESTS_PROTOCOL_TWO_MODULES_HPP

#include <system_error>

#include "lumex/core/protocol/LumexProtocol"

#if defined(_WIN32)
#if defined(PROTOCOL_FIXTURE_BUILDING)
#define PROTOCOL_FIXTURE_API __declspec (dllexport)
#else
#define PROTOCOL_FIXTURE_API __declspec (dllimport)
#endif
#else
#define PROTOCOL_FIXTURE_API __attribute__ ((visibility ("default")))
#endif

namespace fixture
{
// An error code made inside the library from protocol_errc::invalid_value.
PROTOCOL_FIXTURE_API std::error_code invalid_value_error ();
// The address of the category as the library sees it.
PROTOCOL_FIXTURE_API std::error_category const *category_in_module ();
} // namespace fixture

#endif // !LUMEX_TESTS_PROTOCOL_TWO_MODULES_HPP
