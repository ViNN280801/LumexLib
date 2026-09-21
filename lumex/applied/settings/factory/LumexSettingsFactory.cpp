#define LUMEX_IMPLEMENTATION
#include "LumexSettingsFactory.hpp"
#include "lumex/applied/settings/ini/LumexSettingsINI.hpp"
#if defined(LUMEX_SETTINGS_WITH_JSON)
#include "lumex/applied/settings/json/LumexSettingsJSON.hpp"
#endif
#if defined(LUMEX_SETTINGS_WITH_XML)
#include "lumex/applied/settings/xml/LumexSettingsXML.hpp"
#endif

namespace lumex
{
namespace applied
{
namespace settings
{
namespace factory
{
LUMEX_PUBLIC_API
std::unique_ptr<ILumexSettings>
LumexSettingsFactory::create (LumexSettingsExtensions ext)
{
  switch (ext)
    {
    case LumexSettingsExtensions::INI:
      return std::unique_ptr<LumexSettingsINI> (new LumexSettingsINI ());
    case LumexSettingsExtensions::XML:
#if defined(LUMEX_SETTINGS_WITH_XML)
      return std::unique_ptr<LumexSettingsXML> (new LumexSettingsXML ());
#else
      return nullptr;
#endif
    case LumexSettingsExtensions::JSON:
#if defined(LUMEX_SETTINGS_WITH_JSON)
      return std::unique_ptr<LumexSettingsJSON> (new LumexSettingsJSON ());
#else
      return nullptr;
#endif
    default:
      return nullptr;
    }
}
} // namespace factory
} // namespace settings
} // namespace applied
} // namespace lumex
