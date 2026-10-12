#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

#include "lumex/applied/settings/LumexSettings"

int
main ()
{
  std::cout << "=== XML settings via LumexXml ===\n\n";

#if !defined(LUMEX_SETTINGS_WITH_XML)
  std::cout << "LUMEX_SETTINGS_WITH_XML is off; factory XML is nullptr\n";
  std::unique_ptr<ILumexSettings> missing
      = LumexSettingsFactory::create (LumexSettingsExtensions::XML);
  std::cout << "factory XML=" << (missing ? "object" : "nullptr") << '\n';
  return missing == nullptr ? 0 : 1;
#else
  std::string const path ("lumex_settings_example.xml");
  std::remove (path.c_str ());

  std::cout << "--- 1. Missing file is not valid XML ---\n";
  std::cout << "is_xml_valid(missing)="
            << (LumexSettingsXML::is_xml_valid (path) ? "yes" : "no") << '\n';

  std::cout << "\n--- 2. Add / save / load / get / remove ---\n";
  LumexSettingsXML xml;
  xml.add ("pump", "flow", "1.0");
  xml.add ("detector", "wavelength", "254");
  bool const saved = xml.save (path);
  bool const loaded = xml.load (path);
  std::cout << "save=" << (saved ? "yes" : "no")
            << " load=" << (loaded ? "yes" : "no") << " valid="
            << (LumexSettingsXML::is_xml_valid (path) ? "yes" : "no")
            << " flow=" << xml.get ("pump", "flow") << '\n';
  xml.remove ("detector", "wavelength");
  xml.save (path);
  std::cout << "wavelength after remove=\""
            << xml.get ("detector", "wavelength") << "\"\n";

  std::cout << "\n--- 3. Logger-style root as one section ---\n";
  {
    std::ofstream file (path.c_str ());
    file << "<logger><LEVEL>DEBUG</LEVEL><HINT>1</HINT></logger>\n";
  }
  LumexSettingsXML logger_style;
  bool const logger_loaded = logger_style.load (path);
  std::cout << "logger load=" << (logger_loaded ? "yes" : "no")
            << " LEVEL=" << logger_style.get ("logger", "LEVEL") << '\n';

  std::cout << "\n--- 4. Factory ---\n";
  std::unique_ptr<ILumexSettings> via_factory
      = LumexSettingsFactory::create (LumexSettingsExtensions::XML);
  via_factory->add ("run", "operator", "lab");
  via_factory->save (path);
  via_factory->load (path);
  std::cout << "factory operator=" << via_factory->get ("run", "operator")
            << '\n';

  std::remove (path.c_str ());
  std::cout << "\n=== XML settings example finished ===\n";
  return (saved && loaded && logger_loaded && via_factory) ? 0 : 1;
#endif
}
