#include <algorithm>
#include <array>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "lumex/applied/serial/enumeration/LumexSerialPortEnumeration.hpp"
#include "lumex/applied/serial/port/LumexSerialPort.hpp"
#include "lumex/applied/serial/resolver/LumexPortProcessResolver.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"
#include "lumex/core/utility/os/LumexCheckOS.hpp"

#if LUMEX_OS_WINDOWS
#include <Windows.h>

#include <setupapi.h>

#ifndef DIREG_DEV
#define DIREG_DEV 0x00000001
#endif
#ifndef DICS_FLAG_GLOBAL
#define DICS_FLAG_GLOBAL 0x00000001
#endif

struct dev_info_guard_t
{
  HDEVINFO handle;
  explicit dev_info_guard_t (HDEVINFO value) LUMEX_NOEXCEPT : handle (value) {}
  ~dev_info_guard_t ()
  {
    if (handle != INVALID_HANDLE_VALUE)
      ::SetupDiDestroyDeviceInfoList (handle);
  }
  dev_info_guard_t (dev_info_guard_t const &) = delete;
  dev_info_guard_t &operator= (dev_info_guard_t const &) = delete;
};
#else
#include <cerrno>
#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace lumex
{
namespace applied
{
namespace serial
{
namespace enumeration
{
using lumex::applied::serial::port::resolve_serial_port_path;
using lumex::applied::serial::resolver::port_process_resolver;
using namespace lumex::applied::serial::port;

namespace
{
bool
starts_with (std::string const &value, char const *prefix)
{
  std::size_t const prefix_len = std::strlen (prefix);
  return value.size () >= prefix_len
         && value.compare (0, prefix_len, prefix) == 0;
}

#if LUMEX_OS_WINDOWS
std::string
convert_wide_to_utf8 (std::wstring const &wide) LUMEX_NOEXCEPT
{
  if (wide.empty ())
    return std::string ();
  int const needed = ::WideCharToMultiByte (CP_UTF8, 0, wide.c_str (),
                                            static_cast<int> (wide.size ()),
                                            nullptr, 0, nullptr, nullptr);
  if (needed <= 0)
    return std::string ();
  std::string utf8 (static_cast<std::size_t> (needed), '\0');
  int const written = ::WideCharToMultiByte (
      CP_UTF8, 0, wide.c_str (), static_cast<int> (wide.size ()), &utf8[0],
      needed, nullptr, nullptr);
  if (written <= 0)
    return std::string ();
  return utf8;
}

std::string
get_current_process_exe_name () LUMEX_NOEXCEPT
{
  std::array<wchar_t, 512> buf{};
  DWORD const len = ::GetModuleFileNameW (nullptr, buf.data (),
                                          static_cast<DWORD> (buf.size ()));
  if (len == 0 || len >= buf.size ())
    return std::string ();
  std::wstring path (buf.data (), static_cast<std::size_t> (len));
  std::size_t pos = path.rfind (L'\\');
  if (pos == std::wstring::npos)
    pos = path.rfind (L'/');
  std::wstring base = (pos != std::wstring::npos && pos + 1 < path.size ())
                          ? path.substr (pos + 1)
                          : path;
  return convert_wide_to_utf8 (base);
}

std::string
path_for_open (std::string const &port_name)
{
  return resolve_serial_port_path (port_name,
                                   Constants::KSERIAL_PORT_CHANNEL_TYPE);
}

int
com_number (std::string const &str)
{
  int const k_radix = 10;
  int const k_max_com_num = 255;
  if (str.size () < 4 || (str[0] != 'C' && str[0] != 'c')
      || (str[1] != 'O' && str[1] != 'o') || (str[2] != 'M' && str[2] != 'm'))
    return -1;
  int value = 0;
  for (std::size_t idx = 3;
       idx < str.size () && str[idx] >= '0' && str[idx] <= '9'; ++idx)
    {
      int const digit = str[idx] - '0';
      if (value > (k_max_com_num - digit) / k_radix)
        return -1;
      value = (value * k_radix) + digit;
    }
  return (value > k_max_com_num) ? -1 : value;
}
#else
bool
is_safe_tty_name (std::string const &name) LUMEX_NOEXCEPT
{
  if (name.empty ())
    return false;
  if (name.find ('/') != std::string::npos)
    return false;
  if (name.find ("..") != std::string::npos)
    return false;
  return true;
}

void
read_linux_device_attributes (std::string const &tty_name,
                              serial_port_info_t &info)
{
  struct local
  {
    static std::string
    read_file (std::string const &path)
    {
      std::ifstream stream (path.c_str ());
      std::string line;
      if (std::getline (stream, line))
        {
          while (!line.empty ()
                 && (line.back () == '\r' || line.back () == '\n'))
            line.pop_back ();
          return line;
        }
      return std::string ();
    }

    static std::string
    read_link (std::string const &path)
    {
      std::array<char, 256> buf{};
      ssize_t const n
          = ::readlink (path.c_str (), buf.data (), buf.size () - 1);
      if (n <= 0)
        return std::string ();
      buf[static_cast<std::size_t> (n)] = '\0';
      std::string text (buf.data ());
      std::size_t const last = text.rfind ('/');
      return (last != std::string::npos && last + 1 < text.size ())
                 ? text.substr (last + 1)
                 : text;
    }
  };

  std::string const up
      = std::string ("/sys/class/tty/") + tty_name + "/device/..";
  info.vid = local::read_file (up + "/idVendor");
  info.pid = local::read_file (up + "/idProduct");
  info.product = local::read_file (up + "/product");
  info.manufacturer = local::read_file (up + "/manufacturer");
  info.serial_number = local::read_file (up + "/serial");
  info.revision = local::read_file (up + "/bcdDevice");
  info.driver = local::read_link (up + "/driver");

  std::string const busnum = local::read_file (up + "/busnum");
  std::string const devnum = local::read_file (up + "/devnum");
  if (!info.vid.empty () || !info.pid.empty ())
    info.hardware_id = "USB " + info.vid + ":" + info.pid;
  if (!busnum.empty ())
    info.bus_info = "Bus:" + busnum;
  if (!devnum.empty ())
    {
      if (!info.bus_info.empty ())
        info.bus_info += " ";
      info.bus_info += "Dev:" + devnum;
    }

  std::string &details = info.device_details;
  if (!info.hardware_id.empty ())
    details += "HWID:" + info.hardware_id;
  if (!info.revision.empty ())
    details += (details.empty () ? "" : " | ") + std::string ("Rev:")
               + info.revision;
  if (!info.manufacturer.empty ())
    details += (details.empty () ? "" : " | ") + std::string ("Mfg:")
               + info.manufacturer;
  if (!info.product.empty ())
    details += (details.empty () ? "" : " | ") + std::string ("Product:")
               + info.product;
  if (!info.serial_number.empty ())
    details += (details.empty () ? "" : " | ") + std::string ("SN:")
               + info.serial_number;
  if (!busnum.empty ())
    details += (details.empty () ? "" : " | ") + std::string ("Bus:") + busnum;
  if (!devnum.empty ())
    details += (details.empty () ? "" : " | ") + std::string ("Dev:") + devnum;
  if (!info.driver.empty ())
    details += (details.empty () ? "" : " | ") + std::string ("Driver:")
               + info.driver;
}
#endif
} // namespace

char const *
serial_port_state_to_string (serial_port_state state) LUMEX_NOEXCEPT
{
  switch (state)
    {
    case serial_port_state::available:
      return "Available";
    case serial_port_state::busy:
      return "Busy";
    case serial_port_state::free:
      return "Free";
    default:
      return "Unknown";
    }
}

bool
is_bluetooth_enumerated_port (std::string const &port_name) LUMEX_NOEXCEPT
{
#if LUMEX_OS_WINDOWS
  DWORD const k_max_class_guids = 8;
  DWORD const k_friendly_size = 256;

  std::string short_name = port_name;
  if (starts_with (short_name, "\\\\.\\")
      || starts_with (short_name, "\\\\?\\"))
    short_name = short_name.substr (4);
  if (short_name.empty ())
    return false;

  try
    {
      std::array<GUID, 8> port_guids{};
      DWORD port_guids_size = 0;
      if (::SetupDiClassGuidsFromNameW (
              L"Ports", port_guids.data (),
              static_cast<DWORD> (port_guids.size ()), &port_guids_size)
          == 0)
        port_guids_size = 0;

      for (DWORD guid_index = 0; guid_index < port_guids_size; ++guid_index)
        {
          GUID const &guid = port_guids[static_cast<std::size_t> (guid_index)];
          HDEVINFO const dev_info = ::SetupDiGetClassDevsW (
              &guid, nullptr, nullptr, DIGCF_PRESENT);
          if (dev_info == INVALID_HANDLE_VALUE)
            continue;
          dev_info_guard_t const guard (dev_info);

          SP_DEVINFO_DATA dev_info_data{};
          dev_info_data.cbSize = sizeof (SP_DEVINFO_DATA);

          for (DWORD i = 0;
               ::SetupDiEnumDeviceInfo (dev_info, i, &dev_info_data) != 0; ++i)
            {
              std::string port;
              HKEY dev_key = ::SetupDiOpenDevRegKey (dev_info, &dev_info_data,
                                                     DICS_FLAG_GLOBAL, 0,
                                                     DIREG_DEV, KEY_READ);
              if (dev_key != INVALID_HANDLE_VALUE)
                {
                  std::array<wchar_t, 256> port_name_buf{};
                  DWORD port_name_sz = sizeof (port_name_buf);
                  DWORD port_name_type = 0;
                  if (::RegQueryValueExW (
                          dev_key, L"PortName", nullptr, &port_name_type,
                          reinterpret_cast<LPBYTE> (port_name_buf.data ()),
                          &port_name_sz)
                          == ERROR_SUCCESS
                      && port_name_type == REG_SZ && port_name_sz > 0)
                    {
                      port = convert_wide_to_utf8 (
                          std::wstring (port_name_buf.data ()));
                    }
                  ::RegCloseKey (dev_key);
                }

              if (port.empty ())
                {
                  std::array<wchar_t, 256> friendly_name{};
                  DWORD data_type = 0;
                  DWORD size = sizeof (friendly_name);
                  if (::SetupDiGetDeviceRegistryPropertyW (
                          dev_info, &dev_info_data, SPDRP_FRIENDLYNAME,
                          &data_type,
                          reinterpret_cast<PBYTE> (friendly_name.data ()),
                          size, &size)
                      != 0)
                    {
                      std::wstring wstr (friendly_name.data ());
                      std::size_t const pos = wstr.find (L"(COM");
                      if (pos != std::wstring::npos)
                        {
                          std::size_t const end = wstr.find (L')', pos);
                          if (end != std::wstring::npos)
                            port = convert_wide_to_utf8 (
                                wstr.substr (pos + 1, end - pos - 1));
                        }
                    }
                }

              if (port != short_name)
                continue;

              std::array<wchar_t, 256> enumerator_buf{};
              DWORD enumerator_type = 0;
              DWORD enumerator_size = sizeof (enumerator_buf);
              if (::SetupDiGetDeviceRegistryPropertyW (
                      dev_info, &dev_info_data, SPDRP_ENUMERATOR_NAME,
                      &enumerator_type,
                      reinterpret_cast<PBYTE> (enumerator_buf.data ()),
                      enumerator_size, &enumerator_size)
                  == 0)
                return false;

              std::string const enumerator_name = convert_wide_to_utf8 (
                  std::wstring (enumerator_buf.data ()));
              return starts_with (enumerator_name, "BTH");
            }
        }
    }
  catch (...)
    {
      return false;
    }

  static_cast<void> (k_max_class_guids);
  static_cast<void> (k_friendly_size);
  return false;
#else
  static_cast<void> (port_name);
  return false;
#endif
}

std::vector<serial_port_info_t>
enumerate_serial_ports_detailed (std::string const &connected_port,
                                 bool need_to_filter_bluetooth)
{
  std::vector<serial_port_info_t> result;

#if LUMEX_OS_WINDOWS
  DWORD const k_max_class_guids = 8;
  DWORD const k_port_name_size = 32;
  DWORD const k_friendly_size = 256;
  DWORD const k_details_buf_size = 512;
  int const k_fallback_max_com = 255;
  std::size_t const k_com_prefix_len = 3;

  std::set<std::string> unique_ports;
  std::map<std::string, serial_port_info_t> port_info_map;

  std::array<GUID, 8> port_guids{};
  DWORD port_guids_size = 0;
  if (::SetupDiClassGuidsFromNameW (L"Ports", port_guids.data (),
                                    static_cast<DWORD> (port_guids.size ()),
                                    &port_guids_size)
      == 0)
    port_guids_size = 0;

  std::array<GUID, 8> modem_guids{};
  DWORD modem_guids_size = 0;
  if (::SetupDiClassGuidsFromNameW (L"Modem", modem_guids.data (),
                                    static_cast<DWORD> (modem_guids.size ()),
                                    &modem_guids_size)
      == 0)
    modem_guids_size = 0;

  bool const setup_api_available = (port_guids_size + modem_guids_size) > 0;

  for (DWORD guid_index = 0; guid_index < port_guids_size + modem_guids_size;
       ++guid_index)
    {
      GUID const &guid
          = (guid_index < port_guids_size)
                ? port_guids[static_cast<std::size_t> (guid_index)]
                : modem_guids[static_cast<std::size_t> (guid_index
                                                        - port_guids_size)];

      HDEVINFO const dev_info
          = ::SetupDiGetClassDevsW (&guid, nullptr, nullptr, DIGCF_PRESENT);
      if (dev_info == INVALID_HANDLE_VALUE)
        continue;
      dev_info_guard_t const guard (dev_info);

      SP_DEVINFO_DATA dev_info_data{};
      dev_info_data.cbSize = sizeof (SP_DEVINFO_DATA);

      for (DWORD i = 0;
           ::SetupDiEnumDeviceInfo (dev_info, i, &dev_info_data) != 0; ++i)
        {
          std::string port;
          HKEY dev_key = ::SetupDiOpenDevRegKey (dev_info, &dev_info_data,
                                                 DICS_FLAG_GLOBAL, 0,
                                                 DIREG_DEV, KEY_READ);
          if (dev_key != INVALID_HANDLE_VALUE)
            {
              std::array<wchar_t, 32> port_name_buf{};
              DWORD port_name_sz = sizeof (port_name_buf);
              DWORD port_name_type = 0;
              if (::RegQueryValueExW (
                      dev_key, L"PortName", nullptr, &port_name_type,
                      reinterpret_cast<LPBYTE> (port_name_buf.data ()),
                      &port_name_sz)
                      == ERROR_SUCCESS
                  && port_name_type == REG_SZ && port_name_sz > 0)
                {
                  port = convert_wide_to_utf8 (
                      std::wstring (port_name_buf.data ()));
                }
              if (port.empty ())
                {
                  HKEY params_key = nullptr;
                  if (::RegOpenKeyExW (dev_key, L"Device Parameters", 0,
                                       KEY_READ, &params_key)
                      == ERROR_SUCCESS)
                    {
                      port_name_sz = sizeof (port_name_buf);
                      port_name_type = 0;
                      if (::RegQueryValueExW (
                              params_key, L"PortName", nullptr,
                              &port_name_type,
                              reinterpret_cast<LPBYTE> (port_name_buf.data ()),
                              &port_name_sz)
                              == ERROR_SUCCESS
                          && port_name_type == REG_SZ && port_name_sz > 0)
                        {
                          port = convert_wide_to_utf8 (
                              std::wstring (port_name_buf.data ()));
                        }
                      ::RegCloseKey (params_key);
                    }
                }
              ::RegCloseKey (dev_key);
            }

          if (port.empty ())
            {
              std::array<wchar_t, 256> friendly_name{};
              DWORD data_type = 0;
              DWORD size = sizeof (friendly_name);
              if (::SetupDiGetDeviceRegistryPropertyW (
                      dev_info, &dev_info_data, SPDRP_FRIENDLYNAME, &data_type,
                      reinterpret_cast<PBYTE> (friendly_name.data ()), size,
                      &size)
                  != 0)
                {
                  std::wstring wstr (friendly_name.data ());
                  std::size_t const pos = wstr.find (L"(COM");
                  if (pos != std::wstring::npos)
                    {
                      std::size_t const end = wstr.find (L')', pos);
                      if (end != std::wstring::npos)
                        port = convert_wide_to_utf8 (
                            wstr.substr (pos + 1, end - pos - 1));
                    }
                }
            }

          if (!port.empty () && port.size () >= k_com_prefix_len
              && (port[0] == 'L' || port[0] == 'l')
              && (port[1] == 'P' || port[1] == 'p')
              && (port[2] == 'T' || port[2] == 't'))
            port.clear ();

          if (!port.empty () && need_to_filter_bluetooth)
            {
              std::array<wchar_t, 256> enumerator_buf{};
              DWORD enumerator_type = 0;
              DWORD enumerator_size = sizeof (enumerator_buf);
              if (::SetupDiGetDeviceRegistryPropertyW (
                      dev_info, &dev_info_data, SPDRP_ENUMERATOR_NAME,
                      &enumerator_type,
                      reinterpret_cast<PBYTE> (enumerator_buf.data ()),
                      enumerator_size, &enumerator_size)
                  != 0)
                {
                  std::string const enumerator_name = convert_wide_to_utf8 (
                      std::wstring (enumerator_buf.data ()));
                  if (starts_with (enumerator_name, "BTH"))
                    port.clear ();
                }
            }

          if (port.empty ())
            continue;

          unique_ports.insert (port);
          serial_port_info_t &info = port_info_map[port];
          info.path = port;

          std::array<wchar_t, 256> friendly_buf{};
          DWORD data_type = 0;
          DWORD size = sizeof (friendly_buf);
          if (::SetupDiGetDeviceRegistryPropertyW (
                  dev_info, &dev_info_data, SPDRP_FRIENDLYNAME, &data_type,
                  reinterpret_cast<PBYTE> (friendly_buf.data ()), size, &size)
              != 0)
            {
              info.friendly_name
                  = convert_wide_to_utf8 (std::wstring (friendly_buf.data ()));
            }
          else
            {
              size = sizeof (friendly_buf);
              if (::SetupDiGetDeviceRegistryPropertyW (
                      dev_info, &dev_info_data, SPDRP_DEVICEDESC, &data_type,
                      reinterpret_cast<PBYTE> (friendly_buf.data ()), size,
                      &size)
                  != 0)
                {
                  info.friendly_name = convert_wide_to_utf8 (
                      std::wstring (friendly_buf.data ()));
                }
            }

          std::array<wchar_t, 512> buf{};
          DWORD buf_size = sizeof (buf);
          if (::SetupDiGetDeviceRegistryPropertyW (
                  dev_info, &dev_info_data, SPDRP_HARDWAREID, &data_type,
                  reinterpret_cast<PBYTE> (buf.data ()), buf_size, &buf_size)
              != 0)
            info.hardware_id
                = convert_wide_to_utf8 (std::wstring (buf.data ()));

          buf_size = sizeof (buf);
          if (::SetupDiGetDeviceRegistryPropertyW (
                  dev_info, &dev_info_data, SPDRP_MFG, &data_type,
                  reinterpret_cast<PBYTE> (buf.data ()), buf_size, &buf_size)
              != 0)
            info.manufacturer
                = convert_wide_to_utf8 (std::wstring (buf.data ()));

          buf_size = sizeof (buf);
          if (::SetupDiGetDeviceRegistryPropertyW (
                  dev_info, &dev_info_data, SPDRP_DEVICEDESC, &data_type,
                  reinterpret_cast<PBYTE> (buf.data ()), buf_size, &buf_size)
              != 0)
            {
              std::string desc
                  = convert_wide_to_utf8 (std::wstring (buf.data ()));
              if (desc != info.friendly_name)
                info.product = desc;
            }

          buf_size = sizeof (buf);
          if (::SetupDiGetDeviceRegistryPropertyW (
                  dev_info, &dev_info_data, SPDRP_SERVICE, &data_type,
                  reinterpret_cast<PBYTE> (buf.data ()), buf_size, &buf_size)
              != 0)
            info.driver = convert_wide_to_utf8 (std::wstring (buf.data ()));

          buf_size = sizeof (buf);
          if (::SetupDiGetDeviceRegistryPropertyW (
                  dev_info, &dev_info_data, SPDRP_ENUMERATOR_NAME, &data_type,
                  reinterpret_cast<PBYTE> (buf.data ()), buf_size, &buf_size)
              != 0)
            info.bus_info = convert_wide_to_utf8 (std::wstring (buf.data ()));

          buf_size = sizeof (buf);
          if (::SetupDiGetDeviceRegistryPropertyW (
                  dev_info, &dev_info_data, SPDRP_LOCATION_INFORMATION,
                  &data_type, reinterpret_cast<PBYTE> (buf.data ()), buf_size,
                  &buf_size)
              != 0)
            {
              std::string loc
                  = convert_wide_to_utf8 (std::wstring (buf.data ()));
              if (!loc.empty ())
                {
                  if (!info.bus_info.empty ())
                    info.bus_info += " | ";
                  info.bus_info += loc;
                }
            }

          buf_size = sizeof (buf);
          if (::SetupDiGetDeviceRegistryPropertyW (
                  dev_info, &dev_info_data, SPDRP_CLASS, &data_type,
                  reinterpret_cast<PBYTE> (buf.data ()), buf_size, &buf_size)
              != 0)
            info.device_class
                = convert_wide_to_utf8 (std::wstring (buf.data ()));

          std::array<wchar_t, 260> inst_id_buf{};
          if (::SetupDiGetDeviceInstanceIdW (
                  dev_info, &dev_info_data, inst_id_buf.data (),
                  static_cast<DWORD> (inst_id_buf.size ()), nullptr)
              != 0)
            info.instance_id
                = convert_wide_to_utf8 (std::wstring (inst_id_buf.data ()));

          std::string &details = info.device_details;
          if (!info.hardware_id.empty ())
            details = "HWID:" + info.hardware_id;
          if (!info.manufacturer.empty ())
            details += (details.empty () ? "" : " | ") + std::string ("Mfg:")
                       + info.manufacturer;
          if (!info.product.empty ())
            details += (details.empty () ? "" : " | ") + std::string ("Desc:")
                       + info.product;
          if (!info.driver.empty ())
            details += (details.empty () ? "" : " | ")
                       + std::string ("Driver:") + info.driver;
          if (!info.bus_info.empty ())
            details += (details.empty () ? "" : " | ") + std::string ("Bus:")
                       + info.bus_info;
          if (!info.device_class.empty ())
            details += (details.empty () ? "" : " | ") + std::string ("Class:")
                       + info.device_class;
          if (!info.instance_id.empty ())
            details += (details.empty () ? "" : " | ")
                       + std::string ("Instance:") + info.instance_id;
        }
    }

  static_cast<void> (k_max_class_guids);
  static_cast<void> (k_port_name_size);
  static_cast<void> (k_friendly_size);
  static_cast<void> (k_details_buf_size);

  char const *const k_busy_port_hint
      = "Busy by another process. Use Task Manager or handle.exe "
        "(Sysinternals) to identify.";

  for (std::set<std::string>::const_iterator it = unique_ports.begin ();
       it != unique_ports.end (); ++it)
    {
      std::string const &port = *it;
      serial_port_info_t &info = port_info_map[port];
      // Bluetooth SPP/LE virtual COM ports stay in the list when the filter
      // is off, but CreateFileA on a paired-and-disconnected device blocks
      // in the radio stack. Classify from SetupAPI presence only.
      if (starts_with (info.bus_info, "BTH"))
        {
          info.state = serial_port_state::available;
          continue;
        }
      std::string const open_path = path_for_open (port);
      HANDLE handle
          = ::CreateFileA (open_path.c_str (), GENERIC_READ | GENERIC_WRITE, 0,
                           nullptr, OPEN_EXISTING, 0, nullptr);
      if (handle != INVALID_HANDLE_VALUE)
        {
          ::CloseHandle (handle);
          info.state = serial_port_state::available;
        }
      else
        {
          DWORD const err = ::GetLastError ();
          info.state
              = (err == ERROR_ACCESS_DENIED || err == ERROR_SHARING_VIOLATION)
                    ? serial_port_state::busy
                    : serial_port_state::free;
          if (err != ERROR_FILE_NOT_FOUND)
            info.system_error = port_process_resolver::format_system_error (
                static_cast<int> (err));
          if (info.state == serial_port_state::busy)
            {
              if (!connected_port.empty () && port == connected_port)
                {
                  std::string holder
                      = "PID="
                        + std::to_string (static_cast<unsigned long> (
                            ::GetCurrentProcessId ()));
                  std::string exe_name = get_current_process_exe_name ();
                  if (!exe_name.empty ())
                    holder += " " + exe_name;
                  info.holder_process_info = holder;
                }
              else
                info.holder_process_info = k_busy_port_hint;
            }
        }
    }

  std::vector<std::string> sorted_ports (unique_ports.begin (),
                                         unique_ports.end ());
  std::sort (sorted_ports.begin (), sorted_ports.end (),
             [] (std::string const &lhs, std::string const &rhs) {
               int const num_lhs = com_number (lhs);
               int const num_rhs = com_number (rhs);
               if (num_lhs >= 0 && num_rhs >= 0)
                 return num_lhs < num_rhs;
               return lhs < rhs;
             });

  for (std::size_t i = 0; i < sorted_ports.size (); ++i)
    result.push_back (port_info_map[sorted_ports[i]]);

  if (result.empty () && (!need_to_filter_bluetooth || !setup_api_available))
    {
      for (int i = 1; i <= k_fallback_max_com; ++i)
        {
          std::string port_name = "COM" + std::to_string (i);
          std::string const open_path = path_for_open (port_name);
          HANDLE handle = ::CreateFileA (open_path.c_str (),
                                         GENERIC_READ | GENERIC_WRITE, 0,
                                         nullptr, OPEN_EXISTING, 0, nullptr);
          if (handle != INVALID_HANDLE_VALUE)
            {
              ::CloseHandle (handle);
              serial_port_info_t info;
              info.path = port_name;
              info.state = serial_port_state::available;
              result.push_back (info);
            }
          else
            {
              DWORD const err = ::GetLastError ();
              if (err == ERROR_ACCESS_DENIED || err == ERROR_SHARING_VIOLATION)
                {
                  serial_port_info_t info;
                  info.path = port_name;
                  info.state = serial_port_state::busy;
                  info.system_error
                      = port_process_resolver::format_system_error (
                          static_cast<int> (err));
                  if (!connected_port.empty () && port_name == connected_port)
                    {
                      std::string holder
                          = "PID="
                            + std::to_string (static_cast<unsigned long> (
                                ::GetCurrentProcessId ()));
                      std::string exe_name = get_current_process_exe_name ();
                      if (!exe_name.empty ())
                        holder += " " + exe_name;
                      info.holder_process_info = holder;
                    }
                  else
                    info.holder_process_info = k_busy_port_hint;
                  result.push_back (info);
                }
            }
        }
    }
#else
  static_cast<void> (need_to_filter_bluetooth);

  char const *const k_serial_prefixes[]
      = { "ttyS", "ttyUSB", "ttyACM", "ttyAMA", "ttyXRUSB", "ttyO" };
  std::size_t const k_num_prefixes
      = sizeof (k_serial_prefixes) / sizeof (k_serial_prefixes[0]);

  DIR *dir = ::opendir ("/sys/class/tty");
  if (dir != nullptr)
    {
      struct dirent *entry = nullptr;
      while ((entry = ::readdir (dir)) != nullptr)
        {
          std::string name (entry->d_name);
          if (name == "." || name == "..")
            continue;
          if (!is_safe_tty_name (name))
            continue;
          bool is_serial = false;
          for (std::size_t p = 0; p < k_num_prefixes; ++p)
            {
              if (name.find (k_serial_prefixes[p]) == 0)
                {
                  is_serial = true;
                  break;
                }
            }
          if (!is_serial)
            continue;

          std::string dev_path = "/dev/" + name;
          if (::access (dev_path.c_str (), F_OK) != 0)
            continue;

          serial_port_info_t info;
          info.path = dev_path;
          int fd = ::open (dev_path.c_str (), O_RDWR | O_NOCTTY | O_NONBLOCK);
          if (fd >= 0)
            {
              ::close (fd);
              info.state = serial_port_state::available;
              read_linux_device_attributes (name, info);
            }
          else if (errno == EBUSY || errno == EACCES)
            {
              info.state = serial_port_state::busy;
              read_linux_device_attributes (name, info);
              if (!connected_port.empty () && dev_path == connected_port)
                info.holder_process_info
                    = "PID="
                      + std::to_string (
                          static_cast<unsigned long> (::getpid ()));
            }
          else
            {
              info.state = serial_port_state::free;
              info.system_error
                  = port_process_resolver::format_system_error (errno);
            }
          result.push_back (info);
        }
      ::closedir (dir);
    }

  if (result.empty ())
    {
      dir = ::opendir ("/dev");
      if (dir != nullptr)
        {
          struct dirent *entry = nullptr;
          while ((entry = ::readdir (dir)) != nullptr)
            {
              std::string name (entry->d_name);
              if (!is_safe_tty_name (name))
                continue;
              if (name.find ("ttyUSB") != 0 && name.find ("ttyACM") != 0
                  && name.find ("ttyS") != 0)
                continue;
              std::string dev_path = "/dev/" + name;
              struct stat statbuf = {};
              if (::stat (dev_path.c_str (), &statbuf) != 0
                  || !S_ISCHR (statbuf.st_mode))
                continue;

              serial_port_info_t info;
              info.path = dev_path;
              int fd
                  = ::open (dev_path.c_str (), O_RDWR | O_NOCTTY | O_NONBLOCK);
              if (fd >= 0)
                {
                  ::close (fd);
                  info.state = serial_port_state::available;
                  read_linux_device_attributes (name, info);
                }
              else if (errno == EBUSY || errno == EACCES)
                {
                  info.state = serial_port_state::busy;
                  read_linux_device_attributes (name, info);
                  if (!connected_port.empty () && dev_path == connected_port)
                    info.holder_process_info
                        = "PID="
                          + std::to_string (
                              static_cast<unsigned long> (::getpid ()));
                }
              else
                {
                  info.state = serial_port_state::free;
                  info.system_error
                      = port_process_resolver::format_system_error (errno);
                }
              result.push_back (info);
            }
          ::closedir (dir);
        }
    }

  std::sort (
      result.begin (), result.end (),
      [] (serial_port_info_t const &lhs, serial_port_info_t const &rhs) {
        return lhs.path < rhs.path;
      });
#endif

  return result;
}

std::vector<std::string>
enumerate_serial_port_names (bool need_to_filter_bluetooth)
{
  std::vector<serial_port_info_t> detailed = enumerate_serial_ports_detailed (
      std::string (), need_to_filter_bluetooth);
  std::vector<std::string> names;
  names.reserve (detailed.size ());
  for (std::size_t i = 0; i < detailed.size (); ++i)
    names.push_back (detailed[i].path);
  return names;
}

} // namespace enumeration
} // namespace serial
} // namespace applied
} // namespace lumex
