#ifndef LUMEX_XML_XPATH_STACK_HPP
#define LUMEX_XML_XPATH_STACK_HPP

#include "lumex/LumexExport.hpp"

#include "XPathAllocator.hpp"

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace XPath
    {
      namespace Memory
      {
        /**
         * @brief Represents a pair of XPath allocators for managing expression evaluation memory.
         * @details This structure groups two `XPathAllocator` instances: `result` for storing
         *          final evaluation results and `temp` for temporary allocations that can be
         *          freed frequently during intermediate steps. This separation helps optimize
         *          memory usage during complex XPath evaluations.
         *
         * @note This struct itself does not manage the lifetime of the `XPathAllocator`
         *       objects it points to; it merely holds references. The actual allocators
         *       are typically owned by `XPathStackData`.
         */
        struct LUMEX_API XPathStack {
          /// @brief Pointer to the allocator used for storing final XPath expression results.
          XPathAllocator *result;
          /// @brief Pointer to the allocator used for temporary memory during XPath evaluation.
          XPathAllocator *temp;
        };

        /**
         * @brief Manages the memory for XPath evaluation, providing dedicated allocators.
         * @details This class is responsible for setting up and managing the lifetime of
         *          two `XPathAllocator` instances: one for persistent results (`result`)
         *          and one for temporary scratch space (`temp`). It pre-allocates two
         *          `XPathMemoryBlock`s for these allocators and initializes their states.
         *          The `oom` flag indicates if an out-of-memory condition occurred during allocations.
         *
         * @note This class utilizes RAII for managing its internal `XPathAllocator`s.
         * @warning The `blocks` array, `result`, and `temp` allocators, `stack` member,
         *          and `oom` flag are public for direct access, which deviates from
         *          strict encapsulation but is typical in high-performance memory management
         *          components where direct access is needed for efficiency.
         */
        struct LUMEX_API XPathStackData { // NOLINT(cppcoreguidelines-special-member-functions)
          /// @brief An array containing two `XPathMemoryBlock`s used by the `result` and `temp` allocators.
          std::array<XPathMemoryBlock, 2> blocks{}; // NOLINT(misc-non-private-member-variables-in-classes)
          /// @brief The allocator for persistent XPath evaluation results.
          XPathAllocator result; // NOLINT(misc-non-private-member-variables-in-classes)
          /// @brief The allocator for temporary XPath evaluation data.
          XPathAllocator temp; // NOLINT(misc-non-private-member-variables-in-classes)
          /// @brief A nested `XPathStack` structure holding pointers to `result` and `temp` allocators.
          XPathStack stack{}; // NOLINT(misc-non-private-member-variables-in-classes)
          /// @brief A boolean flag indicating if an out-of-memory condition occurred during allocation.
          bool oom{}; // NOLINT(misc-non-private-member-variables-in-classes)

          /**
           * @brief Constructs an XPathStackData object.
           * @details Initializes the two internal `XPathMemoryBlock`s, sets up the `result`
           *          and `temp` `XPathAllocator`s to use these blocks, and links the allocators
           *          to the `oom` flag for error reporting.
           */
          XPathStackData();
          /**
           * @brief Destroys the XPathStackData object.
           * @details Releases all memory managed by the `result` and `temp` allocators,
           *          ensuring proper cleanup of all allocated XPath data.
           */
          ~XPathStackData();
        };
      } // namespace Memory
    } // namespace XPath
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_XPATH_STACK_HPP
