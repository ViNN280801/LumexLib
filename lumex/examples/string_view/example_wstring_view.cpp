#include <iostream>

#include "lumex/core/string_view/LumexStringView"

int
main ()
{
  lumex_wstring_view const name (L"methods/gradient.ini");
  lumex_wstring_view::size_type const slash = name.rfind (L'/');
  lumex_wstring_view const leaf = name.substr (slash + 1U);
  std::wcout << leaf << L' ' << leaf.size () << L'\n';
}
