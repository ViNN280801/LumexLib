// example_format_api.cpp
// LumexFormat, part 2: every output function, in its char and wchar_t form,
// the type-erased API, locales, run-time format strings, errors, and a
// user function that takes a checked format string.
#include <cstddef>
#include <iostream>
#include <iterator>
#include <locale>
#include <sstream>
#include <string>
#include <vector>

#include "lumex/core/fmt/LumexFormat"

namespace fmt = lumex::core::fmt;

namespace
{
class CommaDecimal : public std::numpunct<char>
{
protected:
  char
  do_decimal_point () const override
  {
    return ',';
  }
};

/**
 * A user function with a checked format string: from C++20 a wrong literal
 * is a compile error at the caller, below C++20 it throws at run time.
 */
template <typename... Args>
std::string
tagged (char const *tag, fmt::FormatString<Args...> text, Args &&...args)
{
  return std::string (tag) + ": "
         + fmt::vformat (text.get (), fmt::make_format_args (args...));
}

/** The same for wide text. */
template <typename... Args>
std::wstring
wide_tagged (fmt::WFormatString<Args...> text, Args &&...args)
{
  return L"w: " + fmt::vformat (text.get (), fmt::make_wformat_args (args...));
}

/** The wide counterpart of `render`. */
std::wstring
render_wide (std::wstring const &text, fmt::WFormatArgs args)
{
  return fmt::vformat (text, args);
}

/** A type-erased sink: formats with arguments captured elsewhere. */
std::string
render (std::string const &text, fmt::FormatArgs args)
{
  return fmt::vformat (text, args);
}
} // namespace

int
main ()
{
  std::cout << "=== LumexFormat: output API ===\n";

  std::cout << "\n--- 1. format (char, wchar_t, with a locale) ---\n";
  std::cout << fmt::format ("{} {}", "narrow", 1) << '\n';
  std::wstring const wide = fmt::format (L"{} {} {:>4}", L"wide", 2, 'c');
  std::wcout << wide << L'\n';
  std::locale const comma (std::locale::classic (), new CommaDecimal);
  std::cout << fmt::format (comma, "{:L}", 2.5) << '\n';
  std::wcout << fmt::format (std::locale::classic (), L"{:L}", 3.5) << L'\n';

  std::cout << "\n--- 2. format_to: any output iterator ---\n";
  std::string text;
  fmt::format_to (std::back_inserter (text), "part {};", 1);
  fmt::format_to (std::back_inserter (text), comma, " {:L};", 1.5);
  std::cout << text << '\n';
  std::vector<char> bytes;
  fmt::format_to (std::back_inserter (bytes), "{:#x}", 255);
  std::cout << std::string (bytes.begin (), bytes.end ()) << '\n';
  char array[16] = {};
  char *const end = fmt::format_to (array, "{}", 12345);
  std::cout << "array holds " << (end - array) << " chars: " << array << '\n';
  std::wstring wide_text;
  fmt::format_to (std::back_inserter (wide_text), L"{}+{}", 1, 2);
  std::wcout << wide_text << L'\n';

  std::cout << "\n--- 3. format_to_n and formatted_size ---\n";
  char small[5] = {};
  fmt::format_to_n_result_t<char *> const cut
      = fmt::format_to_n (small, 4, "{}", 123456789);
  std::cout << "wrote \"" << std::string (small, 4) << "\" of " << cut.size
            << " chars, out at +" << (cut.out - small) << '\n';
  wchar_t wide_small[3] = {};
  fmt::format_to_n_result_t<wchar_t *> const wide_cut
      = fmt::format_to_n (wide_small, 2, L"{}", 98765);
  std::cout << "wide full size " << wide_cut.size << '\n';
  std::cout << "formatted_size: " << fmt::formatted_size ("{:>10}", 1) << ", "
            << fmt::formatted_size (L"{}", 12345) << '\n';

  std::cout
      << "\n--- 4. Type-erased: make_format_args, vformat, vformat_to ---\n";
  int const number = 7;
  std::string const name = "seven";
  std::cout << render ("{} is {}", fmt::make_format_args (number, name))
            << '\n';
  std::cout << fmt::vformat (comma, "{:L}", fmt::make_format_args (0.25))
            << '\n';
  std::string sink;
  fmt::vformat_to (std::back_inserter (sink), "{}-{}",
                   fmt::make_format_args (1, 2));
  std::cout << sink << '\n';
  std::wcout << render_wide (L"{} {}", fmt::make_wformat_args (1, L"two"))
             << L'\n';
  std::wstring wide_sink;
  fmt::vformat_to (std::back_inserter (wide_sink), L"{}",
                   fmt::make_wformat_args (3));
  std::wcout << fmt::vformat (std::locale::classic (), L"{:L}",
                              fmt::make_wformat_args (4))
             << wide_sink << L'\n';
  std::cout << tagged ("tag", "{} and {:.2f}", 1, 2.0) << '\n';
  std::wcout << wide_tagged (L"{}", 42) << L'\n';

  std::cout << "\n--- 5. Run-time format strings ---\n";
  std::string const from_config = "{:>8} | {:<6}";
  std::cout << fmt::format (fmt::runtime (from_config), "right", "left")
            << '\n';
  std::wcout << fmt::format (fmt::runtime (L"{}"), 5) << L'\n';

  std::cout << "\n--- 6. Errors: FormatError and try_format ---\n";
  try
    {
      (void)fmt::format (fmt::runtime ("{} and {:d}"), 1, "not a number");
    }
  catch (fmt::FormatError const &error)
    {
      std::cout << "FormatError: " << error.what () << " at offset "
                << error.position () << '\n';
    }
  fmt::FormatError const plain ("no offset");
  std::cout << "no_position: "
            << (plain.position () == fmt::FormatError::no_position ()) << '\n';
  fmt::try_format_result_t<char> const good = fmt::try_format ("{:>5}", 1);
  fmt::try_format_result_t<char> const bad
      = fmt::try_format (fmt::runtime ("{} {}"), 1);
  std::cout << "try_format: [" << good.text << "] success=" << good.success
            << "; [" << bad.error << "] success=" << bad.success << '\n';
  fmt::try_format_result_t<wchar_t> const wide_try
      = fmt::try_format (L"{}", 3);
  std::wcout << L"wide try_format: " << wide_try.text << L'\n';

  std::cout << "\n--- 7. print and println ---\n";
  fmt::print ("print to std::cout: {}\n", 1);
  fmt::println ("println to std::cout: {}", 2);
  std::ostringstream stream;
  fmt::print (stream, "{}+", 3);
  fmt::println (stream, "{}", 4);
  std::cout << "stream got: " << stream.str ();
  std::wostringstream wide_stream;
  fmt::print (wide_stream, L"{}", 5);
  fmt::println (wide_stream, L"{}", 6);
  std::wcout << L"wide stream got: " << wide_stream.str ();

  std::cout << "\n=== example_format_api finished ===\n";
  return 0;
}
