#ifndef LUMEX_XML_ATTRIBUTE_HPP
#define LUMEX_XML_ATTRIBUTE_HPP

#if __cplusplus >= 201703L
  #include <string_view>
#endif

#include "lumex/core/utility/LumexAttributes.hpp"

#include "lumex/xml/utility/XmlMacros.hpp"

#include "XmlAttributeBase.hpp"

using namespace Lumex::Xml::Types;

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace Attribute
    {
      class XmlAttribute
      {
        friend class XmlAttributeIterator;
        friend class LumexXmlNode;

      public:
        using unspecified_bool_type = void (*)(XmlAttribute ***);

        // Default constructor. Constructs an empty attribute.
        XmlAttribute();

        // Constructs attribute from internal pointer
        explicit XmlAttribute(XmlAttributeBase *attr);

        // Safe bool conversion operator
        operator unspecified_bool_type() const;

        bool operator!() const;

        // Comparison operators (compares wrapped attribute pointers)
        bool operator==(XmlAttribute const &other) const;

        bool operator!=(XmlAttribute const &other) const;

        bool operator<(XmlAttribute const &other) const;

        bool operator>(XmlAttribute const &other) const;

        bool operator<=(XmlAttribute const &other) const;

        bool operator>=(XmlAttribute const &other) const;

        // Check if attribute is empty (null)
        LUMEX_ATTRIBUTE_NODISCARD("The returned boolean indicates whether the attribute is empty; discarding it "
                                  "negates the purpose of the getter")
        bool empty() const;

        // Get attribute name/value, or "" if attribute is empty
        LUMEX_ATTRIBUTE_NODISCARD("The returned attribute name (C-style string) should be used; discarding it "
                                  "negates the purpose of the getter")
        char_t const *name() const;

        LUMEX_ATTRIBUTE_NODISCARD("The returned attribute value (C-style string) should be used; discarding it "
                                  "negates the purpose of the getter")
        char_t const *value() const;

        // Get attribute value, or the default value if attribute is empty
        char_t const *as_string(char_t const *def = LUMEX_XML_TEXT("")) const;

        // Get attribute value as a number, or the default value if conversion did not succeed or attribute is empty
        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned value is the default value; discarding it negates the purpose of the getter")
        int as_int(int def = 0) const;

        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned value is the default value; discarding it negates the purpose of the getter")
        unsigned int as_uint(unsigned int def = 0) const;

        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned value is the default value; discarding it negates the purpose of the getter")
        double as_double(double def = 0) const;

        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned value is the default value; discarding it negates the purpose of the getter")
        float as_float(float def = 0) const;

        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned value is the default value; discarding it negates the purpose of the getter")
        long long as_llong(long long def = 0) const;

        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned value is the default value; discarding it negates the purpose of the getter")
        unsigned long long as_ullong(unsigned long long def = 0) const;

        // Get attribute value as bool (returns true if first character is in '1tTyY' set), or the default value if
        // attribute is empty
        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned value is the default value; discarding it negates the purpose of the getter")
        bool as_bool(bool def = false) const;

        // Set attribute name/value (returns false if attribute is empty or there is not enough memory)
        bool set_name(char_t const *rhs);

        bool set_name(char_t const *rhs, size_t size);

#if __cplusplus >= 201703L
        bool set_name(std::string_view rhs);
#endif
        bool set_value(const char_t *rhs);

        bool set_value(char_t const *rhs, size_t size);

#if __cplusplus >= 201703L
        bool set_value(std::string_view rhs);
#endif

        // Set attribute value with type conversion (numbers are converted to strings, boolean is converted to
        // "true"/"false")
        bool set_value(int rhs);

        bool set_value(unsigned int rhs);

        bool set_value(long rhs);

        bool set_value(unsigned long rhs);

        bool set_value(double rhs);

        bool set_value(double rhs, int precision);

        bool set_value(float rhs);

        bool set_value(float rhs, int precision);

        bool set_value(bool rhs);

        bool set_value(long long rhs);

        bool set_value(unsigned long long rhs);

        // Set attribute value (equivalent to set_value without error checking)
        XmlAttribute &operator=(char_t const *rhs);

        XmlAttribute &operator=(int rhs);

        XmlAttribute &operator=(unsigned int rhs);

        XmlAttribute &operator=(long rhs);

        XmlAttribute &operator=(unsigned long rhs);

        XmlAttribute &operator=(double rhs);

        XmlAttribute &operator=(float rhs);

        XmlAttribute &operator=(bool rhs);

#if __cplusplus >= 201703L
        XmlAttribute &operator=(std::string_view rhs);
#endif

        XmlAttribute &operator=(long long rhs);

        XmlAttribute &operator=(unsigned long long rhs);

        // Get next/previous attribute in the attribute list of the parent node
        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned attribute should be used; discarding it negates the purpose of the getter")
        XmlAttribute next_attribute() const;

        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned attribute should be used; discarding it negates the purpose of the getter")
        XmlAttribute previous_attribute() const;

        // Get hash value (unique for handles to the same object)
        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned hash value should be used; discarding it negates the purpose of the getter")
        size_t hash_value() const;

        // Get internal pointer
        LUMEX_ATTRIBUTE_NODISCARD(
          "The returned pointer should be used; discarding it negates the purpose of the getter")
        XmlAttributeBase *get() const;

        void set(XmlAttributeBase *attr);

      private:
        XmlAttributeBase *m_attr;
      };
    } // namespace Attribute
    namespace Utility
    {
      bool is_attribute_of(Attribute::XmlAttributeBase *attr, Node::XmlNodeBase *node);
    } // namespace Utility
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_ATTRIBUTE_HPP
