#ifndef LUMEX_XML_UTILS_HPP
#define LUMEX_XML_UTILS_HPP

#include <array>
#include <cstdlib>

#include "lumex/core/utility/LumexMacros.hpp"

#include "lumex/xml/constants/LumexXmlConstants.hpp"
#include "lumex/xml/memory/LumexXmlAllocator.hpp"
#include "lumex/xml/types/LumexXmlTypes.hpp"
#include "lumex/xml/utility/LumexXmlMacros.hpp"

using namespace Lumex::Xml::Memory;
using namespace Lumex::Xml::Constants;
using namespace Lumex::Xml::Types;

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace Utility
    {
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
        return wcstod(value, NULL);
#else
        return strtod(value, NULL);
#endif
      }

      inline float
      get_value_float(char_t const *value)
      {
#ifdef LUMEX_XML_WCHAR_MODE
        return static_cast<float>(wcstod(value, NULL));
#else
        return static_cast<float>(strtod(value, NULL));
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
          xml_allocator_t *alloc = LUMEX_XML_GETPAGE_IMPL( // NOLINT(cppcoreguidelines-pro-type-const-cast)
                                     header)
                                     ->allocator;

          if(header & header_mask) alloc->deallocate_string(dest);

          dest = NULL;
          header &= ~header_mask;

          return true;
        }

        if(dest && strcpy_insitu_allow(source_length, header, header_mask, dest))
        {
          memcpy(dest, source, source_length * sizeof(char_t));
          dest[source_length] = 0;

          return true;
        }

        xml_allocator_t *alloc = LUMEX_XML_GETPAGE_IMPL( // NOLINT(cppcoreguidelines-pro-type-const-cast)
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
    } // namespace Utility
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_UTILS_HPP
