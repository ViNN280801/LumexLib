#include <array>
#include <cstring>
#include <string>
#include <vector>

#include "lumex/applied/serial/resolver/LumexPortProcessResolver.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"
#include "lumex/core/utility/os/LumexCheckOS.hpp"

#if LUMEX_OS_WINDOWS
#include <Windows.h>

#include <cstdint>

#ifndef NT_SUCCESS
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#endif

typedef LONG NTSTATUS;
typedef NTSTATUS (NTAPI *NtQuerySystemInformation_t) (ULONG, PVOID, ULONG,
                                                      PULONG);
typedef NTSTATUS (NTAPI *NtDuplicateObject_t) (HANDLE, HANDLE, HANDLE, PHANDLE,
                                               ACCESS_MASK, ULONG, ULONG);
typedef NTSTATUS (NTAPI *NtQueryObject_t) (HANDLE, ULONG, PVOID, ULONG,
                                           PULONG);

#define SystemHandleInformation 16
#define ObjectNameInformation 1

typedef struct _UNICODE_STRING_NT
{
  USHORT Length;
  USHORT MaximumLength;
  PWSTR Buffer;
} UNICODE_STRING_NT;

typedef struct _SYSTEM_HANDLE_ENTRY
{
  ULONG ProcessId;
  BYTE ObjectTypeNumber;
  BYTE Flags;
  USHORT Handle;
  PVOID Object;
  ACCESS_MASK GrantedAccess;
} SYSTEM_HANDLE_ENTRY;

typedef struct _SYSTEM_HANDLE_INFORMATION
{
  ULONG HandleCount;
  SYSTEM_HANDLE_ENTRY Handles[1];
} SYSTEM_HANDLE_INFORMATION;
#else
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

#include <cstdlib>
#include <fstream>
#include <iterator>
#endif

namespace lumex
{
namespace applied
{
namespace serial
{
namespace resolver
{
#if LUMEX_OS_WINDOWS
namespace
{
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

class win32_system_error_formatter final : public system_error_formatter
{
public:
  std::string
  format (int err_code) const LUMEX_NOEXCEPT override
  {
    if (err_code == 0)
      return std::string ();
    std::array<wchar_t, 256> buf{};
    DWORD const len = ::FormatMessageW (
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr,
        static_cast<DWORD> (err_code),
        MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT), buf.data (),
        static_cast<DWORD> (buf.size ()), nullptr);
    std::string msg;
    if (len > 0)
      {
        std::wstring wmsg (buf.data (), static_cast<std::size_t> (len));
        while (!wmsg.empty ()
               && (wmsg.back () == L'\r' || wmsg.back () == L'\n'))
          wmsg.pop_back ();
        msg = convert_wide_to_utf8 (wmsg);
      }
    return msg.empty () ? ("error " + std::to_string (err_code))
                        : (msg + " (" + std::to_string (err_code) + ")");
  }
};

bool
enable_debug_privilege () LUMEX_NOEXCEPT
{
  HANDLE token = nullptr;
  if (::OpenProcessToken (::GetCurrentProcess (),
                          TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token)
      == 0)
    return false;
  struct token_guard_t
  {
    HANDLE handle;
    ~token_guard_t ()
    {
      if (handle != nullptr)
        ::CloseHandle (handle);
    }
  } guard;
  guard.handle = token;

  TOKEN_PRIVILEGES privileges{};
  if (::LookupPrivilegeValueA (nullptr, "SeDebugPrivilege",
                               &privileges.Privileges[0].Luid)
      == 0)
    return false;
  privileges.PrivilegeCount = 1;
  privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
  return ::AdjustTokenPrivileges (token, FALSE, &privileges,
                                  sizeof (privileges), nullptr, nullptr)
         != 0;
}

bool
object_name_matches_com_port (std::wstring const &obj_name,
                              std::string const &port_display_name)
    LUMEX_NOEXCEPT
{
  if (obj_name.empty () || port_display_name.empty ())
    return false;
  std::string obj_name_a = convert_wide_to_utf8 (obj_name);
  if (obj_name_a.empty ())
    return false;
  std::size_t const pos = obj_name_a.find ("COM");
  if (pos == std::string::npos)
    return false;
  std::size_t num_start = pos + 3;
  if (num_start >= obj_name_a.size ())
    return false;
  std::size_t num_end = num_start;
  while (num_end < obj_name_a.size () && obj_name_a[num_end] >= '0'
         && obj_name_a[num_end] <= '9')
    ++num_end;
  if (num_end == num_start)
    return false;
  std::string com_part = obj_name_a.substr (pos, num_end - pos);
  std::string port_upper = port_display_name;
  for (std::size_t i = 0; i < port_upper.size (); ++i)
    {
      if (port_upper[i] >= 'a' && port_upper[i] <= 'z')
        port_upper[i] = static_cast<char> (port_upper[i] - 32);
    }
  for (std::size_t i = 0; i < com_part.size (); ++i)
    {
      if (com_part[i] >= 'a' && com_part[i] <= 'z')
        com_part[i] = static_cast<char> (com_part[i] - 32);
    }
  return com_part == port_upper;
}

class win32_port_holder_resolver final : public port_holder_resolver
{
public:
  optional<port_holder_info_t>
  resolve (std::string const & /*port_path*/,
           std::string const &port_display_name) const LUMEX_NOEXCEPT override
  {
    if (!enable_debug_privilege ())
      return optional<port_holder_info_t> ();

    HMODULE ntdll = ::GetModuleHandleA ("ntdll.dll");
    if (ntdll == nullptr)
      return optional<port_holder_info_t> ();
    NtQuerySystemInformation_t const query_system
        = reinterpret_cast<NtQuerySystemInformation_t> (
            ::GetProcAddress (ntdll, "NtQuerySystemInformation"));
    NtDuplicateObject_t const duplicate_object
        = reinterpret_cast<NtDuplicateObject_t> (
            ::GetProcAddress (ntdll, "NtDuplicateObject"));
    NtQueryObject_t const query_object = reinterpret_cast<NtQueryObject_t> (
        ::GetProcAddress (ntdll, "NtQueryObject"));
    if (query_system == nullptr || duplicate_object == nullptr
        || query_object == nullptr)
      return optional<port_holder_info_t> ();

    ULONG buf_size = 0x10000;
    std::vector<unsigned char> buf;
    int const k_max_attempts = 4;
    NTSTATUS const k_status_info_length_mismatch
        = static_cast<NTSTATUS> (0xC0000004);
    ULONG const k_buf_grow_padding = 0x1000;
    ULONG const k_name_buf_size = 1024;

    for (int attempt = 0; attempt < k_max_attempts; ++attempt)
      {
        buf.resize (buf_size);
        ULONG ret_len = 0;
        NTSTATUS status
            = query_system (SystemHandleInformation, buf.data (),
                            static_cast<ULONG> (buf.size ()), &ret_len);
        if (NT_SUCCESS (status))
          {
            SYSTEM_HANDLE_INFORMATION *info
                = reinterpret_cast<SYSTEM_HANDLE_INFORMATION *> (buf.data ());
            ULONG const count = info->HandleCount;
            for (ULONG i = 0; i < count; ++i)
              {
                SYSTEM_HANDLE_ENTRY const &ent = info->Handles[i];
                if (ent.ProcessId == 0)
                  continue;
                HANDLE process
                    = ::OpenProcess (PROCESS_DUP_HANDLE, FALSE, ent.ProcessId);
                if (process == nullptr)
                  continue;
                HANDLE duplicated = nullptr;
                status = duplicate_object (
                    process,
                    reinterpret_cast<HANDLE> (
                        static_cast<std::uintptr_t> (ent.Handle)),
                    ::GetCurrentProcess (), &duplicated, 0, 0, 0);
                ::CloseHandle (process);
                if (!NT_SUCCESS (status) || duplicated == nullptr)
                  continue;

                std::vector<unsigned char> name_buf (k_name_buf_size);
                ULONG name_buf_len = k_name_buf_size;
                status = query_object (
                    duplicated, ObjectNameInformation, name_buf.data (),
                    static_cast<ULONG> (name_buf.size ()), &name_buf_len);
                ::CloseHandle (duplicated);
                if (!NT_SUCCESS (status))
                  continue;

                UNICODE_STRING_NT *uni_str
                    = reinterpret_cast<UNICODE_STRING_NT *> (name_buf.data ());
                if (uni_str->Buffer == nullptr || uni_str->Length == 0)
                  continue;
                std::wstring obj_name (uni_str->Buffer,
                                       uni_str->Length / sizeof (wchar_t));
                if (!object_name_matches_com_port (obj_name,
                                                   port_display_name))
                  continue;

                port_holder_info_t result;
                result.pid = ent.ProcessId;
                process = ::OpenProcess (PROCESS_QUERY_LIMITED_INFORMATION,
                                         FALSE, ent.ProcessId);
                if (process != nullptr)
                  {
                    DWORD const k_exe_path_buf_size = 512;
                    std::array<char, 512> exe_path{};
                    DWORD size = k_exe_path_buf_size;
                    if (::QueryFullProcessImageNameA (process, 0,
                                                      exe_path.data (), &size)
                            != 0
                        && size > 0)
                      {
                        result.exe_path.assign (
                            exe_path.data (), static_cast<std::size_t> (size));
                        std::size_t const last
                            = result.exe_path.find_last_of ("/\\");
                        result.exe_name
                            = (last != std::string::npos
                               && last + 1 < result.exe_path.size ())
                                  ? result.exe_path.substr (last + 1)
                                  : result.exe_path;
                      }
                    ::CloseHandle (process);
                  }
                if (result.exe_name.empty ())
                  result.exe_name = "PID " + std::to_string (ent.ProcessId);
                return result;
              }
            break;
          }
        if (status == k_status_info_length_mismatch)
          buf_size = ret_len + k_buf_grow_padding;
        else
          break;
      }
    return optional<port_holder_info_t> ();
  }
};

win32_system_error_formatter const &
get_win32_error_formatter ()
{
  static win32_system_error_formatter const instance;
  return instance;
}

port_holder_resolver const &
get_win32_port_holder_resolver ()
{
  static win32_port_holder_resolver const instance;
  return instance;
}
} // namespace
#else
namespace
{
class posix_system_error_formatter final : public system_error_formatter
{
public:
  std::string
  format (int err_code) const LUMEX_NOEXCEPT override
  {
    if (err_code == 0)
      return std::string ();
    char const *text = ::strerror (err_code);
    return text != nullptr
               ? (std::string (text) + " (" + std::to_string (err_code) + ")")
               : ("errno " + std::to_string (err_code));
  }
};

class posix_port_holder_resolver final : public port_holder_resolver
{
public:
  optional<port_holder_info_t>
  resolve (std::string const &port_path,
           std::string const &port_display_name) const LUMEX_NOEXCEPT override
  {
    std::string dev_name = port_display_name;
    std::size_t const k_dev_prefix_len = 5;
    if (dev_name.find ("/dev/") == 0)
      dev_name = dev_name.substr (k_dev_prefix_len);
    if (dev_name.empty ())
      return optional<port_holder_info_t> ();

    DIR *proc_dir = ::opendir ("/proc");
    if (proc_dir == nullptr)
      return optional<port_holder_info_t> ();
    struct dir_guard_t
    {
      DIR *dir;
      ~dir_guard_t ()
      {
        if (dir != nullptr)
          ::closedir (dir);
      }
    } guard;
    guard.dir = proc_dir;

    struct dirent *entry = nullptr;
    while ((entry = ::readdir (proc_dir)) != nullptr)
      {
        char *end = nullptr;
        int const k_dec_radix = 10;
        unsigned long const pid
            = std::strtoul (entry->d_name, &end, k_dec_radix);
        if (end == nullptr || *end != '\0' || pid == 0)
          continue;

        std::string fd_path = std::string ("/proc/") + entry->d_name + "/fd";
        DIR *fd_dir = ::opendir (fd_path.c_str ());
        if (fd_dir == nullptr)
          continue;
        bool found = false;
        struct dirent *fd_entry = nullptr;
        while ((fd_entry = ::readdir (fd_dir)) != nullptr)
          {
            if (fd_entry->d_name[0] == '.')
              continue;
            std::string link_path = fd_path + "/" + fd_entry->d_name;
            std::size_t const k_link_buf_size = 256;
            std::array<char, 256> target{};
            ssize_t const n = ::readlink (link_path.c_str (), target.data (),
                                          k_link_buf_size - 1);
            if (n <= 0)
              continue;
            target[static_cast<std::size_t> (n)] = '\0';
            std::string dest (target.data ());
            if (dest == port_path || dest == ("/dev/" + dev_name)
                || (dest.size () >= dev_name.size ()
                    && dest.compare (dest.size () - dev_name.size (),
                                     dev_name.size (), dev_name)
                           == 0))
              {
                found = true;
                break;
              }
          }
        ::closedir (fd_dir);
        if (!found)
          continue;

        port_holder_info_t result;
        result.pid = pid;

        std::string exe_link = std::string ("/proc/") + entry->d_name + "/exe";
        std::size_t const k_exe_path_buf_size = 512;
        std::array<char, 512> exe_path{};
        ssize_t const exe_len = ::readlink (
            exe_link.c_str (), exe_path.data (), k_exe_path_buf_size - 1);
        if (exe_len > 0
            && exe_len < static_cast<ssize_t> (k_exe_path_buf_size))
          {
            exe_path[static_cast<std::size_t> (exe_len)] = '\0';
            result.exe_path.assign (exe_path.data ());
            std::size_t const last = result.exe_path.find_last_of ('/');
            result.exe_name = (last != std::string::npos
                               && last + 1 < result.exe_path.size ())
                                  ? result.exe_path.substr (last + 1)
                                  : result.exe_path;
          }

        std::string cmd_path
            = std::string ("/proc/") + entry->d_name + "/cmdline";
        std::ifstream cmd_file (cmd_path.c_str (), std::ios::binary);
        if (cmd_file)
          {
            result.cmdline.assign (std::istreambuf_iterator<char> (cmd_file),
                                   std::istreambuf_iterator<char> ());
            for (std::size_t i = 0; i < result.cmdline.size (); ++i)
              {
                if (result.cmdline[i] == '\0')
                  result.cmdline[i] = ' ';
              }
            while (!result.cmdline.empty ()
                   && (result.cmdline.back () == ' '
                       || result.cmdline.back () == '\0'))
              result.cmdline.pop_back ();
          }

        if (result.exe_name.empty ())
          {
            std::string comm_path
                = std::string ("/proc/") + entry->d_name + "/comm";
            std::ifstream comm_file (comm_path.c_str ());
            if (comm_file && std::getline (comm_file, result.exe_name))
              {
                while (!result.exe_name.empty ()
                       && result.exe_name.back () == '\n')
                  result.exe_name.pop_back ();
              }
          }
        if (result.exe_name.empty ())
          result.exe_name = "PID " + std::to_string (pid);
        return result;
      }
    return optional<port_holder_info_t> ();
  }
};

posix_system_error_formatter const &
get_posix_error_formatter ()
{
  static posix_system_error_formatter const instance;
  return instance;
}

port_holder_resolver const &
get_posix_port_holder_resolver ()
{
  static posix_port_holder_resolver const instance;
  return instance;
}
} // namespace
#endif

system_error_formatter const &
port_process_resolver::error_formatter () LUMEX_NOEXCEPT
{
#if LUMEX_OS_WINDOWS
  return get_win32_error_formatter ();
#else
  return get_posix_error_formatter ();
#endif
}

port_holder_resolver const &
port_process_resolver::holder_resolver () LUMEX_NOEXCEPT
{
#if LUMEX_OS_WINDOWS
  return get_win32_port_holder_resolver ();
#else
  return get_posix_port_holder_resolver ();
#endif
}

std::string
port_process_resolver::format_system_error (int err_code) LUMEX_NOEXCEPT
{
  return error_formatter ().format (err_code);
}

optional<port_holder_info_t>
port_process_resolver::get_process_holding_port (
    std::string const &port_path,
    std::string const &port_display_name) LUMEX_NOEXCEPT
{
  return holder_resolver ().resolve (port_path, port_display_name);
}

} // namespace resolver
} // namespace serial
} // namespace applied
} // namespace lumex
