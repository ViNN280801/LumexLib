// Enumerates every HID device through the vendored hidapi 0.15.0 and checks
// the version the library reports. hid_exit is deliberately not called: a
// process that also loads another library shares hidapi state with it.

#include <cstdio>

#include <hidapi.h>

#if HID_API_VERSION != HID_API_MAKE_VERSION(0, 15, 0)
#error "lumex::3rdparty_hidapi is not hidapi 0.15.0"
#endif

int
main ()
{
  struct hid_api_version const *version = hid_version ();
  if (version == nullptr || version->major != 0 || version->minor != 15)
    return 1;

  int count = 0;
  hid_device_info *devices = hid_enumerate (0, 0);
  for (hid_device_info *d = devices; d != nullptr; d = d->next)
    ++count;
  hid_free_enumeration (devices);

  std::printf ("hidapi %s, %d device(s)\n", hid_version_str (), count);
  return 0;
}
