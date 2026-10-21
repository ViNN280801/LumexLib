// The interface between the executable and the library of the fixture.
#ifndef LUMEX_TESTS_FRAMING_TWO_MODULES_HPP
#define LUMEX_TESTS_FRAMING_TWO_MODULES_HPP

#include <system_error>

#include "lumex/core/framing/LumexFraming"

#if defined(_WIN32)
#if defined(FRAMING_FIXTURE_BUILDING)
#define FRAMING_FIXTURE_API __declspec (dllexport)
#else
#define FRAMING_FIXTURE_API __declspec (dllimport)
#endif
#else
#define FRAMING_FIXTURE_API __attribute__ ((visibility ("default")))
#endif

namespace fixture
{
// An error code made inside the library from framing_errc::bad_checksum.
FRAMING_FIXTURE_API std::error_code checksum_error ();
// The address of the category as the library sees it.
FRAMING_FIXTURE_API std::error_category const *category_in_module ();
} // namespace fixture

#endif // !LUMEX_TESTS_FRAMING_TWO_MODULES_HPP
