#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <cstdio>

#include "lumex/applied/settings/LumexSettings"

int
main ()
{
  std::cout << "=== Workflow: repair a method INI with defaults ===\n\n";

  std::string const path ("lumex_settings_workflow.ini");
  std::remove (path.c_str ());

  std::shared_ptr<ILumexSettings> settings (
      LumexSettingsFactory::create (LumexSettingsExtensions::INI));
  LumexSettingsGuard guard (settings, path);

  bool const created = guard.ensureExistsWithDefaults ([&path, settings] () {
    settings->add ("method", "name", "isocratic");
    settings->add ("pump", "flow", "1.0");
    return settings->save (path);
  });

  std::vector<lumex_settings_key_spec_t> specs (1);
  specs[0].section = "detector";
  specs[0].key = "wavelength";
  specs[0].default_value = "254";
  specs[0].validate = nullptr;
  guard.ensureKeysWithDefaults (specs);
  settings->load (path);

  std::cout << "created=" << (created ? "yes" : "no")
            << " method=" << settings->get ("method", "name")
            << " wavelength=" << settings->get ("detector", "wavelength")
            << '\n';

  std::remove (path.c_str ());
  return 0;
}
