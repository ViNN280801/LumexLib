// Compiled, not run, by cmake.consumer_exceptions_switch_compile_checks (see
// CMakeLists.txt). The headers of the four modules that follow the switch
// LUMEX_HAS_EXCEPTIONS, each with its throwing function in use, so that a
// module macro that the build does not accept stops the compilation.
#include <cstddef>

#include "lumex/core/expected/Expected"
#include "lumex/core/optional/opt/LumexOptional.hpp"
#include "lumex/core/span/LumexSpan"
#include "lumex/core/string_view/LumexStringView"

int
use_the_modules ()
{
  lumex::core::expected::result::expected<int, int> const e (1);
  lumex::core::optional::opt::optional<int> const o (2);
  int raw[2] = { 3, 4 };
  lumex::core::span::view::span<int> const s (raw, 2);
  lumex_string_view const text ("ab", 2);
  return e.value () + o.value () + s.at (1)
         + static_cast<int> (text.at (1) == 'b' ? 1 : 0) + LUMEX_HAS_EXCEPTIONS
         + LUMEX_EXPECTED_HAS_EXCEPTIONS + LUMEX_OPTIONAL_HAS_EXCEPTIONS
         + LUMEX_SPAN_HAS_EXCEPTIONS + LUMEX_STRING_VIEW_HAS_EXCEPTIONS;
}
