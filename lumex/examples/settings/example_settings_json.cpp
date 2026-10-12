#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

#include "lumex/applied/settings/LumexSettings"

int
main ()
{
  std::cout << "=== JSON settings via nlohmann ===\n\n";

#if !defined(LUMEX_SETTINGS_WITH_JSON)
  std::cout << "LUMEX_SETTINGS_WITH_JSON is off; factory JSON is nullptr\n";
  std::unique_ptr<ILumexSettings> missing
      = LumexSettingsFactory::create (LumexSettingsExtensions::JSON);
  std::cout << "factory JSON=" << (missing ? "object" : "nullptr") << '\n';
  return missing == nullptr ? 0 : 1;
#else
  std::string const path ("lumex_settings_example.json");
  std::remove (path.c_str ());

  std::cout << "--- 1. Missing file is not valid JSON ---\n";
  std::cout << "is_json_valid(missing)="
            << (LumexSettingsJSON::is_json_valid (path) ? "yes" : "no")
            << '\n';

  std::cout << "\n--- 2. Add / save / load / get / remove ---\n";
  LumexSettingsJSON json;
  json.add ("pump", "flow", "1.0");
  json.add ("detector", "wavelength", "254");
  bool const saved = json.save (path);
  bool const loaded = json.load (path);
  std::cout << "save=" << (saved ? "yes" : "no")
            << " load=" << (loaded ? "yes" : "no") << " valid="
            << (LumexSettingsJSON::is_json_valid (path) ? "yes" : "no")
            << " flow=" << json.get ("pump", "flow") << '\n';
  json.remove ("detector", "wavelength");
  json.save (path);
  std::cout << "wavelength after remove=\""
            << json.get ("detector", "wavelength") << "\"\n";

  std::cout << "\n--- 3. Nested logger object as one section ---\n";
  {
    std::ofstream file (path.c_str ());
    file << "{\"logger\":{\"LEVEL\":\"DEBUG\",\"HINT\":1}}\n";
  }
  LumexSettingsJSON logger_style;
  bool const logger_loaded = logger_style.load (path);
  std::cout << "logger load=" << (logger_loaded ? "yes" : "no")
            << " LEVEL=" << logger_style.get ("logger", "LEVEL") << '\n';

  std::cout << "\n--- 4. Factory ---\n";
  std::unique_ptr<ILumexSettings> via_factory
      = LumexSettingsFactory::create (LumexSettingsExtensions::JSON);
  via_factory->add ("run", "operator", "lab");
  via_factory->save (path);
  via_factory->load (path);
  std::cout << "factory operator=" << via_factory->get ("run", "operator")
            << '\n';

  std::remove (path.c_str ());
  std::cout << "\n=== JSON settings example finished ===\n";
  return (saved && loaded && logger_loaded && via_factory) ? 0 : 1;
#endif
}
