#ifndef LUMEX_XML_TEXT_HPP
#define LUMEX_XML_TEXT_HPP

#include "lumex/core/utility/LumexAttributes.hpp"

#include "lumex/xml/types/XmlTypes.hpp"

using namespace Lumex::Xml::Types;

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    // Forward declaration
    namespace Node
    {
      class XmlNode;
      struct XmlNodeBase;
    }
    using namespace Node;

    namespace Text
    {
      class XmlText
      {
        friend class Node::XmlNode;

      public:
        using unspecified_bool_type = void (*)(XmlText ***);

        // Default constructor. Constructs an empty object.
        XmlText();

        // Safe bool conversion operator
        operator unspecified_bool_type() const;

        // Borland C++ workaround
        bool operator!() const;

        // Check if text object is empty (null)
        LUMEX_ATTRIBUTE_NODISCARD("The returned boolean indicates whether the text object is empty; discarding it "
                                  "negates the purpose of the getter.")
        bool empty() const;

        // Get text, or "" if object is empty
        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned C-style string text should be used; discarding it negates the purpose of the getter.")
        char_t const *get() const;

        // Get text, or the default value if object is empty
        char_t const *as_string(char_t const *def = LUMEX_XML_TEXT("")) const;

        // Get text as a number, or the default value if conversion did not succeed or object is empty
        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned integer value should be used; discarding it negates the purpose of the getter.")
        int as_int(int def = 0) const;

        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned unsigned integer value should be used; discarding it negates the purpose of the getter.")
        unsigned int as_uint(unsigned int def = 0) const;

        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned double value should be used; discarding it negates the purpose of the getter.")
        double as_double(double def = 0) const;

        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned float value should be used; discarding it negates the purpose of the getter.")
        float as_float(float def = 0) const;

        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned long long value should be used; discarding it negates the purpose of the getter.")
        long long as_llong(long long def = 0) const;

        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned unsigned long long value should be used; discarding it negates the purpose of the getter.")
        unsigned long long as_ullong(unsigned long long def = 0) const;

        // Get text as bool (returns true if first character is in '1tTyY' set), or the default value if object is empty
        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned boolean value should be used; discarding it negates the purpose of the getter.")
        bool as_bool(bool def = false) const;

        // Set text (returns false if object is empty or there is not enough memory)
        bool set(char_t const *rhs);
        bool set(char_t const *rhs, size_t size);
#if __cplusplus >= 201703L
        bool set(string_view_t rhs);
#endif

        // Set text with type conversion (numbers are converted to strings, boolean is converted to "true"/"false")
        bool set(int rhs);
        bool set(unsigned int rhs);
        bool set(long rhs);
        bool set(unsigned long rhs);
        bool set(double rhs);
        bool set(double rhs, int precision);
        bool set(float rhs);
        bool set(float rhs, int precision);
        bool set(bool rhs);

        bool set(long long rhs);
        bool set(unsigned long long rhs);

        // Set text (equivalent to set without error checking)
        XmlText &operator=(char_t const *rhs);
        XmlText &operator=(int rhs);
        XmlText &operator=(unsigned int rhs);
        XmlText &operator=(long rhs);
        XmlText &operator=(unsigned long rhs);
        XmlText &operator=(double rhs);
        XmlText &operator=(float rhs);
        XmlText &operator=(bool rhs);

#if __cplusplus >= 201703L
        XmlText &operator=(string_view_t rhs);
#endif

        XmlText &operator=(long long rhs);
        XmlText &operator=(unsigned long long rhs);

        // Get the data node (node_pcdata or node_cdata) for this object
        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned data node should be used; discarding it negates the purpose of the getter.")
        XmlNode data() const;

      private:
        explicit XmlText(XmlNodeBase *root);

        XmlNodeBase *m_root;

        XmlNodeBase *_data_new();

        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned internal pointer should be used; discarding it negates the purpose of the getter.")
        XmlNodeBase *_data() const;
      };

      bool operator&&(XmlText const &lhs, bool rhs);

      bool operator||(XmlText const &lhs, bool rhs);
    } // namespace Text
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_TEXT_HPP
