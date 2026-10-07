/*
 * Copyright (c) 2026 Vladislav Semykin <vladislav.semykin@gmail.com>
 *
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of
 * charge, to any person obtaining a copy
 * of this software and associated
 * documentation files (the "Software"), to deal
 * in the Software without
 * restriction, including without limitation the rights
 * to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the
 * Software, and to permit persons to whom the Software is
 * furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice
 * and this permission notice shall be included in
 * all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT
 * WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO
 * THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH
 * THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#define LUMEX_IMPLEMENTATION
#include "lumex/LumexExport.hpp"

#include "LumexStacktrace.hpp"
#include "lumex/core/utility/macros/LumexKeywords.hpp"
namespace lumex // NOLINT(modernize-concat-nested-namespaces)
{
namespace core
{
namespace exceptions
{
namespace stacktrace
{
namespace detail
{
#if defined(LUMEX_OS_WINDOWS)
// Static member definition
bool dbg_help_initializer::s_initialized = false;

// Static mutex definition
std::mutex g_dbghelp_mutex;

// dbg_help_initializer implementation
dbg_help_initializer::dbg_help_initializer ()
{
  std::lock_guard<std::mutex> lock (g_dbghelp_mutex);
  if (!s_initialized)
    {
      HANDLE process = GetCurrentProcess ();
      SymSetOptions (SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
      if (SymInitialize (process, nullptr, TRUE) == TRUE)
        s_initialized = true;
    }
}

bool
dbg_help_initializer::is_initialized () const LUMEX_NOEXCEPT
{
  return s_initialized;
}

// Template specialization for capture_stacktrace
template <>
LUMEX_PUBLIC_API lumex_basic_stacktrace<std::allocator<lumex_stacktrace_entry>>
capture_stacktrace<std::allocator<lumex_stacktrace_entry>> (
    std::size_t skip, std::size_t max_depth,
    std::allocator<lumex_stacktrace_entry> const &alloc) LUMEX_NOEXCEPT
{
  // Ensure DbgHelp is initialized
  static dbg_help_initializer dbghelp_init;

  if (!dbghelp_init.is_initialized ())
    return lumex_basic_stacktrace<std::allocator<lumex_stacktrace_entry>> (
        alloc);

  // Capture raw addresses
  std::size_t const max_frames = 128;
  void *frames[max_frames];

  USHORT frame_count = CaptureStackBackTrace (
      static_cast<DWORD> (skip + 1),
      static_cast<DWORD> ((std::min)(max_depth, max_frames)), frames, nullptr);

  // Convert to lumex_stacktrace_entry vector
  using container_type = typename lumex_basic_stacktrace<
      std::allocator<lumex_stacktrace_entry>>::container_type;
  container_type entries (alloc);
  entries.reserve (frame_count);

  for (USHORT i = 0; i < frame_count; ++i)
    if (frames[i] != nullptr)
      entries.emplace_back (frames[i]);

  return lumex_basic_stacktrace<std::allocator<lumex_stacktrace_entry>> (
      std::move (entries));
}

bool
resolve_symbol_info (void *address, std::string &function_name,
                     std::string &source_file,
                     std::uint32_t &line_number) LUMEX_NOEXCEPT
{
  if (address == nullptr)
    return false;

  std::lock_guard<std::mutex> lock (g_dbghelp_mutex);

  HANDLE process = GetCurrentProcess ();

  // Get symbol info
  char symbol_buffer[sizeof (SYMBOL_INFO) + MAX_SYM_NAME];
  SYMBOL_INFO *symbol = reinterpret_cast<SYMBOL_INFO *> (symbol_buffer);
  symbol->SizeOfStruct = sizeof (SYMBOL_INFO);
  symbol->MaxNameLen = MAX_SYM_NAME;

  DWORD64 displacement = 0;
  bool has_symbol = SymFromAddr (process, reinterpret_cast<DWORD64> (address),
                                 &displacement, symbol)
                    != FALSE;

  if (has_symbol)
    {
      function_name = symbol->Name;

      // Try to demangle C++ names
      char undecorated[MAX_SYM_NAME];
      if (UnDecorateSymbolName (symbol->Name, undecorated, MAX_SYM_NAME,
                                UNDNAME_COMPLETE)
          > 0)
        function_name = undecorated;
    }
  else
    {
      // Fallback to address
      char addr_str[kDefaultAddrStrSize];
      std::snprintf (addr_str, kDefaultAddrStrSize, "0x%p", address);
      function_name = addr_str;
    }

  // Get line info
  IMAGEHLP_LINE64 line_info = {};
  line_info.SizeOfStruct = sizeof (IMAGEHLP_LINE64);
  DWORD line_displacement = 0;

  if (SymGetLineFromAddr64 (process, reinterpret_cast<DWORD64> (address),
                            &line_displacement, &line_info)
      == TRUE)
    {
      source_file = line_info.FileName;
      line_number = line_info.LineNumber;

      // Append to function name for complete description
      function_name += " at ";
      function_name += line_info.FileName;
      function_name += ":";
      function_name += std::to_string (line_info.LineNumber);
    }
  else
    {
      source_file.clear ();
      line_number = 0;

      // Add module info to description
      IMAGEHLP_MODULE64 module_info = {};
      module_info.SizeOfStruct = sizeof (IMAGEHLP_MODULE64);

      if (SymGetModuleInfo64 (process, reinterpret_cast<DWORD64> (address),
                              &module_info)
          == TRUE)
        {
          function_name += " in ";
          function_name += module_info.ModuleName;
        }
    }

  return true;
}
#else
std::string
demangle_symbol (char const *mangled)
{
  if (mangled == nullptr)
    return "";

  int status = 0;
  std::unique_ptr<char, void (*) (void *)> demangled (
      abi::__cxa_demangle (mangled, nullptr, nullptr, &status), std::free);

  if (status == 0 && demangled)
    return std::string (demangled.get ());
  return std::string (mangled);
}

template <>
LUMEX_PUBLIC_API lumex_basic_stacktrace<std::allocator<lumex_stacktrace_entry>>
capture_stacktrace<std::allocator<lumex_stacktrace_entry>> (
    std::size_t skip, std::size_t max_depth,
    std::allocator<lumex_stacktrace_entry> const &alloc) LUMEX_NOEXCEPT
{
  // Capture raw addresses
  std::size_t const max_frames = 128;
  void *frames[max_frames];

  int frame_count = backtrace (
      frames, static_cast<int> ((std::min)(max_depth + skip + 1, max_frames)));

  // If we have no frames at all, return empty
  if (frame_count <= 0)
    return lumex_basic_stacktrace<std::allocator<lumex_stacktrace_entry>> (
        alloc);

  using container_type = typename lumex_basic_stacktrace<
      std::allocator<lumex_stacktrace_entry>>::container_type;
  container_type entries (alloc);

  // In Release builds, we might only get 1 frame due to optimizations
  // In that case, include it even if skip would normally exclude it
  int start_index = (frame_count <= 1) ? 0 : static_cast<int> (skip + 1);
  int actual_count = std::max (0, frame_count - start_index);

  if (actual_count <= 0)
    return lumex_basic_stacktrace<std::allocator<lumex_stacktrace_entry>> (
        alloc);

  entries.reserve (static_cast<container_type::size_type> (actual_count));

  for (int i = start_index; i < frame_count; ++i)
    {
      if (frames[i])
        entries.emplace_back (frames[i]);
    }

  return lumex_basic_stacktrace<std::allocator<lumex_stacktrace_entry>> (
      std::move (entries));
}

bool
resolve_symbol_info (void *address, std::string &function_name,
                     std::string &source_file,
                     std::uint32_t &line_number) LUMEX_NOEXCEPT
{
  source_file.clear ();
  line_number = 0;
  if (address == nullptr)
    return false;

  try
    {
      std::string const location
          = utility::debug::Detail::describe_module_address (address);

      Dl_info info;
      if (dladdr (address, &info) != 0 && info.dli_sname != nullptr)
        {
          function_name = demangle_symbol (info.dli_sname);
          if (!location.empty ())
            function_name += " (" + location + ")";
          return true;
        }
      if (!location.empty ())
        {
          function_name = location;
          return true;
        }
    }
  catch (...)
    {
      // Fall through to the raw address.
    }

  char addr_str[kDefaultAddrStrSize];
  std::snprintf (addr_str, kDefaultAddrStrSize, "0x%p", address);
  function_name = addr_str;
  return false;
}
#endif
} // namespace detail
} // namespace stacktrace
} // namespace exceptions
} // namespace core
} // namespace lumex

// Suppress C4251 warnings for explicit template instantiation
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

// GCC on Windows (MinGW) ignores, and warns about, an export attribute on an
// explicit instantiation of a class that was defined with it already.
#if defined(__MINGW32__)
#define LUMEX_STACKTRACE_INSTANTIATION_API
#else
#define LUMEX_STACKTRACE_INSTANTIATION_API LUMEX_API
#endif

// Explicit template instantiation
template class LUMEX_STACKTRACE_INSTANTIATION_API
    lumex::core::exceptions::stacktrace::lumex_basic_stacktrace<std::allocator<
        lumex::core::exceptions::stacktrace::lumex_stacktrace_entry>>;

#ifdef _MSC_VER
#pragma warning(pop)
#endif
