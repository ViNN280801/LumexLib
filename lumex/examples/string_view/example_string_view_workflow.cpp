#include <iostream>
#include <string>

#include "lumex/core/string_view/LumexStringView"

using namespace lumex::core::string_view::view;

int
main ()
{
  std::cout << "=== Workflow: classify a method path without copying ===\n\n";

  char const *raw = "methods/isocratic.ini";
  lumex_string_view path (raw);
  bool const ini = path.ends_with (lumex_string_view (".ini"));
  lumex_string_view::size_type const slash = path.rfind ('/');
  lumex_string_view const name
      = (slash == lumex_string_view::npos) ? path : path.substr (slash + 1U);
  std::cout << "ini=" << (ini ? "yes" : "no") << " name=\""
            << std::string (name.data (), name.size ()) << "\"\n";
  return 0;
}
