#ifndef LUMEX_XML_UTILS_HPP
#define LUMEX_XML_UTILS_HPP

#include <array>
#include <cfloat> // For DBL_DIG
#include <cstdio>
#include <cstdlib>
#include <cwchar>

#include "lumex/xml/constants/XmlConstants.hpp"
#include "lumex/xml/memory/XmlAllocator.hpp"
#include "lumex/xml/types/XmlTypes.hpp"
#include "lumex/xml/utility/XmlMacros.hpp"
#include "lumex/xml/xpath/memory/XPathAllocator.hpp"

using namespace Lumex::Xml::Memory;
using namespace Lumex::Xml::Constants;
using namespace Lumex::Xml::Types;
using namespace Lumex::Xml::XPath::Memory;

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace Utility
    {
      struct opt_false {
        enum : std::uint8_t
        {
          value = 0
        };
      };
      struct opt_true {
        enum : std::uint8_t
        {
          value = 1
        };
      };
      struct utf16_counter {
        using value_type = size_t;

        static value_type
        low(value_type result, uint32_t /* unused */)
        {
          return result + 1;
        }

        static value_type
        high(value_type result, uint32_t /* unused */)
        {
          return result + 2;
        }
      };
      inline uint16_t
      endian_swap(uint16_t value)
      {
        return static_cast<uint16_t>(((value & 0xff) << 8) | (value >> 8));
      }

      inline uint32_t
      endian_swap(uint32_t value)
      {
        return ((value & 0xff) << 24) | ((value & 0xff00) << 8) | ((value & 0xff0000) >> 8) | (value >> 24);
      }
      template <typename opt_swap> struct utf16_decoder {
        using type = uint16_t;

        template <typename Traits>
        static typename Traits::value_type
        process(uint16_t const *data, size_t size, typename Traits::value_type result, Traits /*unused*/)
        {
          while(size)
          {
            uint16_t lead = opt_swap::value ? endian_swap(*data) : *data;

            // U+0000..U+D7FF
            if(lead < 0xD800)
            { // NOLINT(bugprone-branch-clone)
              result = Traits::low(result, lead);
              data += 1;
              size -= 1;
            }
            // U+E000..U+FFFF
            else if(static_cast<unsigned int>(lead - 0xE000) < 0x2000)
            {
              result = Traits::low(result, lead);
              data += 1;
              size -= 1;
            }
            // surrogate pair lead
            else if(static_cast<unsigned int>(lead - 0xD800) < 0x400 && size >= 2)
            {
              uint16_t next = opt_swap::value ? endian_swap(data[1]) : data[1];

              if(static_cast<unsigned int>(next - 0xDC00) < 0x400)
              {
                result = Traits::high(result, 0x10000 + ((lead & 0x3ff) << 10) + (next & 0x3ff));
                data += 2;
                size -= 2;
              }
              else
              {
                data += 1;
                size -= 1;
              }
            }
            else
            {
              data += 1;
              size -= 1;
            }
          }

          return result;
        }
      };

      template <typename opt_swap> struct utf32_decoder {
        using type = uint32_t;

        template <typename Traits>
        static typename Traits::value_type
        process(uint32_t const *data, size_t size, typename Traits::value_type result, Traits /* unused */)
        {
          while(size)
          {
            uint32_t lead = opt_swap::value ? endian_swap(*data) : *data;

            // U+0000..U+FFFF
            if(lead < 0x10000)
            {
              result = Traits::low(result, lead);
              data += 1;
              size -= 1;
            }
            // U+10000..U+10FFFF
            else
            {
              result = Traits::high(result, lead);
              data += 1;
              size -= 1;
            }
          }

          return result;
        }
      };

      struct latin1_decoder {
        using type = uint8_t;

        template <typename Traits>
        static typename Traits::value_type
        process(uint8_t const *data, size_t size, typename Traits::value_type result, Traits /* unused */)
        {
          while(size)
          {
            result = Traits::low(result, *data);
            data += 1;
            size -= 1;
          }

          return result;
        }
      };
      struct utf8_counter {
        using value_type = size_t;

        static value_type
        low(value_type result, uint32_t chr) // NOLINT(bugprone-easily-swappable-parameters)
        {
          // U+0000..U+007F
          if(chr < 0x80) return result + 1;

          // U+0080..U+07FF
          if(chr < 0x800) return result + 2;

          // U+0800..U+FFFF
          return result + 3;
        }

        static value_type
        high(value_type result, uint32_t /* unused */)
        {
          // U+10000..U+10FFFF
          return result + 4;
        }
      };

      struct utf8_writer {
        using value_type = uint8_t *;

        static value_type
        low(value_type result, uint32_t chr) // NOLINT(bugprone-easily-swappable-parameters)
        {
          // U+0000..U+007F
          if(chr < 0x80)
          {
            *result = static_cast<uint8_t>(chr);
            return result + 1;
          }
          // U+0080..U+07FF
          if(chr < 0x800)
          {
            result[0] = static_cast<uint8_t>(0xC0 | (chr >> 6));
            result[1] = static_cast<uint8_t>(0x80 | (chr & 0x3F));
            return result + 2;
          }
          // U+0800..U+FFFF
          result[0] = static_cast<uint8_t>(0xE0 | (chr >> 12));
          result[1] = static_cast<uint8_t>(0x80 | ((chr >> 6) & 0x3F));
          result[2] = static_cast<uint8_t>(0x80 | (chr & 0x3F));
          return result + 3;
        }

        static value_type
        high(value_type result, uint32_t chr)
        {
          // U+10000..U+10FFFF
          result[0] = static_cast<uint8_t>(0xF0 | (chr >> 18));
          result[1] = static_cast<uint8_t>(0x80 | ((chr >> 12) & 0x3F));
          result[2] = static_cast<uint8_t>(0x80 | ((chr >> 6) & 0x3F));
          result[3] = static_cast<uint8_t>(0x80 | (chr & 0x3F));
          return result + 4;
        }

        static value_type
        any(value_type result, uint32_t chr)
        {
          return (chr < 0x10000) ? low(result, chr) : high(result, chr);
        }
      };
      struct utf16_writer {
        using value_type = uint16_t *;

        static value_type
        low(value_type result, uint32_t chr) // NOLINT(bugprone-easily-swappable-parameters)
        {
          *result = static_cast<uint16_t>(chr);

          return result + 1;
        }

        static value_type
        high(value_type result, uint32_t chr) // NOLINT(bugprone-easily-swappable-parameters)
        {
          uint32_t msh = (chr - 0x10000U) >> 10;
          uint32_t lsh = (chr - 0x10000U) & 0x3ff;

          result[0]    = static_cast<uint16_t>(0xD800 + msh);
          result[1]    = static_cast<uint16_t>(0xDC00 + lsh);

          return result + 2;
        }

        static value_type
        any(value_type result, uint32_t chr) // NOLINT(bugprone-easily-swappable-parameters)
        {
          return (chr < 0x10000) ? low(result, chr) : high(result, chr);
        }
      };

      struct utf32_counter {
        using value_type = size_t;

        static value_type
        low(value_type result, uint32_t /* unused */)
        {
          return result + 1;
        }

        static value_type
        high(value_type result, uint32_t /* unused */)
        {
          return result + 1;
        }
      };

      struct utf32_writer {
        using value_type = uint32_t *;

        static value_type
        low(value_type result, uint32_t chr)
        {
          *result = chr;

          return result + 1;
        }

        static value_type
        high(value_type result, uint32_t chr)
        {
          *result = chr;

          return result + 1;
        }

        static value_type
        any(value_type result, uint32_t chr)
        {
          *result = chr;

          return result + 1;
        }
      };

      struct latin1_writer {
        using value_type = uint8_t *;

        static value_type
        low(value_type result, uint32_t chr)
        {
          *result = static_cast<uint8_t>(chr > 255 ? '?' : chr);

          return result + 1;
        }

        static value_type
        high(value_type result, uint32_t chr)
        {
          (void)chr;

          *result = '?';

          return result + 1;
        }
      };
      template <size_t size> struct wchar_selector;
      template <> struct wchar_selector<2> {
        using type    = uint16_t;
        using counter = utf16_counter;
        using writer  = utf16_writer;
        using decoder = utf16_decoder<opt_false>;
      };

      template <> struct wchar_selector<4> {
        using type    = uint32_t;
        using counter = utf32_counter;
        using writer  = utf32_writer;
        using decoder = utf32_decoder<opt_false>;
      };

      using wchar_counter = wchar_selector<sizeof(wchar_t)>::counter;
      using wchar_writer  = wchar_selector<sizeof(wchar_t)>::writer;

      struct wchar_decoder {
        using type = wchar_t;

        template <typename Traits>
        static typename Traits::value_type
        process(wchar_t const *data, size_t size, typename Traits::value_type result, Traits traits)
        {
          using decoder = wchar_selector<sizeof(wchar_t)>::decoder;

          return decoder::process(reinterpret_cast<typename decoder::type const *>(data), size, result, traits);
        }
      };
      struct utf8_decoder {
        using type = uint8_t;

        template <typename Traits>
        static typename Traits::value_type
        process(uint8_t const *data, size_t size, typename Traits::value_type result, Traits /* unused */)
        {
          uint8_t const utf8_byte_mask = 0x3f;

          while(size)
          {
            uint8_t lead = *data;

            // 0xxxxxxx -> U+0000..U+007F
            if(lead < 0x80)
            {
              result = Traits::low(result, lead);
              data += 1;
              size -= 1;

              // process aligned single-byte (ascii) blocks
              if((reinterpret_cast<uintptr_t>(data) & 3) == 0)
              {
                // round-trip through void* to silence 'cast increases required alignment of target type' warnings
                while(size >= 4
                      && (*static_cast<uint32_t const *>(static_cast< // NOLINT(bugprone-casting-through-void)
                                                         void const *>(data))
                          & 0x80808080)
                           == 0)
                {
                  result = Traits::low(result, data[0]);
                  result = Traits::low(result, data[1]);
                  result = Traits::low(result, data[2]);
                  result = Traits::low(result, data[3]);
                  data += 4;
                  size -= 4;
                }
              }
            }
            // 110xxxxx -> U+0080..U+07FF
            else if(static_cast<unsigned int>(lead - 0xC0) < 0x20 && size >= 2 && (data[1] & 0xc0) == 0x80)
            {
              result = Traits::low(result, ((lead & ~0xC0) << 6) | (data[1] & utf8_byte_mask));
              data += 2;
              size -= 2;
            }
            // 1110xxxx -> U+0800-U+FFFF
            else if(static_cast<unsigned int>(lead - 0xE0) < 0x10 && size >= 3 && (data[1] & 0xc0) == 0x80
                    && (data[2] & 0xc0) == 0x80)
            {
              result = Traits::low(result, ((lead & ~0xE0) << 12) | ((data[1] & utf8_byte_mask) << 6)
                                             | (data[2] & utf8_byte_mask));
              data += 3;
              size -= 3;
            }
            // 11110xxx -> U+10000..U+10FFFF
            else if(static_cast<unsigned int>(lead - 0xF0) < 0x08 && size >= 4 && (data[1] & 0xc0) == 0x80
                    && (data[2] & 0xc0) == 0x80 && (data[3] & 0xc0) == 0x80)
            {
              result = Traits::high(result, ((lead & ~0xF0) << 18) | ((data[1] & utf8_byte_mask) << 12)
                                              | ((data[2] & utf8_byte_mask) << 6) | (data[3] & utf8_byte_mask));
              data += 4;
              size -= 4;
            }
            // 10xxxxxx or 11111xxx -> invalid
            else
            {
              data += 1;
              size -= 1;
            }
          }

          return result;
        }
      };
      template <typename U>
      inline U
      string_to_integer(char_t const *value, // NOLINT(readability-function-cognitive-complexity)
                        U minv, U maxv)      // NOLINT(bugprone-easily-swappable-parameters)
      {
        U result          = 0;
        char_t const *str = value;

        while(LUMEX_XML_IS_CHARTYPE(*str, ct_space)) // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
          str++;

        bool negative = (*str == '-');

        str += (*str == '+' || *str == '-');

        bool overflow = false;

        if(str[0] == '0' && (str[1] | ' ') == 'x')
        {
          str += 2;

          // since overflow detection relies on length of the sequence skip leading zeros
          while(*str == '0') str++;

          char_t const *start = str;

          for(;;)
          {
            if(static_cast<unsigned>(*str - '0') < kDecimalBase)
              result = (result * kHexadecimalBase) + (*str - '0');
            else if(static_cast<unsigned>((*str | ' ') - 'a') < kHexCharOffsetLimit)
              result = (result * kHexadecimalBase) + ((*str | ' ') - 'a' + kDecimalBase);
            else
              break;

            str++;
          }

          auto digits = static_cast<size_t>(str - start);
          overflow    = digits > sizeof(U) * 2;
        }
        else
        {
          // since overflow detection relies on length of the sequence skip leading zeros
          while(*str == '0') str++;

          char_t const *start = str;

          for(;;)
          {
            if(static_cast<unsigned>(*str - '0') < kDecimalBase)
              result = (result * kDecimalBase) + (*str - '0');
            else
              break;

            str++;
          }

          auto digits = static_cast<size_t>(str - start);

          static_assert(sizeof(U) == kSizeOfU8Bytes || sizeof(U) == 4 || sizeof(U) == 2);

          size_t const max_digits10 = sizeof(U) == 8 ? 20
                                      : sizeof(U) // NOLINT(readability-avoid-nested-conditional-operator)
                                          == 4
                                        ? 10
                                        : 5;
          char_t const max_lead     = sizeof(U) == 8 ? '1'
                                      : sizeof(U) // NOLINT(readability-avoid-nested-conditional-operator)
                                      == 4
                                        ? '4'
                                        : '6';
          size_t const high_bit     = (sizeof(U) * 8) - 1;

          overflow                  = digits >= max_digits10
                     && !(digits == max_digits10 && (*start < max_lead || (*start == max_lead && result >> high_bit)));
        }

        if(negative) return (overflow || result > 0 - minv) ? minv : 0 - result;
        return (overflow || result > maxv) ? maxv : result;
      }

      inline int
      get_value_int(char_t const *value)
      {
        return string_to_integer<unsigned int>( // NOLINT(cppcoreguidelines-narrowing-conversions,
                                                // bugprone-narrowing-conversions)
          value, static_cast<unsigned int>(INT_MIN), INT_MAX);
      }

      inline unsigned int
      get_value_uint(char_t const *value)
      {
        return string_to_integer<unsigned int>(value, 0, UINT_MAX);
      }

      inline double
      get_value_double(char_t const *value)
      {
#ifdef LUMEX_XML_WCHAR_MODE
        return wcstod(value, nullptr);
#else
        return strtod(value, nullptr);
#endif
      }

#ifdef LUMEX_XML_WCHAR_MODE
      inline void
      convert_wchar_endian_swap(wchar_t *result, wchar_t const *data, size_t length)
      {
        for(size_t i = 0; i < length; ++i)
          result[i] = static_cast<wchar_t>(endian_swap(static_cast<wchar_selector<sizeof(wchar_t)>::type>(data[i])));
      }
#endif

      inline float
      get_value_float(char_t const *value)
      {
#ifdef LUMEX_XML_WCHAR_MODE
        return static_cast<float>(wcstod(value, nullptr));
#else
        return static_cast<float>(strtod(value, nullptr));
#endif
      }

      inline bool
      get_value_bool(char_t const *value)
      {
        // only look at first char
        char_t first = *value;

        // 1*, t* (true), T* (True), y* (yes), Y* (YES)
        return (first == '1' || first == 't' || first == 'T' || first == 'y' || first == 'Y');
      }

      inline long long
      get_value_llong(char_t const *value)
      {
        return string_to_integer<unsigned long long>( // NOLINT(cppcoreguidelines-narrowing-conversions,
                                                      // bugprone-narrowing-conversions)
          value, static_cast<unsigned long long>(LLONG_MIN), LLONG_MAX);
      }

      inline unsigned long long
      get_value_ullong(char_t const *value)
      {
        return string_to_integer<unsigned long long>(value, 0, ULLONG_MAX);
      }

      template <typename Header>
      inline bool
      strcpy_insitu_allow(size_t length, Header const &header, uintptr_t header_mask, char_t *target)
      {
        if(header & kxml_memory_page_contents_shared_mask) return false;

        size_t target_length = strlength(target);
        if((header & header_mask) == 0) return target_length >= length;

        size_t const reuse_threshold = 32;
        return target_length >= length
               && (target_length < reuse_threshold || target_length - length < target_length / 2);
      }

      template <typename String, typename Header>
      inline bool
      strcpy_insitu(String &dest, Header &header, uintptr_t header_mask, char_t const *source, size_t source_length)
      {
        LUMEX_ASSERT((header & header_mask) == 0 || dest);
        if(source_length == 0)
        {
          XmlAllocator *alloc = LUMEX_XML_GETPAGE_IMPL( // NOLINT(cppcoreguidelines-pro-type-const-cast)
                                  header)
                                  ->allocator;

          if(header & header_mask) alloc->deallocate_string(dest);

          dest = nullptr;
          header &= ~header_mask;

          return true;
        }

        if(dest && strcpy_insitu_allow(source_length, header, header_mask, dest))
        {
          memcpy(dest, source, source_length * sizeof(char_t));
          dest[source_length] = 0;

          return true;
        }

        XmlAllocator *alloc = LUMEX_XML_GETPAGE_IMPL( // NOLINT(cppcoreguidelines-pro-type-const-cast)
                                header)
                                ->allocator;
        char_t *buf = alloc->allocate_string(source_length + 1);
        if(!buf) return false;

        memcpy(buf, source, source_length * sizeof(char_t));
        buf[source_length] = 0;

        if(header & header_mask) alloc->deallocate_string(dest);

        dest = buf;
        header |= header_mask;

        return true;
      }

      inline size_t
      strlength(char_t const *str)
      {
        LUMEX_ASSERT(str);

#ifdef LUMEX_XML_WCHAR_MODE
        return wcslen(str);
#else
        return strlen(str);
#endif
      }

      // Compare two strings
      inline bool
      strequal(char_t const *src, char_t const *dst)
      {
        LUMEX_ASSERT(src && dst);

#ifdef LUMEX_XML_WCHAR_MODE
        return wcscmp(src, dst) == 0;
#else
        return strcmp(src, dst) == 0;
#endif
      }

#if __cplusplus >= 201703L
      // Check if the null-terminated dst string is equal to the entire contents of srcview
      inline bool
      stringview_equal(string_view_t srcview, char_t const *dst)
      {
        // std::basic_string_view::compare(const char*) has the right behavior, but it performs an
        // extra traversal of dst to compute its length.
        LUMEX_ASSERT(dst);
        char_t const *src = srcview.data();
        size_t srclen     = srcview.size();

        while((srclen != 0) && (*dst != 0) && (*src == *dst))
        {
          --srclen;
          ++dst;
          ++src;
        }
        return srclen == 0 && *dst == 0;
      }
#endif

      // Compare lhs with [rhs_begin, rhs_end)
      inline bool
      strequalrange(char_t const *lhs, char_t const *rhs, size_t count)
      {
        for(size_t i = 0; i < count; ++i)
          if(lhs[i] != rhs[i]) return false;

        return lhs[count] == 0;
      }

      // Get length of wide string, even if CRT lacks wide character support
      inline size_t
      strlength_wide(wchar_t const *str)
      {
        LUMEX_ASSERT(str);

#ifdef LUMEX_XML_WCHAR_MODE
        return wcslen(str);
#else
        const wchar_t *end = str;
        while(*end != 0) end++;
        return static_cast<size_t>(end - str);
#endif
      }

      template <typename U, typename String, typename Header>
      inline bool
      set_value_integer(String &dest, Header &header, uintptr_t header_mask, U value, bool negative)
      {
        static constexpr size_t const kBufSize = 64UL;
        std::array<char_t, kBufSize> buf{};
        char_t *end   = buf.data() + buf.size();
        char_t *begin = integer_to_string(buf, end, value, negative);

        return strcpy_insitu(dest, header, header_mask, begin, end - begin);
      }

      template <typename String, typename Header>
      inline bool
      set_value_convert(String &dest, Header &header,
                        uintptr_t header_mask, // NOLINT(bugprone-easily-swappable-parameters)
                        float value, int precision)
      {
        static constexpr size_t const kBufSize = 128U;
        std::array<char_t, kBufSize> buf{};
        snprintf(buf, "%.*g", precision, double(value));

        return set_value_ascii(dest, header, header_mask, buf);
      }

      template <typename String, typename Header>
      inline bool
      set_value_convert(String &dest, Header &header,
                        uintptr_t header_mask, // NOLINT(bugprone-easily-swappable-parameters)
                        double value, int precision)
      {
        static constexpr size_t const kBufSize = 128U;
        std::array<char_t, kBufSize> buf{};
        snprintf(buf, "%.*g", precision, value);
        return set_value_ascii(dest, header, header_mask, buf);
      }

      template <typename String, typename Header>
      inline bool
      set_value_bool(String &dest, Header &header, uintptr_t header_mask, bool value)
      {
        return strcpy_insitu(dest, header, header_mask, value ? LUMEX_XML_TEXT("true") : LUMEX_XML_TEXT("false"),
                             value ? kTrueStringLength : kFalseStringLength);
      }

      inline double
      gen_nan()
      {
        double const volatile zero = 0.0;
        return zero / zero; // NOLINT(bugprone-divide-by-zero, misc-redundant-expression
      }

      inline bool
      is_nan(double value)
      {
        double const volatile val = value;
        return val != val;
      }

      inline bool
      hash_insert(void const **table, size_t size, void const *key)
      {
        LUMEX_ASSERT(key);

        auto hash_value                               = static_cast<unsigned int>(reinterpret_cast<uintptr_t>(key));

        constexpr unsigned int kMurmurHash3Finalizer  = 0x85ebca6bU;
        constexpr unsigned int kMurmurHash3Multiplier = 0xc2b2ae35U;
        constexpr unsigned int kMurmurHash3Shift16    = 16;
        constexpr unsigned int kMurmurHash3Shift13    = 13;

        // MurmurHash3 32-bit finalizer
        hash_value ^= hash_value >> kMurmurHash3Shift16;
        hash_value *= kMurmurHash3Finalizer;
        hash_value ^= hash_value >> kMurmurHash3Shift13;
        hash_value *= kMurmurHash3Multiplier;
        hash_value ^= hash_value >> kMurmurHash3Shift16;

        size_t hashmod = size - 1;
        size_t bucket  = hash_value & hashmod;

        for(size_t probe = 0; probe <= hashmod; ++probe)
        {
          if(table[bucket] == nullptr)
          {
            table[bucket] = key;
            return true;
          }

          if(table[bucket] == key) return false;

          // hash collision, quadratic probing
          bucket = (bucket + probe + 1) & hashmod;
        }

        LUMEX_ASSERT(false && "Hash table is full"); // unreachable
        return false;
      }

      inline unsigned int
      hash_string(char_t const *str)
      {
        // Jenkins one-at-a-time hash (http://en.wikipedia.org/wiki/Jenkins_hash_function#one-at-a-time)
        unsigned int result                   = 0;

        constexpr unsigned int const kShift3  = 3;
        constexpr unsigned int const kShift6  = 6;
        constexpr unsigned int const kShift10 = 10;
        constexpr unsigned int const kShift11 = 11;
        constexpr unsigned int const kShift15 = 15;

        while(*str != 0)
        {
          result += static_cast<unsigned int>(*str++);
          result += result << kShift10;
          result ^= result >> kShift6;
        }

        result += result << kShift3;
        result ^= result >> kShift11;
        result += result << kShift15;

        return result;
      }

      inline bool
      is_little_endian()
      {
        unsigned int chr = 1;

        return *reinterpret_cast<unsigned char *>(std::addressof(chr)) == 1;
      }

      inline xml_encoding
      get_wchar_encoding()
      {
        static_assert(sizeof(wchar_t) == 2 || sizeof(wchar_t) == 4);

        if(sizeof(wchar_t) == 2) return is_little_endian() ? encoding_utf16_le : encoding_utf16_be;
        return is_little_endian() ? encoding_utf32_le : encoding_utf32_be;
      }

      inline bool
      parse_declaration_encoding(uint8_t const *data, // NOLINT(readability-function-cognitive-complexity)
                                 size_t size, uint8_t const *&out_encoding, size_t &out_length)
      {
        // check if we have a non-empty XML declaration
        if(size < 6 // NOLINT(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
           || !(
             (data[0] == '<') &                            // NOLINT(readability-implicit-bool-conversion)
               (data[1] == '?') &                          // NOLINT(readability-implicit-bool-conversion)
               (data[2] == 'x') &                          // NOLINT(readability-implicit-bool-conversion)
               (data[3] == 'm') &                          // NOLINT(readability-implicit-bool-conversion)
               (data[4] == 'l')                            // NOLINT(readability-implicit-bool-conversion)
             && LUMEX_XML_IS_CHARTYPE(data[5], ct_space))) // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
          return false;

        // scan XML declaration until the encoding field
        for(size_t i = 6; i + 1 < size; ++i) // NOLINT(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
        {
          // declaration can not contain ? in quoted values
          if(data[i] == '?') return false;

          if(data[i] == 'e' && data[i + 1] == 'n')
          {
            size_t offset = i;

            // encoding follows the version field which can't contain 'en' so this has to be the encoding if XML is well
            // formed
            LUMEX_XML_SCANCHAR('e');
            LUMEX_XML_SCANCHAR('n');
            LUMEX_XML_SCANCHAR('c');
            LUMEX_XML_SCANCHAR('o');
            LUMEX_XML_SCANCHAR('d');
            LUMEX_XML_SCANCHAR('i');
            LUMEX_XML_SCANCHAR('n');
            LUMEX_XML_SCANCHAR('g');

            // S? = S?
            LUMEX_XML_SCANCHARTYPE(ct_space); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
            LUMEX_XML_SCANCHAR('=');
            LUMEX_XML_SCANCHARTYPE(ct_space); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)

            // the only two valid delimiters are ' and "
            uint8_t delimiter = (offset < size && data[offset] == '"') ? '"' : '\'';

            LUMEX_XML_SCANCHAR(delimiter);

            size_t start = offset;

            out_encoding = data + offset;

            LUMEX_XML_SCANCHARTYPE(ct_symbol); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)

            out_length = offset - start;

            LUMEX_XML_SCANCHAR(delimiter);

            return true;
          }
        }

        return false;
      }

      inline xml_encoding
      guess_buffer_encoding(uint8_t const *data, size_t size) // NOLINT(readability-function-cognitive-complexity)
      {
        // skip encoding autodetection if input buffer is too small
        if(size < 4) return encoding_utf8;

        uint8_t d0_ = data[0];
        uint8_t d1_ = data[1];
        uint8_t d2_ = data[2];
        uint8_t d3_ = data[3];

        // look for BOM in first few bytes
        if(d0_ == 0 && d1_ == 0 && d2_ == 0xfe && d3_ == 0xff) return encoding_utf32_be;
        if(d0_ == 0xff && d1_ == 0xfe && d2_ == 0 && d3_ == 0) return encoding_utf32_le;
        if(d0_ == 0xfe && d1_ == 0xff) return encoding_utf16_be;
        if(d0_ == 0xff && d1_ == 0xfe) return encoding_utf16_le;
        if(d0_ == 0xef && d1_ == 0xbb && d2_ == 0xbf) return encoding_utf8;

        // look for <, <? or <?xm in various encodings
        if(d0_ == 0 && d1_ == 0 && d2_ == 0 && d3_ == 0x3c) return encoding_utf32_be;
        if(d0_ == 0x3c && d1_ == 0 && d2_ == 0 && d3_ == 0) return encoding_utf32_le;
        if(d0_ == 0 && d1_ == 0x3c && d2_ == 0 && d3_ == 0x3f) return encoding_utf16_be;
        if(d0_ == 0x3c && d1_ == 0 && d2_ == 0x3f && d3_ == 0) return encoding_utf16_le;

        // look for utf16 < followed by node name (this may fail, but is better than utf8 since it's zero terminated so
        // early)
        if(d0_ == 0 && d1_ == 0x3c) return encoding_utf16_be;
        if(d0_ == 0x3c && d1_ == 0) return encoding_utf16_le;

        // no known BOM detected; parse declaration
        uint8_t const *enc = nullptr;
        size_t enc_length  = 0;

        if(d0_ == 0x3c && d1_ == 0x3f && d2_ == 0x78 && d3_ == 0x6d
           && parse_declaration_encoding(data, size, enc, enc_length))
        {
          // iso-8859-1 (case-insensitive)
          if(enc_length == 10 && (enc[0] | ' ') == 'i' && (enc[1] | ' ') == 's' && (enc[2] | ' ') == 'o'
             && enc[3] == '-' && enc[4] == '8' && enc[5] == '8' && enc[6] == '5' && enc[7] == '9' && enc[8] == '-'
             && enc[9] == '1')
            return encoding_latin1;

          // latin1 (case-insensitive)
          if(enc_length == 6 && (enc[0] | ' ') == 'l' && (enc[1] | ' ') == 'a' && (enc[2] | ' ') == 't'
             && (enc[3] | ' ') == 'i' && (enc[4] | ' ') == 'n' && enc[5] == '1')
            return encoding_latin1;
        }

        return encoding_utf8;
      }

      inline xml_encoding
      get_buffer_encoding(xml_encoding encoding, void const *contents, size_t size)
      {
        // replace wchar encoding with utf implementation
        if(encoding == encoding_wchar) return get_wchar_encoding();

        // replace utf16 encoding with utf16 with specific endianness
        if(encoding == encoding_utf16) return is_little_endian() ? encoding_utf16_le : encoding_utf16_be;

        // replace utf32 encoding with utf32 with specific endianness
        if(encoding == encoding_utf32) return is_little_endian() ? encoding_utf32_le : encoding_utf32_be;

        // only do autodetection if no explicit encoding is requested
        if(encoding != encoding_auto) return encoding;

        // try to guess encoding (based on XML specification, Appendix F.1)
        uint8_t const *data = static_cast<uint8_t const *>(contents);

        return guess_buffer_encoding(data, size);
      }

      inline bool
      get_mutable_buffer(char_t *&out_buffer, size_t &out_length, void const *contents, size_t size, bool is_mutable)
      {
        size_t length = size / sizeof(char_t);

        if(is_mutable)
        {
          out_buffer
            = static_cast<char_t *>(const_cast<void *>(contents)); // NOLINT(cppcoreguidelines-pro-type-const-cast)
          out_length = length;
        }
        else
        {
          auto *buffer                                                      // NOLINT(cppcoreguidelines-owning-memory)
            = static_cast<char_t *>(malloc((length + 1) * sizeof(char_t))); // NOLINT(cppcoreguidelines-no-malloc)
          if(buffer == nullptr) return false;

          if(contents != nullptr)
            std::memcpy(buffer, contents, length * sizeof(char_t));
          else
            LUMEX_ASSERT(length == 0);

          buffer[length] = 0;

          out_buffer     = buffer;
          out_length     = length + 1;
        }

        return true;
      }

      inline xml_encoding
      get_write_native_encoding()
      {
#ifdef LUMEX_XML_WCHAR_MODE
        return get_wchar_encoding();
#else
        return encoding_utf8;
#endif
      }

      inline xml_encoding
      get_write_encoding(xml_encoding encoding)
      {
        // replace wchar encoding with utf implementation
        if(encoding == encoding_wchar) return get_wchar_encoding();

        // replace utf16 encoding with utf16 with specific endianness
        if(encoding == encoding_utf16) return is_little_endian() ? encoding_utf16_le : encoding_utf16_be;

        // replace utf32 encoding with utf32 with specific endianness
        if(encoding == encoding_utf32) return is_little_endian() ? encoding_utf32_le : encoding_utf32_be;

        // only do autodetection if no explicit encoding is requested
        if(encoding != encoding_auto) return encoding;

        // assume utf8 encoding
        return encoding_utf8;
      }

#ifdef LUMEX_XML_WCHAR_MODE
      inline bool
      need_endian_swap_utf(xml_encoding le_, xml_encoding re_) // NOLINT(bugprone-easily-swappable-parameters)
      {
        return (le_ == encoding_utf16_be && re_ == encoding_utf16_le)
               || (le_ == encoding_utf16_le && re_ == encoding_utf16_be)
               || (le_ == encoding_utf32_be && re_ == encoding_utf32_le)
               || (le_ == encoding_utf32_le && re_ == encoding_utf32_be);
      }

      inline bool
      convert_buffer_endian_swap(char_t *&out_buffer, size_t &out_length, void const *contents, size_t size,
                                 bool is_mutable)
      {
        auto const *data = static_cast<char_t const *>(contents);
        size_t length    = size / sizeof(char_t);

        if(is_mutable)
        {
          auto *buffer = const_cast<char_t *>(data); // NOLINT(cppcoreguidelines-pro-type-const-cast)

          convert_wchar_endian_swap(buffer, data, length);

          out_buffer = buffer;
          out_length = length;
        }
        else
        {
          auto *buffer = static_cast<char_t *>(     // NOLINT(cppcoreguidelines-owning-memory)
            malloc((length + 1) * sizeof(char_t))); // NOLINT(cppcoreguidelines-no-malloc)
          if(buffer == nullptr) return false;

          convert_wchar_endian_swap(buffer, data, length);
          buffer[length] = 0;

          out_buffer     = buffer;
          out_length     = length + 1;
        }

        return true;
      }

      template <typename D>
      inline bool
      convert_buffer_generic(char_t *&out_buffer, size_t &out_length, void const *contents, size_t size, D)
      {
        typename D::type const *data = static_cast<typename D::type const *>(contents);
        size_t data_length           = size / sizeof(typename D::type);

        // first pass: get length in wchar_t units
        size_t length = D::process(data, data_length, 0, wchar_counter());

        // allocate buffer of suitable length
        auto *buffer = static_cast<char_t *>(     // NOLINT(cppcoreguidelines-owning-memory)
          malloc((length + 1) * sizeof(char_t))); // NOLINT(cppcoreguidelines-no-malloc)
        if(buffer == nullptr) return false;

        // second pass: convert utf16 input to wchar_t
        wchar_writer::value_type obegin = reinterpret_cast<wchar_writer::value_type>(buffer);
        wchar_writer::value_type oend   = D::process(data, data_length, obegin, wchar_writer());

        LUMEX_ASSERT(oend == obegin + length);
        *oend      = 0;

        out_buffer = buffer;
        out_length = length + 1;

        return true;
      }

      inline bool
      convert_buffer(char_t *&out_buffer, size_t &out_length, xml_encoding encoding, void const *contents, size_t size,
                     bool is_mutable)
      {
        // get native encoding
        xml_encoding wchar_encoding = get_wchar_encoding();

        // fast path: no conversion required
        if(encoding == wchar_encoding) return get_mutable_buffer(out_buffer, out_length, contents, size, is_mutable);

        // only endian-swapping is required
        if(need_endian_swap_utf(encoding, wchar_encoding))
          return convert_buffer_endian_swap(out_buffer, out_length, contents, size, is_mutable);

        // source encoding is utf8
        if(encoding == encoding_utf8)
          return convert_buffer_generic(out_buffer, out_length, contents, size, utf8_decoder());

        // source encoding is utf16
        if(encoding == encoding_utf16_be || encoding == encoding_utf16_le)
        {
          xml_encoding native_encoding = is_little_endian() ? encoding_utf16_le : encoding_utf16_be;

          return (native_encoding == encoding)
                   ? convert_buffer_generic(out_buffer, out_length, contents, size, utf16_decoder<opt_false>())
                   : convert_buffer_generic(out_buffer, out_length, contents, size, utf16_decoder<opt_true>());
        }

        // source encoding is utf32
        if(encoding == encoding_utf32_be || encoding == encoding_utf32_le)
        {
          xml_encoding native_encoding = is_little_endian() ? encoding_utf32_le : encoding_utf32_be;

          return (native_encoding == encoding)
                   ? convert_buffer_generic(out_buffer, out_length, contents, size, utf32_decoder<opt_false>())
                   : convert_buffer_generic(out_buffer, out_length, contents, size, utf32_decoder<opt_true>());
        }

        // source encoding is latin1
        if(encoding == encoding_latin1)
          return convert_buffer_generic(out_buffer, out_length, contents, size, latin1_decoder());

        LUMEX_ASSERT(false && "Invalid encoding"); // unreachable
        return false;
      }
#else
      template <typename D>
      inline bool
      convert_buffer_generic(char_t *&out_buffer, size_t &out_length, void const *contents, size_t size, D /* unused */)
      {
        typename D::type const *data = static_cast<typename D::type const *>(contents);
        size_t data_length           = size / sizeof(typename D::type);

        // first pass: get length in utf8 units
        size_t length = D::process(data, data_length, 0, utf8_counter());

        // allocate buffer of suitable length
        auto *buffer = static_cast<char_t *>(     // NOLINT(cppcoreguidelines-owning-memory)
          malloc((length + 1) * sizeof(char_t))); // NOLINT(cppcoreguidelines-no-malloc)
        if(buffer == nullptr) return false;

        // second pass: convert utf16 input to utf8
        uint8_t *obegin = reinterpret_cast<uint8_t *>(buffer);
        uint8_t *oend   = D::process(data, data_length, obegin, utf8_writer());

        LUMEX_ASSERT(oend == obegin + length);
        *oend      = 0;

        out_buffer = buffer;
        out_length = length + 1;

        return true;
      }

      inline size_t
      get_latin1_7bit_prefix_length(uint8_t const *data, size_t size)
      {
        for(size_t i = 0; i < size; ++i)
          if(data[i] > 127) return i;

        return size;
      }

      inline bool
      convert_buffer_latin1(char_t *&out_buffer, size_t &out_length, void const *contents, size_t size, bool is_mutable)
      {
        uint8_t const *data = static_cast<uint8_t const *>(contents);
        size_t data_length  = size;

        // get size of prefix that does not need utf8 conversion
        size_t prefix_length = get_latin1_7bit_prefix_length(data, data_length);
        LUMEX_ASSERT(prefix_length <= data_length);

        uint8_t const *postfix = data + prefix_length;
        size_t postfix_length  = data_length - prefix_length;

        // if no conversion is needed, just return the original buffer
        if(postfix_length == 0) return get_mutable_buffer(out_buffer, out_length, contents, size, is_mutable);

        // first pass: get length in utf8 units
        size_t length = prefix_length + latin1_decoder::process(postfix, postfix_length, 0, utf8_counter());

        // allocate buffer of suitable length
        auto *buffer =                                                  // NOLINT(cppcoreguidelines-owning-memory)
          static_cast<char_t *>(malloc((length + 1) * sizeof(char_t))); // NOLINT(cppcoreguidelines-no-malloc)
        if(buffer == nullptr) return false;

        // second pass: convert latin1 input to utf8
        memcpy(buffer, data, prefix_length);

        uint8_t *obegin = reinterpret_cast<uint8_t *>(buffer);
        uint8_t *oend   = latin1_decoder::process(postfix, postfix_length, obegin + prefix_length, utf8_writer());

        LUMEX_ASSERT(oend == obegin + length);
        *oend      = 0;

        out_buffer = buffer;
        out_length = length + 1;

        return true;
      }

      inline bool
      convert_buffer(char_t *&out_buffer, size_t &out_length, xml_encoding encoding, void const *contents, size_t size,
                     bool is_mutable)
      {
        // fast path: no conversion required
        if(encoding == encoding_utf8) return get_mutable_buffer(out_buffer, out_length, contents, size, is_mutable);

        // source encoding is utf16
        if(encoding == encoding_utf16_be || encoding == encoding_utf16_le)
        {
          xml_encoding native_encoding = is_little_endian() ? encoding_utf16_le : encoding_utf16_be;

          return (native_encoding == encoding)
                   ? convert_buffer_generic(out_buffer, out_length, contents, size, utf16_decoder<opt_false>())
                   : convert_buffer_generic(out_buffer, out_length, contents, size, utf16_decoder<opt_true>());
        }

        // source encoding is utf32
        if(encoding == encoding_utf32_be || encoding == encoding_utf32_le)
        {
          xml_encoding native_encoding = is_little_endian() ? encoding_utf32_le : encoding_utf32_be;

          return (native_encoding == encoding)
                   ? convert_buffer_generic(out_buffer, out_length, contents, size, utf32_decoder<opt_false>())
                   : convert_buffer_generic(out_buffer, out_length, contents, size, utf32_decoder<opt_true>());
        }

        // source encoding is latin1
        if(encoding == encoding_latin1)
          return convert_buffer_latin1(out_buffer, out_length, contents, size, is_mutable);

        LUMEX_ASSERT(false && "Invalid encoding"); // unreachable
        return false;
      }
#endif

      inline size_t
      as_utf8_begin(const wchar_t *str, size_t length)
      {
        // get length in utf8 characters
        return wchar_decoder::process(str, length, 0, utf8_counter());
      }

      inline void
      as_utf8_end(char *buffer, size_t size, wchar_t const *str, size_t length)
      {
        // convert to utf8
        uint8_t *begin = reinterpret_cast<uint8_t *>(buffer);
        uint8_t *end   = wchar_decoder::process(str, length, begin, utf8_writer());

        LUMEX_ASSERT(begin + size == end);
        (void)(end == nullptr);
        (void)(size == 0U);
      }

      inline std::string
      as_utf8_impl(wchar_t const *str, size_t length)
      {
        // first pass: get length in utf8 characters
        size_t size = as_utf8_begin(str, length);

        // allocate resulting string
        std::string result;
        result.resize(size);

        // second pass: convert to utf8
        if(size > 0) as_utf8_end(&result[0], size, str, length);

        return result;
      }

      inline std::basic_string<wchar_t>
      as_wide_impl(char const *str, size_t size)
      {
        uint8_t const *data = reinterpret_cast<uint8_t const *>(str);

        // first pass: get length in wchar_t units
        size_t length = utf8_decoder::process(data, size, 0, wchar_counter());

        // allocate resulting string
        std::basic_string<wchar_t> result;
        result.resize(length);

        // second pass: convert to wchar_t
        if(length > 0)
        {
          wchar_writer::value_type begin = reinterpret_cast<wchar_writer::value_type>(&result[0]);
          wchar_writer::value_type end   = utf8_decoder::process(data, size, begin, wchar_writer());

          LUMEX_ASSERT(begin + length == end);
          (void)(end == nullptr);
        }

        return result;
      }
      template <typename D, typename T>
      inline size_t
      convert_buffer_output_generic(typename T::value_type dest, char_t const *data, size_t length, D /* unused */,
                                    T /* unused */)
      {
        static_assert(sizeof(char_t) == sizeof(typename D::type));

        typename T::value_type end = D::process(reinterpret_cast<typename D::type const *>(data), length, dest, T());

        return static_cast<size_t>(end - dest) * sizeof(*dest);
      }

      template <typename D, typename T>
      inline size_t
      convert_buffer_output_generic(typename T::value_type dest, char_t const *data, size_t length, D /* unused */,
                                    T /* unused */, bool opt_swap)
      {
        static_assert(sizeof(char_t) == sizeof(typename D::type));

        typename T::value_type end = D::process(reinterpret_cast<typename D::type const *>(data), length, dest, T());

        if(opt_swap)
          for(typename T::value_type i = dest; i != end; ++i) *i = endian_swap(*i);

        return static_cast<size_t>(end - dest) * sizeof(*dest);
      }

#ifdef LUMEX_XML_WCHAR_MODE
      inline size_t
      get_valid_length(char_t const *data, size_t length)
      {
        if(length < 1) return 0;

        // discard last character if it's the lead of a surrogate pair
        return (sizeof(wchar_t) == 2
                && static_cast<unsigned int>(static_cast<uint16_t>(data[length - 1]) - 0xD800) < 0x400)
                 ? length - 1
                 : length;
      }

      inline size_t
      convert_buffer_output(char_t const *r_char, uint8_t *r_u8, uint16_t *r_u16, uint32_t *r_u32, char_t const *data,
                            size_t length, xml_encoding encoding)
      {
        // only endian-swapping is required
        if(need_endian_swap_utf(encoding, get_wchar_encoding()))
        {
          convert_wchar_endian_swap(r_char, data, length);

          return length * sizeof(char_t);
        }

        // convert to utf8
        if(encoding == encoding_utf8)
          return convert_buffer_output_generic(r_u8, data, length, wchar_decoder(), utf8_writer());

        // convert to utf16
        if(encoding == encoding_utf16_be || encoding == encoding_utf16_le)
        {
          xml_encoding native_encoding = is_little_endian() ? encoding_utf16_le : encoding_utf16_be;

          return convert_buffer_output_generic(r_u16, data, length, wchar_decoder(), utf16_writer(),
                                               native_encoding != encoding);
        }

        // convert to utf32
        if(encoding == encoding_utf32_be || encoding == encoding_utf32_le)
        {
          xml_encoding native_encoding = is_little_endian() ? encoding_utf32_le : encoding_utf32_be;

          return convert_buffer_output_generic(r_u32, data, length, wchar_decoder(), utf32_writer(),
                                               native_encoding != encoding);
        }

        // convert to latin1
        if(encoding == encoding_latin1)
          return convert_buffer_output_generic(r_u8, data, length, wchar_decoder(), latin1_writer());

        LUMEX_ASSERT(false && "Invalid encoding"); // unreachable
        return 0;
      }
#else
      inline size_t
      get_valid_length(char_t const *data, size_t length)
      {
        if(length < 5) return 0;

        for(size_t i = 1; i <= 4; ++i)
        {
          auto chr = static_cast<uint8_t>(data[length - i]);

          // either a standalone character or a leading one
          if((chr & 0xc0) != 0x80) return length - i;
        }

        // there are four non-leading characters at the end, sequence tail is broken so might as well process the whole
        // chunk
        return length;
      }

      inline size_t
      convert_buffer_output(char_t * /* r_char */, uint8_t *r_u8, uint16_t *r_u16, uint32_t *r_u32, char_t const *data,
                            size_t length, xml_encoding encoding)
      {
        if(encoding == encoding_utf16_be || encoding == encoding_utf16_le)
        {
          xml_encoding native_encoding = is_little_endian() ? encoding_utf16_le : encoding_utf16_be;

          return convert_buffer_output_generic(r_u16, data, length, utf8_decoder(), utf16_writer(),
                                               native_encoding != encoding);
        }

        if(encoding == encoding_utf32_be || encoding == encoding_utf32_le)
        {
          xml_encoding native_encoding = is_little_endian() ? encoding_utf32_le : encoding_utf32_be;

          return convert_buffer_output_generic(r_u32, data, length, utf8_decoder(), utf32_writer(),
                                               native_encoding != encoding);
        }

        if(encoding == encoding_latin1)
          return convert_buffer_output_generic(r_u8, data, length, utf8_decoder(), latin1_writer());

        LUMEX_ASSERT(false && "Invalid encoding"); // unreachable
        return 0;
      }
#endif

      inline bool
      allow_insert_attribute(xml_node_type parent)
      {
        return parent == node_element || parent == node_declaration;
      }

      inline bool
      allow_insert_child(xml_node_type parent, xml_node_type child)
      {
        if(parent != node_document && parent != node_element) return false;
        if(child == node_document || child == node_null) return false;
        if(parent != node_document && (child == node_declaration || child == node_doctype)) return false;

        return true;
      }

      template <typename String, typename Header>
      inline void
      node_copy_string(String &dest, Header &header, uintptr_t header_mask, char_t *source, Header &source_header,
                       XmlAllocator *alloc)
      {
        LUMEX_ASSERT(!dest && (header & header_mask) == 0); // copies are performed into fresh nodes

        if(source)
        {
          if(alloc && (source_header & header_mask) == 0)
          {
            dest = source;

            // since strcpy_insitu can reuse document buffer memory we need to mark both source and dest as shared
            header |= kxml_memory_page_contents_shared_mask;
            source_header |= kxml_memory_page_contents_shared_mask;
          }
          else
          {
            // if strcpy_insitu fails (out of memory) we just leave the destination name/value empty
            (void)strcpy_insitu(dest, header, header_mask, source, strlength(source));
          }
        }
      }

      inline bool
      check_string_to_number_format(char_t const *string)
      {
        // parse leading whitespace
        while(LUMEX_XML_IS_CHARTYPE( // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
          *string, ct_space))
          ++string;

        // parse sign
        if(*string == '-') ++string;

        if(*string == 0) return false;

        // if there is no integer part, there should be a decimal part with at least one digit
        if(!LUMEX_XML_IS_CHARTYPEX( // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
             string[0], ctx_digit)
           && (string[0] != '.'
               || !LUMEX_XML_IS_CHARTYPEX( // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                 string[1], ctx_digit)))
          return false;

        // parse integer part
        while(LUMEX_XML_IS_CHARTYPEX( // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
          *string, ctx_digit))
          ++string;

        // parse decimal part
        if(*string == '.')
        {
          ++string;

          while(LUMEX_XML_IS_CHARTYPEX( // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
            *string, ctx_digit))
            ++string;
        }

        // parse trailing whitespace
        while(LUMEX_XML_IS_CHARTYPE( // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
          *string, ct_space))
          ++string;

        return *string == 0;
      }

      inline double
      convert_string_to_number(char_t const *string)
      {
        // check string format
        if(!check_string_to_number_format(string)) return gen_nan();

// parse string
#ifdef LUMEX_XML_WCHAR_MODE
        return wcstod(string, nullptr);
#else
        return strtod(string, nullptr);
#endif
      }

      inline bool
      convert_string_to_number_scratch(char_t // NOLINT(cppcoreguidelines-avoid-c-arrays, modernize-avoid-c-arrays)
                                       (&buffer)[32],
                                       char_t const *begin, char_t const *end, double *out_result)
      {
        auto length     = static_cast<size_t>(end - begin);
        char_t *scratch = buffer; // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay)

        if(length >= sizeof(buffer) / sizeof(buffer[0]))
        {
          // need to make dummy on-heap copy
          scratch = static_cast<char_t *>(          // NOLINT(cppcoreguidelines-owning-memory)
            malloc((length + 1) * sizeof(char_t))); // NOLINT(cppcoreguidelines-no-malloc)
          if(scratch == nullptr) return false;
        }

        // copy string to zero-terminated buffer and perform conversion
        memcpy(scratch, begin, length * sizeof(char_t));
        scratch[length] = 0;

        *out_result     = convert_string_to_number(scratch);

        // free dummy buffer
        if(scratch != buffer) // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
          free(scratch);      // NOLINT(cppcoreguidelines-owning-memory, cppcoreguidelines-no-malloc)

        return true;
      }

      inline double
      round_nearest(double value)
      {
        return floor(value + 0.5);
      }

      inline double
      round_nearest_nzero(double value)
      {
        // same as round_nearest, but returns -0 for [-0.5, -0]
        // ceil is used to differentiate between +0 and -0 (we return -0 for [-0.5, -0] and +0 for +0)
        return (value >= -0.5 && value <= 0) ? ceil(value) : floor(value + 0.5);
      }

      inline bool
      starts_with(char_t const *str, char_t const *pattern)
      {
        while((*pattern != 0) && *str == *pattern)
        {
          str++;
          pattern++;
        }

        return *pattern == 0;
      }

      inline char_t const *
      find_char(char_t const *str, char_t chr)
      {
#ifdef LUMEX_XML_WCHAR_MODE
        return wcschr(str, chr);
#else
        return strchr(str, chr);
#endif
      }

      inline char_t const *
      find_substring(char_t const *str, char_t const *pattern)
      {
#ifdef LUMEX_XML_WCHAR_MODE
        // MSVC6 wcsstr bug workaround (if s is empty it always returns 0)
        return (*pattern == 0) ? str : wcsstr(str, pattern);
#else
        return strstr(str, pattern);
#endif
      }

      // Converts symbol to lower case, if it is an ASCII one
      inline char_t
      tolower_ascii(char_t chr)
      {
        return static_cast<unsigned int>(chr - 'A') < 26 ? static_cast<char_t>(chr | ' ') : chr;
      }

      inline bool
      is_xpath_attribute(char_t const *name)
      {
        return !starts_with(name, LUMEX_XML_TEXT("xmlns")) || (name[5] != 0 && name[5] != ':');
      }

      inline bool
      convert_number_to_boolean(double value)
      {
        return (value != 0 && !is_nan(value));
      }

      inline void
      truncate_zeros(char const *begin, char *end)
      {
        while(begin != end && end[-1] == '0') end--;

        *end = 0;
      }

      inline char_t *
      normalize_space(char_t *buffer)
      {
        char_t *write = buffer;

        for(char_t *it = buffer; *it != 0;)
        {
          char_t chr = *it++;

          if(LUMEX_XML_IS_CHARTYPE(chr, ct_space)) // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
          {
            // replace whitespace sequence with single space
            while(LUMEX_XML_IS_CHARTYPE(*it, ct_space)) // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
              it++;

            // avoid leading spaces
            if(write != buffer) *write++ = ' ';
          }
          else
            *write++ = chr;
        }

        // remove trailing space
        if(write != buffer
           && LUMEX_XML_IS_CHARTYPE( // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
             write[-1], ct_space))
          write--;

        // zero-terminate
        *write = 0;

        return write;
      }

      inline char_t *
      translate(char_t *buffer, char_t const *from, char_t const *to_, size_t to_length)
      {
        char_t *write = buffer;

        while(*buffer != 0)
        {
          char_t volatile chr = *buffer++;

          char_t const *pos   = find_char(from, chr);

          if(pos == nullptr)
            *write++ = chr; // do not process
          else if(static_cast<size_t>(pos - from) < to_length)
            *write++ = to_[pos - from]; // replace
        }

        // zero-terminate
        *write = 0;

        return write;
      }

      inline unsigned char *
      translate_table_generate(XPathAllocator *alloc,
                               char_t const *from, // NOLINT(bugprone-easily-swappable-parameters)
                               char_t const *to_)
      {
        std::array<unsigned char, 128> table = {0};

        while(*from != 0)
        {
          auto fc_ = static_cast<unsigned int>(*from); // NOLINT(bugprone-signed-char-misuse)
          auto tc_ = static_cast<unsigned int>(*to_);  // NOLINT(bugprone-signed-char-misuse)

          if(fc_ >= 128 || tc_ >= 128) return nullptr;

          // code=128 means "skip character"
          if(table.at(fc_) == 0) table.at(fc_) = static_cast<unsigned char>((tc_ != 0) ? tc_ : 128);

          from++;
          if(tc_ != 0) to_++;
        }

        for(size_t i = 0; i < table.size(); ++i)
          if(table.at(i) == 0) table.at(i) = static_cast<unsigned char>(i);

        void *result = alloc->allocate(sizeof(table));
        if(result == nullptr) return nullptr;

        std::memcpy(result, table.data(), table.size());

        return static_cast<unsigned char *>(result);
      }

      inline char_t *
      translate_table(char_t *buffer, unsigned char const *table)
      {
        char_t *write = buffer;

        while(*buffer != 0)
        {
          char_t chr = *buffer++;
          auto index = static_cast<unsigned int>(chr); // NOLINT(bugprone-signed-char-misuse)

          if(index < 128)
          {
            unsigned char code = table[index];

            // code=128 means "skip character" (table size is 128 so 128 can be a special value)
            // this code skips these characters without extra branches
            *write = static_cast<char_t>(code);
            write += 1 - (code >> 7);
          }
          else { *write++ = chr; }
        }

        // zero-terminate
        *write = 0;

        return write;
      }

      inline char_t const *
      convert_number_to_string_special(double value)
      {
        double const volatile val = value;

        if(val == 0) return LUMEX_XML_TEXT("0");
        if(val != val) return LUMEX_XML_TEXT("NaN");
        if(val * 2 == val) return value > 0 ? LUMEX_XML_TEXT("Infinity") : LUMEX_XML_TEXT("-Infinity");
        return nullptr;
      }

      inline void
      convert_number_to_mantissa_exponent(double value,
                                          char // NOLINT(cppcoreguidelines-avoid-c-arrays, modernize-avoid-c-arrays)
                                          (&buffer)[32],
                                          char **out_mantissa, int *out_exponent)
      {
        // get a scientific notation value with IEEE DBL_DIG decimals
#ifdef LUMEX_XML_WCHAR_MODE
        swprintf( // NOLINT(cppcoreguidelines-pro-type-vararg)
          reinterpret_cast<wchar_t *>(buffer), 32, L"%.*e", DBL_DIG, value);
#else
        snprintf( // NOLINT(cppcoreguidelines-pro-type-vararg)
          buffer, // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
          32, "%.*e", DBL_DIG, value);
#endif

        // get the exponent (possibly negative)
        char *exponent_string = strchr(buffer, 'e'); // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
        LUMEX_ASSERT(exponent_string);

        int exponent = atoi(exponent_string + 1);

        // extract mantissa string: skip sign
        char *mantissa
          = buffer[0] == '-' ? buffer + 1 : buffer; // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
        LUMEX_ASSERT(mantissa[0] != '0' && (mantissa[1] == '.' || mantissa[1] == ','));

        // divide mantissa by 10 to eliminate integer part
        mantissa[1] = mantissa[0];
        mantissa++;
        exponent++;

        // remove extra mantissa digits and zero-terminate mantissa
        truncate_zeros(mantissa, exponent_string);

        // fill results
        *out_mantissa = mantissa;
        *out_exponent = exponent;
      }
    } // namespace Utility
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_UTILS_HPP
