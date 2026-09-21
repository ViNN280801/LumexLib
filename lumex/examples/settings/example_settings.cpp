#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <cstdio>

#include "lumex/applied/settings/LumexSettings"

using namespace lumex::applied::settings::ini;

int
main ()
{
  std::cout << "=== INI settings, factory, guard ===\n\n";

  std::string const path ("lumex_settings_example.ini");
  std::remove (path.c_str ());

  std::cout << "--- 1. Missing file is not valid INI ---\n";
  std::cout << "is_ini_valid(missing)="
            << (LumexSettingsINI::is_ini_valid (path) ? "yes" : "no") << '\n';

  std::cout << "\n--- 2. Add / save / load / get / remove ---\n";
  LumexSettingsINI ini;
  ini.add ("pump", "flow", "1.0");
  ini.add ("detector", "wavelength", "254");
  bool const saved = ini.save (path);
  bool const loaded = ini.load (path);
  std::cout << "save=" << (saved ? "yes" : "no")
            << " load=" << (loaded ? "yes" : "no") << " valid="
            << (LumexSettingsINI::is_ini_valid (path) ? "yes" : "no")
            << " flow=" << ini.get ("pump", "flow") << '\n';
  ini.remove ("detector", "wavelength");
  ini.save (path);
  std::cout << "wavelength after remove=\""
            << ini.get ("detector", "wavelength") << "\"\n";

  std::cout << "\n--- 3. Factory ---\n";
  std::unique_ptr<ILumexSettings> via_factory
      = LumexSettingsFactory::create (LumexSettingsExtensions::INI);
  via_factory->add ("run", "operator", "lab");
  via_factory->save (path);
  via_factory->load (path);
  std::cout << "factory operator=" << via_factory->get ("run", "operator")
            << '\n';

  std::cout << "\n--- 4. Guard fills missing keys ---\n";
  std::shared_ptr<ILumexSettings> shared (via_factory.release ());
  LumexSettingsGuard guard (shared, path);
  std::vector<lumex_settings_key_spec_t> specs;
  lumex_settings_key_spec_t spec;
  spec.section = "pump";
  spec.key = "flow";
  spec.default_value = "0.5";
  spec.validate = nullptr;
  specs.push_back (spec);
  lumex_settings_key_spec_t missing;
  missing.section = "oven";
  missing.key = "temperature";
  missing.default_value = "40";
  missing.validate = nullptr;
  specs.push_back (missing);
  bool const filled = guard.ensureKeysWithDefaults (specs);
  shared->load (path);
  std::cout << "ensureKeys changed=" << (filled ? "yes" : "no")
            << " oven=" << shared->get ("oven", "temperature") << '\n';

#if defined(LUMEX_SETTINGS_WITH_XML)
  std::cout << "\n--- 5. XML factory (same interface) ---\n";
  std::string const xml_path ("lumex_settings_example.xml");
  std::remove (xml_path.c_str ());
  std::unique_ptr<ILumexSettings> xml_settings
      = LumexSettingsFactory::create (LumexSettingsExtensions::XML);
  xml_settings->add ("pump", "flow", "0.8");
  xml_settings->save (xml_path);
  xml_settings->load (xml_path);
  std::cout << "xml factory flow=" << xml_settings->get ("pump", "flow")
            << '\n';
  std::remove (xml_path.c_str ());
#endif

#if defined(LUMEX_SETTINGS_WITH_JSON)
  std::cout << "\n--- 6. JSON factory (same interface) ---\n";
  std::string const json_path ("lumex_settings_example.json");
  std::remove (json_path.c_str ());
  std::unique_ptr<ILumexSettings> json_settings
      = LumexSettingsFactory::create (LumexSettingsExtensions::JSON);
  json_settings->add ("pump", "flow", "0.8");
  json_settings->save (json_path);
  json_settings->load (json_path);
  std::cout << "json factory flow=" << json_settings->get ("pump", "flow")
            << '\n';
  std::remove (json_path.c_str ());
#endif

  std::remove (path.c_str ());
  std::cout << "\n=== Settings example finished ===\n";
  return 0;
}
