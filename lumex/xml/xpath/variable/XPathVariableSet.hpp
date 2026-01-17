#ifndef LUMEX_XML_XPATH_VARIABLE_SET_HPP
#define LUMEX_XML_XPATH_VARIABLE_SET_HPP

#include "lumex/LumexExport.hpp"

#include "lumex/xml/xpath/node/XPathNodeSet.hpp"

#include "XPathVariable.hpp"

using namespace Lumex::Xml::XPath::Node;

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Xml
  {
    namespace XPath
    {
      namespace Variable
      {
        /**
         * @brief Manages a collection of XPath variables.
         * @details This class provides a container for `XPathVariable` objects, allowing
         *          them to be added, retrieved, and their values set by name. It uses
         *          a hash-table-like approach with an array of linked lists (`_data`)
         *          to store variables, enabling efficient lookups.
         *
         * @note The `XPathVariableSet` is responsible for the lifetime of the `XPathVariable`
         *       objects it contains, allocating and deallocating them as needed. It is
         *       designed with value semantics (copy and move constructors/assignments)
         *       to allow easy passing and management.
         * @warning The maximum number of variables is fixed by `_max_variables`. Adding more
         *          variables than this limit will not cause a crash but will lead to collisions
         *          and performance degradation without increasing storage.
         */
        class LUMEX_API XPathVariableSet // NOLINT(cppcoreguidelines-special-member-functions)
        {
        public:
          /**
           * @brief Default constructor.
           * @details Initializes an empty `XPathVariableSet`, setting all internal
           *          variable pointers to `nullptr`.
           */
          XPathVariableSet();
          /**
           * @brief Destructor.
           * @details Iterates through all stored variables and deallocates their memory,
           *          ensuring no memory leaks.
           */
          ~XPathVariableSet();

          /**
           * @brief Copy constructor.
           * @details Creates a new `XPathVariableSet` by performing a deep copy of all
           *          variables from `rhs`. This involves allocating new memory for each variable
           *          and copying its value.
           * @param rhs The `XPathVariableSet` to copy from.
           * @warning Can be expensive for sets with many variables, especially large string or node-set variables.
           */
          XPathVariableSet(XPathVariableSet const &rhs);
          /**
           * @brief Copy assignment operator.
           * @details Assigns the contents of `rhs` to this `XPathVariableSet` by performing a deep copy.
           *          Any existing variables in this set are destroyed first.
           * @param rhs The `XPathVariableSet` to copy from.
           * @return A reference to this `XPathVariableSet` after the assignment.
           * @warning Can be expensive due to deep copying and destruction of existing variables.
           */
          XPathVariableSet &operator=(XPathVariableSet const &rhs);

          /**
           * @brief Move constructor.
           * @details Efficiently transfers ownership of variables from `rhs` to this new set,
           *          leaving `rhs` in an empty state. No deep copies are performed.
           * @param rhs The `XPathVariableSet` to move from.
           */
          XPathVariableSet(XPathVariableSet &&rhs) noexcept;
          /**
           * @brief Move assignment operator.
           * @details Efficiently transfers ownership of variables from `rhs` to this set.
           *          Any existing variables in this set are destroyed first. `rhs` is left
           *          in an empty state.
           * @param rhs The `XPathVariableSet` to move from.
           * @return A reference to this `XPathVariableSet` after the assignment.
           */
          XPathVariableSet &operator=(XPathVariableSet &&rhs) noexcept;

          /**
           * @brief Adds a new variable to the set or retrieves an existing one.
           * @details This function first checks if a variable with `name` already exists.
           *          If it does and its `type` matches, the existing variable is returned.
           *          Otherwise, a new variable of the specified `type` and `name` is created
           *          and added to the set.
           * @param name The null-terminated C-style string name of the variable.
           * @param type The desired `xpath_value_type` of the variable.
           * @return A pointer to the newly added or existing `XPathVariable`, or `nullptr`
           *         if creation fails (e.g., out of memory) or if a variable with the same
           *         name but a different type already exists.
           * @warning The caller should check the return value for `nullptr`.
           */
          XPathVariable *add(char_t const *name, xpath_value_type type);

          /**
           * @brief Sets the boolean value of an existing variable.
           * @details Finds a variable by `name`. If found and its type is `xpath_type_boolean`,
           *          its value is updated. No type conversion is performed.
           * @param name The name of the variable.
           * @param value The boolean value to set.
           * @return `true` if the variable was found and its value set, `false` otherwise
           *         (e.g., variable not found or type mismatch).
           */
          bool set(char_t const *name, bool value);
          /**
           * @brief Sets the numeric value of an existing variable.
           * @details Finds a variable by `name`. If found and its type is `xpath_type_number`,
           *          its value is updated. No type conversion is performed.
           * @param name The name of the variable.
           * @param value The double value to set.
           * @return `true` if the variable was found and its value set, `false` otherwise.
           */
          bool set(char_t const *name, double value);
          /**
           * @brief Sets the string value of an existing variable.
           * @details Finds a variable by `name`. If found and its type is `xpath_type_string`,
           *          its value is updated with a copy of the provided string.
           * @param name The name of the variable.
           * @param value The null-terminated C-style string value to set.
           * @return `true` if the variable was found and its value set, `false` otherwise.
           */
          bool set(char_t const *name, char_t const *value);
          /**
           * @brief Sets the node-set value of an existing variable.
           * @details Finds a variable by `name`. If found and its type is `xpath_type_node_set`,
           *          its value is updated with a copy of the provided node set.
           * @param name The name of the variable.
           * @param value The `XPathNodeSet` value to set.
           * @return `true` if the variable was found and its value set, `false` otherwise.
           */
          bool set(char_t const *name, XPathNodeSet const &value);

          /**
           * @brief Gets a mutable pointer to an existing variable by name.
           * @param name The name of the variable to retrieve.
           * @return A mutable pointer to the `XPathVariable` if found, `nullptr` otherwise.
           */
          XPathVariable *get(char_t const *name);
          /**
           * @brief Gets a constant pointer to an existing variable by name.
           * @param name The name of the variable to retrieve.
           * @return A constant pointer to the `XPathVariable` if found, `nullptr` otherwise.
           */
          XPathVariable const *get(char_t const *name) const;

        private:
          /// @brief The maximum number of variables supported in the hash table.
          constexpr static size_t _max_variables = 64;
          /// @brief An array of pointers to `XPathVariable` objects, serving as hash buckets.
          std::array<XPathVariable *, _max_variables> _data{};

          /**
           * @brief Internal helper to assign variables from another set.
           * @details Performs a deep copy of variables from `rhs` into this set. Used by
           *          copy constructor and copy assignment operator.
           * @param rhs The `XPathVariableSet` to assign from.
           * @return `void` (returns early if allocation fails).
           */
          void _assign(XPathVariableSet const &rhs);
          /**
           * @brief Internal helper to swap the contents of two `XPathVariableSet` objects.
           * @details Swaps the internal `_data` arrays with `rhs`, effectively exchanging
           *          ownership of all variables.
           * @param rhs The `XPathVariableSet` to swap with.
           */
          void _swap(XPathVariableSet &rhs);

          /**
           * @brief Internal helper to find a variable by name.
           * @details Locates an `XPathVariable` in the internal hash table by its name.
           * @param name The null-terminated C-style string name of the variable to find.
           * @return A mutable pointer to the found `XPathVariable`, or `nullptr` if not found.
           */
          XPathVariable *_find(char_t const *name) const;

          /**
           * @brief Internal helper to clone an `XPathVariable` and its chain.
           * @details Recursively clones a chain of `XPathVariable` objects, creating new
           *          memory for each cloned variable and copying its value.
           * @param var The head of the `XPathVariable` chain to clone.
           * @param out_result A pointer to a `XPathVariable*` that will receive the head
           *                   of the newly cloned chain.
           * @return `true` if cloning was successful, `false` if any memory allocation failed.
           */
          static bool _clone(XPathVariable *var, XPathVariable **out_result);
          /**
           * @brief Internal helper to destroy an `XPathVariable` chain.
           * @details Recursively deallocates memory for a chain of `XPathVariable` objects.
           * @param var The head of the `XPathVariable` chain to destroy.
           */
          static void _destroy(XPathVariable *var);
        };

        /**
         * @brief Utility function to retrieve a variable from a set, handling scratch buffer for name.
         * @details This function takes a variable name that might not be null-terminated (given by `begin` and `end`),
         *          copies it into a scratch buffer (or allocates temporary heap memory if needed),
         *          and then looks up the variable in the provided `XPathVariableSet`.
         * @param buffer A fixed-size character array used as a scratch buffer for the variable name.
         * @param set A pointer to the `XPathVariableSet` to search in.
         * @param begin A pointer to the beginning of the variable name in the query string.
         * @param end A pointer to one past the end of the variable name in the query string.
         * @param out_result A pointer to an `XPathVariable*` that will receive the found variable, or `nullptr`.
         * @return `true` if the operation was successful (including memory allocation for scratch buffer),
         *         `false` if memory allocation failed. The `out_result` should be checked to see if the variable was
         * actually found.
         * @warning `buffer` must be large enough to hold `(end - begin) + 1` characters. If not, a heap allocation
         * occurs.
         */
        inline bool
        get_variable_scratch(char_t (&buffer)[32], // NOLINT(cppcoreguidelines-avoid-c-arrays, modernize-avoid-c-arrays)
                             XPathVariableSet *set, char_t const *begin, char_t const *end, XPathVariable **out_result)
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

          // copy string to zero-terminated buffer and perform lookup
          memcpy(scratch, begin, length * sizeof(char_t));
          scratch[length] = 0;

          *out_result     = set->get(scratch);

          // free dummy buffer
          if(scratch != buffer) // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
            free(scratch);      // NOLINT(cppcoreguidelines-owning-memory, cppcoreguidelines-no-malloc)

          return true;
        }
      } // namespace Variable
    } // namespace XPath
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_XPATH_VARIABLE_SET_HPP
