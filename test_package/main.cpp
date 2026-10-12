#include <iostream>
#include <string>

#include "lumex/core/fmt/LumexFormat"
#include "lumex/core/string/LumexString"
#include "lumex/core/unicode/LumexUnicode"

#include "lumex/xml/LumexXml"

int
main ()
{
  using lumex::core::fmt::format;
  using lumex::core::string::utility::stringify;

  std::string const text = format ("{} {}", "conan", 1);
  if (text != "conan 1")
    {
      std::cerr << "format: '" << text << "'\n";
      return 1;
    }

  std::string const joined = stringify (1);
  if (joined != "1")
    {
      std::cerr << "stringify: '" << joined << "'\n";
      return 2;
    }

  if (lumex::core::unicode::convert::to_utf8 (
          lumex::core::unicode::convert::to_wide ("\xC3\xA9"))
      != "\xC3\xA9")
    {
      std::cerr << "unicode round trip failed\n";
      return 4;
    }

  lumex::xml::document::XmlDocument document;
  if (!document.load_string ("<root/>"))
    {
      std::cerr << "xml load_string failed\n";
      return 3;
    }

  std::cout << text << "\nstringify " << joined << "\nxml document ok\n";
  return 0;
}
