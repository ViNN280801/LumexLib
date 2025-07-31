#ifndef LUMEX_XML_CLEANER_HPP
#define LUMEX_XML_CLEANER_HPP

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace Utility
    {
      template <typename T> struct LumexXmlCleaner { // NOLINT(cppcoreguidelines-special-member-functions)
        using D = void (*)(T *);

        T *data{};   // NOLINT(misc-non-private-member-variables-in-classes)
        D deleter{}; // NOLINT(misc-non-private-member-variables-in-classes)

        LumexXmlCleaner(T *data_, D deleter_) : data(data_), deleter(deleter_) {}

        ~LumexXmlCleaner()
        {
          if(data) deleter(data);
        }

        T *
        release()
        {
          T *result = data;
          data      = nullptr;
          return result;
        }
      };
    } // namespace Utility
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_CLEANER_HPP
