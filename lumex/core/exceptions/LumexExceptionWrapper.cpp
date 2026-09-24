#define LUMEX_IMPLEMENTATION
#include "lumex/core/utility/callback/LumexCallbackSlot.hpp"

#include "LumexExceptionWrapper.hpp"

namespace lumex
{
namespace core
{
namespace exceptions
{
namespace Wrapper
{
namespace
{
// Only this translation unit touches the slot, so the executable and the
// exceptions shared library see one reporter through the exported wrappers.
struct safe_call_reporter_tag_t;
using SafeCallReporterSlot = lumex::core::utility::callback::LumexCallbackSlot<
    safe_call_reporter_tag_t, void (char const *)>;
} // namespace

LUMEX_PUBLIC_API
void
set_safe_call_reporter (safe_call_report_fn reporter) LUMEX_NOEXCEPT
{
  SafeCallReporterSlot::set (reporter);
}

LUMEX_PUBLIC_API
safe_call_report_fn
get_safe_call_reporter () LUMEX_NOEXCEPT
{
  return SafeCallReporterSlot::get ();
}
} // namespace Wrapper
} // namespace exceptions
} // namespace core
} // namespace lumex
