// The documents the index writes: Markdown for people and, when the library
// was built with LUMEX_PROTOCOL_WITH_JSON, JSON for programs. The protocol is
// the invented beacon of example_protocol_beacon.hpp.
#include <iostream>
#include <string>

#include "example_protocol_beacon.hpp"

#if defined(LUMEX_PROTOCOL_WITH_JSON)
#include "lumex/core/protocol/export/LumexProtocolExportJson.hpp"
#endif

namespace
{
// Inside a namespace of our own: at the global scope the alias index would
// clash with the POSIX function index ().
namespace index = lumex::core::protocol::index;
} // namespace

int
main ()
{
  std::cout << "=== lumex::protocol: export ===\n\n";
  index::index_result_t const &built
      = index::protocol_index<beacon::beacon_v1, beacon::lamp> ();
  if (!built)
    {
      std::cout << "no index: " << built.error ().message << '\n';
      return 1;
    }

  std::cout << "--- 1. Markdown ---\n";
  std::cout << index::export_markdown (*built) << '\n';

#if defined(LUMEX_PROTOCOL_WITH_JSON)
  std::cout << "--- 2. JSON ---\n";
  std::cout << index::export_json_text (*built, 2) << '\n';
#else
  std::cout << "--- 2. JSON ---\n"
            << "this build has no LUMEX_PROTOCOL_WITH_JSON\n";
#endif
  return 0;
}
