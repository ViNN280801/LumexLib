#ifndef LUMEX_XML_CLEANER_HPP
#define LUMEX_XML_CLEANER_HPP

#include "lumex/LumexExport.hpp"

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace Utility
    {
      /**
       * @brief A simple RAII (Resource Acquisition Is Initialization) wrapper for managing dynamically allocated
       * memory.
       * @details `XmlCleaner` ensures that a provided memory block is deallocated using a specified deleter
       *          when the `XmlCleaner` object goes out of scope, unless `release()` is called.
       *          This is particularly useful for managing memory that might be conditionally owned or passed
       *          between functions.
       * @tparam T The type of the pointer to be managed.
       */
      template <typename T> struct LUMEX_API XmlCleaner { // NOLINT(cppcoreguidelines-special-member-functions)
        /// @brief Type alias for the deleter function pointer.
        using D = void (*)(T *);

        /// @brief Pointer to the managed data.
        T *data{}; // NOLINT(misc-non-private-member-variables-in-classes)
        /// @brief Function pointer to the deleter (e.g., `free`, `delete[]`).
        D deleter{}; // NOLINT(misc-non-private-member-variables-in-classes)

        /**
         * @brief Constructs an `XmlCleaner` object, taking ownership of the provided data.
         * @param[in] data_ A pointer to the data to be managed.
         * @param[in] deleter_ A function pointer to the deleter responsible for freeing `data_`.
         */
        XmlCleaner(T *data_, D deleter_) : data(data_), deleter(deleter_) {}

        /**
         * @brief Destructor. Deallocates the managed data using the provided deleter if `release()` was not called.
         */
        ~XmlCleaner()
        {
          if(data) deleter(data);
        }

        /**
         * @brief Releases ownership of the managed data.
         * @return A pointer to the previously managed data. The caller is now responsible for its deallocation.
         * @details After calling `release()`, the `XmlCleaner` object will no longer deallocate the memory.
         */
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
