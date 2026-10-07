#include <iostream>
#include <string>

#include "lumex/core/string_view/LumexStringView"

using namespace lumex::core::string_view::view;

int
main ()
{
  std::cout << "=== LumexStringView inspect / search / slice ===\n\n";

  std::string const text = "lumex/core/filesystem";
  lumex_string_view sv (text);

  std::cout << "--- 1. Size / data ---\n";
  std::cout << "view=\"" << std::string (sv.data (), sv.size ())
            << "\" size=" << sv.size ()
            << " empty=" << (sv.empty () ? "yes" : "no") << '\n';

  std::cout << "\n--- 2. starts_with / ends_with ---\n";
  std::cout << "starts lumex="
            << (sv.starts_with (lumex_string_view ("lumex")) ? "yes" : "no")
            << " starts '/'=" << (sv.starts_with ('/') ? "yes" : "no")
            << " ends filesystem="
            << (sv.ends_with (lumex_string_view ("filesystem")) ? "yes" : "no")
            << '\n';

  std::cout << "\n--- 3. find / rfind / compare ---\n";
  lumex_string_view::size_type const slash = sv.find ('/');
  lumex_string_view::size_type const last = sv.rfind ('/');
  std::cout << "find('/')=" << slash << " rfind('/')=" << last
            << " compare(lumex)=" << sv.compare (lumex_string_view ("lumex"))
            << '\n';

  std::cout << "\n--- 4. substr ---\n";
  lumex_string_view const leaf = sv.substr (last + 1U);
  std::cout << "leaf=\"" << std::string (leaf.data (), leaf.size ()) << "\"\n";

  std::cout << "\n=== StringView example finished ===\n";
  return 0;
}
