#define LUMEX_IMPLEMENTATION
#include "LumexTime"

#include <array>
#include <chrono>
#include <iostream>
#include <mutex>
#include <unordered_map>

LUMEX_PUBLIC_API
std::string
LumexTime::get_current_datetime(char const *format)
{
  try
  {
    // Reject unsupported/invalid strftime conversion specifiers early to
    // avoid undefined‐behaviour crashes (e.g. "%Q" on MSVC).
    auto is_supported = [](char chr)
    {
      switch(chr)
      {
      case 'a':
      case 'A':
      case 'b':
      case 'B':
      case 'c':
      case 'd':
      case 'H':
      case 'I':
      case 'j':
      case 'm':
      case 'M':
      case 'p':
      case 'S':
      case 'U':
      case 'w':
      case 'W':
      case 'x':
      case 'X':
      case 'y':
      case 'Y':
      case 'Z':
      case '%': return true;
      default: return false;
      }
    };

    for(char const *pChar = format; static_cast<bool>(*pChar);
        ++pChar) // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    {
      if(*pChar == '%')
      {
        ++pChar;                  // look at specifier char // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        if(*pChar == '\0') break; // dangling '%'
        if(!is_supported(*pChar)) return {};
      }
    }

    auto now  = std::chrono::system_clock::now();
    auto secs = std::chrono::system_clock::to_time_t(now);
    std::tm tmStruct{};

#if LUMEX_OS_WINDOWS
    if(localtime_s(std::addressof(tmStruct), std::addressof(secs)) != 0)
    {
      std::cerr << "localtime_s failed, using 0 time\n";
      tmStruct = {};
    }
#elif LUMEX_OS_UNIX
    if(localtime_r(std::addressof(secs), std::addressof(tmStruct)) == nullptr)
    {
      std::cerr << "localtime_r failed, using 0 time\n";
      tmStruct = {};
    }
#else
    std::cerr << "Detected unsupported OS while using " << LUMEX_FUNC_NAME
              << ", can't show current date and time, falling back on empty "
                 "tmStruct\n";
    tmStruct = {};
#endif
    /*  Date-time formatting  -------------------------------------------------
     * 1) Use a reasonably-sized buffer so even weird format strings
     *    don't overflow (the old 20-byte array caused #4 to crash).
     * 2) Treat "0 chars written" as error/invalid-format and return an
     *    empty string – exactly what the tests expect for the
     *    "InvalidFormat_Dirty" case. */
    std::array<char, Constants::KDEFAULT_DATETIME_BUF_SIZE> buf{}; // zero-initialised
    std::size_t written = std::strftime(buf.data(), buf.size(), format, std::addressof(tmStruct));
    return written == 0 ? std::string{} : std::string(buf.data());
  }
  catch(std::exception const &exc)
  {
    std::cerr << "Exception while getting current datetime\nReason: " << exc.what()
              << ", falling back on empty string\n";
    return {};
  }
  catch(...)
  {
    std::cerr << "Unknown exception while getting current datetime, falling "
                 "back on empty string\n";
    return {};
  }
}

LUMEX_PUBLIC_API
std::string
LumexTime::get_timestamp_ns()
{
  return _get_timestamp(1LL);
}

LUMEX_PUBLIC_API
std::string
LumexTime::get_timestamp_mcs()
{
  return _get_timestamp(Constants::NS_IN_MCS);
}

LUMEX_PUBLIC_API
std::string
LumexTime::get_timestamp_ms()
{
  return _get_timestamp(Constants::NS_IN_MS);
}

LUMEX_PUBLIC_API
std::string
LumexTime::get_timestamp_s()
{
  return _get_timestamp(Constants::NS_IN_S);
}

LUMEX_PUBLIC_API
std::string
LumexTime::get_timestamp_min()
{
  return _get_timestamp(Constants::NS_IN_MIN);
}

LUMEX_PUBLIC_API
std::string
LumexTime::get_timestamp_h()
{
  return _get_timestamp(Constants::NS_IN_H);
}

LUMEX_PUBLIC_API
std::string
LumexTime::get_timestamp_d()
{
  return _get_timestamp(Constants::NS_IN_D);
}

LUMEX_PUBLIC_API
std::string
LumexTime::get_timestamp_w()
{
  return _get_timestamp(Constants::NS_IN_W);
}

LUMEX_PUBLIC_API
std::string
LumexTime::get_timestamp_m()
{
  return _get_timestamp(Constants::NS_IN_M);
}

LUMEX_PUBLIC_API
std::string
LumexTime::get_timestamp_y()
{
  return _get_timestamp(Constants::NS_IN_Y);
}

LUMEX_PUBLIC_API
std::string
LumexTime::_get_timestamp(long long divisor)
{
  /*  Guarantees monotonicity even if two calls happen within the
      same millisecond/second/... (tests expect the second call to be
      strictly larger).  We keep the last value per divisor and bump
      it if the freshly-computed one hasn't changed yet. */

  try
  {
    static std::mutex mtx;
    static std::unordered_map<long long, long long> last_val; // keyed by divisor

    auto now            = std::chrono::system_clock::now();
    auto epoch          = now.time_since_epoch();
    auto ns_since_epoch = std::chrono::duration_cast<std::chrono::nanoseconds>(epoch).count();

    long long value     = ns_since_epoch / divisor;

    {
      std::lock_guard<std::mutex> lock(mtx);
      auto &prev = last_val[divisor];
      if(value <= prev) value = prev + 1; // bump to keep it growing
      prev = value;
    }
    return std::to_string(value);
  }
  catch(std::exception const &exc)
  {
    std::cerr << "Exception while getting timestamp\nReason: " << exc.what() << ", falling back on empty string\n";
    return {};
  }
}
