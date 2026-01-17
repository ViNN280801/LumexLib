#ifndef LUMEX_XML_XPATH_VARIABLE_HPP
#define LUMEX_XML_XPATH_VARIABLE_HPP

#include "lumex/LumexExport.hpp"

#include "lumex/core/utility/LumexAttributes.hpp"

#include "lumex/xml/types/XmlTypes.hpp"
#include "lumex/xml/utility/XmlUtils.hpp"

#include "lumex/xml/xpath/node/XPathNodeSet.hpp"

using namespace Lumex::Xml::Types;
using namespace Lumex::Xml::Utility;
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
         * @brief Base class for all XPath variables.
         * @details This abstract base class defines the interface for XPath variables,
         *          allowing retrieval and setting of their values regardless of their
         *          specific XPath type (boolean, number, string, node-set). It also
         *          provides functionality for chaining variables in a linked list,
         *          which is used by `XPathVariableSet`.
         *
         * @note This class does not handle memory management for string values or node sets directly
         *       when setting values; it expects the underlying variable-specific structures
         *       (e.g., `xpath_variable_string`) to manage that. It is designed with
         *       non-copyable semantics to prevent accidental deep copies of potentially large data.
         */
        class LUMEX_API XPathVariable // NOLINT(cppcoreguidelines-special-member-functions)
        {
          friend class xpath_variable_set;
          friend struct xpath_variable_boolean;
          friend struct xpath_variable_number;
          friend struct xpath_variable_string;
          friend struct xpath_variable_node_set;

        public:
          /**
           * @brief Retrieves the name of the XPath variable.
           * @details This function provides access to the null-terminated C-style
           *          string representing the name of the variable.
           * @return A `char_t const*` pointing to the variable's name. The pointer
           *         is valid for the lifetime of the `XPathVariable` object.
           * @throws `LUMEX_ASSERT` if the variable type is invalid.
           */
          // The returned value is the same as the one returned by the name() method;
          // discarding it negates the purpose of the getter.
          LUMEX_ATTRIBUTE_NODISCARD("The returned value is the same as the one returned by the name() method; "
                                    "discarding it negates the purpose of the getter.")
          char_t const *name() const;

          /**
           * @brief Retrieves the XPath value type of the variable.
           * @details This function returns the specific XPath data type that this
           *          variable instance is designed to hold (e.g., `xpath_type_boolean`,
           *          `xpath_type_string`).
           * @return An `xpath_value_type` enumeration value.
           */
          // The returned value is the same as the one returned by the type() method;
          // discarding it negates the purpose of the getter.
          LUMEX_ATTRIBUTE_NODISCARD("The returned value is the same as the one returned by the type() method; "
                                    "discarding it negates the purpose of the getter.")
          xpath_value_type type() const;

          /**
           * @brief Gets the boolean value of the variable.
           * @details If the variable's actual type is `xpath_type_boolean`, its value is returned.
           *          Otherwise, a default value (`false`) is returned without type conversion.
           * @return The boolean value if the type matches, `false` otherwise.
           */
          // The returned value is the same as the one returned by the get_boolean() method;
          // discarding it negates the purpose of the getter.
          LUMEX_ATTRIBUTE_NODISCARD("The returned value is the same as the one returned by the get_boolean() method; "
                                    "discarding it negates the purpose of the getter.")
          bool get_boolean() const;

          /**
           * @brief Gets the numeric value of the variable.
           * @details If the variable's actual type is `xpath_type_number`, its value is returned.
           *          Otherwise, a default value (NaN) is returned without type conversion.
           * @return The double-precision floating-point value if the type matches, NaN otherwise.
           */
          LUMEX_ATTRIBUTE_NODISCARD("The returned value is the same as the one returned by the get_number() method; "
                                    "discarding it negates the purpose of the getter.")
          double get_number() const;

          /**
           * @brief Gets the string value of the variable.
           * @details If the variable's actual type is `xpath_type_string`, its value is returned.
           *          Otherwise, an empty string (`LUMEX_XML_TEXT("")`) is returned without type conversion.
           * @return A `char_t const*` pointing to the string value if the type matches, or an empty
           *         string literal otherwise. The pointer is valid for the lifetime of the variable.
           */
          LUMEX_ATTRIBUTE_NODISCARD("The returned value is the same as the one returned by the get_string() method; "
                                    "discarding it negates the purpose of the getter.")
          char_t const *get_string() const;

          /**
           * @brief Gets the node-set value of the variable.
           * @details If the variable's actual type is `xpath_type_node_set`, its value is returned.
           *          Otherwise, an empty `XPathNodeSet` is returned without type conversion.
           * @return A constant reference to the `XPathNodeSet` value if the type matches, or a
           *         reference to a dummy empty node set otherwise.
           */
          LUMEX_ATTRIBUTE_NODISCARD("The returned value is the same as the one returned by the get_node_set() method; "
                                    "discarding it negates the purpose of the getter.")
          XPathNodeSet const &get_node_set() const;

          /**
           * @brief Sets the boolean value of the variable.
           * @details Attempts to set the variable's value to the provided boolean. This operation
           *          only succeeds if the variable's type is `xpath_type_boolean`. No type
           *          conversion is performed.
           * @param value The boolean value to set.
           * @return `true` if the value was set (type matched), `false` otherwise.
           */
          bool set(bool value);

          /**
           * @brief Sets the numeric value of the variable.
           * @details Attempts to set the variable's value to the provided double. This operation
           *          only succeeds if the variable's type is `xpath_type_number`. No type
           *          conversion is performed.
           * @param value The double value to set.
           * @return `true` if the value was set (type matched), `false` otherwise.
           */
          bool set(double value);

          /**
           * @brief Sets the string value of the variable.
           * @details Attempts to set the variable's value to a copy of the provided string.
           *          This operation only succeeds if the variable's type is `xpath_type_string`.
           *          A heap allocation is performed to store a copy of the string.
           * @param value A null-terminated C-style string to copy as the variable's value.
           * @return `true` if the value was set (type matched and allocation successful), `false` otherwise.
           * @note The variable takes ownership of the copied string data.
           */
          bool set(char_t const *value);

          /**
           * @brief Sets the node-set value of the variable.
           * @details Attempts to set the variable's value to the provided `XPathNodeSet`.
           *          This operation only succeeds if the variable's type is `xpath_type_node_set`.
           * @param value The `XPathNodeSet` to set as the variable's value.
           * @return `true` if the value was set (type matched), `false` otherwise.
           * @note The `XPathNodeSet` is copied by value, meaning the variable takes its own copy
           *       of the node set's internal data.
           */
          bool set(XPathNodeSet const &value);

          /**
           * @brief Gets a pointer to the next variable in a linked list.
           * @details This function is used to traverse a chain of `XPathVariable` objects,
           *          typically managed by an `XPathVariableSet`.
           * @return A pointer to the next `XPathVariable` in the list, or `nullptr` if this is the last one.
           */
          // The returned value is the same as the one returned by the next() method;
          // discarding it negates the purpose of the getter.
          LUMEX_ATTRIBUTE_NODISCARD("The returned value is the same as the one returned by the next() method; "
                                    "discarding it negates the purpose of the getter.")
          XPathVariable *
          next() const
          {
            return m_next;
          }

          /**
           * @brief Sets the next variable in the linked list.
           * @param next A pointer to the `XPathVariable` that should follow this one in the list.
           */
          void
          set_next(XPathVariable *next)
          {
            m_next = next;
          }

        private:
          /// @brief The XPath value type this variable holds.
          Types::xpath_value_type m_type;
          /// @brief Pointer to the next variable in a linked list (used by `XPathVariableSet`).
          XPathVariable *m_next;

          /**
           * @brief Private constructor for base `XPathVariable`.
           * @details Initializes the variable with its type. Used by derived variable types.
           * @param type The `xpath_value_type` of this variable.
           */
          XPathVariable(xpath_value_type type);

          // Non-copyable semantics:
          // Disable copy constructor and assignment operator to prevent unintended copying
          // of base class members and to enforce management via factory functions.
          XPathVariable(XPathVariable const &);
          XPathVariable &operator=(XPathVariable const &);
        };

        /**
         * @brief Represents an XPath variable holding a boolean value.
         * @details This concrete `XPathVariable` specialization stores a boolean value.
         *          It includes a small inline buffer for its name to avoid extra allocations.
         */
        struct xpath_variable_boolean : XPathVariable {
          /**
           * @brief Constructs a boolean XPath variable.
           * @details Initializes the base `XPathVariable` with `xpath_type_boolean`.
           */
          xpath_variable_boolean() : XPathVariable(xpath_type_boolean) {}

          /// @brief The boolean value of the variable.
          bool value{}; // NOLINT(misc-non-private-member-variables-in-classes)
          /// @brief Inline buffer for the variable name.
          char_t name[1]{}; // NOLINT(misc-non-private-member-variables-in-classes, cppcoreguidelines-avoid-c-arrays,
                            // modernize-avoid-c-arrays)
        };

        /**
         * @brief Represents an XPath variable holding a numeric (double) value.
         * @details This concrete `XPathVariable` specialization stores a double-precision
         *          floating-point number. It includes a small inline buffer for its name.
         */
        struct xpath_variable_number : XPathVariable {
          /**
           * @brief Constructs a numeric XPath variable.
           * @details Initializes the base `XPathVariable` with `xpath_type_number`.
           */
          xpath_variable_number() : XPathVariable(xpath_type_number) {}

          /// @brief The double-precision floating-point value of the variable.
          double value{}; // NOLINT(misc-non-private-member-variables-in-classes)
          /// @brief Inline buffer for the variable name.
          char_t name[1]{}; // NOLINT(misc-non-private-member-variables-in-classes, cppcoreguidelines-avoid-c-arrays,
          // modernize-avoid-c-arrays)
        };

        /**
         * @brief Represents an XPath variable holding a string value.
         * @details This concrete `XPathVariable` specialization stores a string value.
         *          It manages its own dynamically allocated memory for the string content
         *          and includes a small inline buffer for its name.
         */
        struct xpath_variable_string : XPathVariable { // NOLINT(cppcoreguidelines-special-member-functions)
          /**
           * @brief Constructs a string XPath variable.
           * @details Initializes the base `XPathVariable` with `xpath_type_string`.
           */
          xpath_variable_string() : XPathVariable(xpath_type_string) {}

          /**
           * @brief Destroys the string XPath variable.
           * @details Frees the dynamically allocated memory for the string value if it exists.
           */
          ~xpath_variable_string()
          {
            if(value != nullptr) free(value); // NOLINT(cppcoreguidelines-owning-memory, cppcoreguidelines-no-malloc)
          }

          /// @brief Pointer to the dynamically allocated string value.
          char_t *value{}; // NOLINT(misc-non-private-member-variables-in-classes)
          /// @brief Inline buffer for the variable name.
          char_t name[1]{}; // NOLINT(misc-non-private-member-variables-in-classes, cppcoreguidelines-avoid-c-arrays,
          // modernize-avoid-c-arrays)
        };

        /**
         * @brief Represents an XPath variable holding a node-set value.
         * @details This concrete `XPathVariable` specialization stores an `XPathNodeSet`.
         *          It includes a small inline buffer for its name.
         */
        struct xpath_variable_node_set : XPathVariable {
          /**
           * @brief Constructs a node-set XPath variable.
           * @details Initializes the base `XPathVariable` with `xpath_type_node_set`.
           */
          xpath_variable_node_set() : XPathVariable(xpath_type_node_set) {}

          /// @brief The `XPathNodeSet` value of the variable.
          XPathNodeSet value; // NOLINT(misc-non-private-member-variables-in-classes)
          /// @brief Inline buffer for the variable name.
          char_t name[1]{}; // NOLINT(misc-non-private-member-variables-in-classes, cppcoreguidelines-avoid-c-arrays,
          // modernize-avoid-c-arrays)
        };

        /**
         * @brief Factory function to create a new XPath variable of a specific type.
         * @details This templated function allocates memory for a new `XPathVariable`
         *          instance (of a derived type like `xpath_variable_boolean`, `_number`,
         *          `_string`, or `_node_set`) and initializes its name.
         * @tparam T The specific derived `XPathVariable` type to create.
         * @param name The null-terminated C-style string name of the variable.
         * @return A pointer to the newly created variable on success, or `nullptr` if
         *         memory allocation fails or if `name` is empty.
         * @warning The caller is responsible for freeing the allocated memory using
         *          `delete_xpath_variable`.
         */
        template <typename T>
        LUMEX_API inline T *
        new_xpath_variable(char_t const *name)
        {
          size_t length = Utility::strlength(name);
          if(length == 0) return nullptr; // empty variable names are invalid

          // $$ we can't use offsetof(T, name) because T is non-POD, so we just allocate additional length characters
          void *memory         // NOLINT(cppcoreguidelines-owning-memory)
            = malloc(sizeof(T) // NOLINT(cppcoreguidelines-no-malloc)
                     + (length * sizeof(char_t)));
          if(!memory) return nullptr;

          T *result = new(memory) T(); // NOLINT(cppcoreguidelines-owning-memory)

          std::memcpy(result->name, // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
                      name, (length + 1) * sizeof(char_t));

          return result;
        }

        /**
         * @brief Factory function to create a new XPath variable based on its type.
         * @details This overloaded function dispatches to the appropriate templated
         *          `new_xpath_variable` function based on the `xpath_value_type`.
         * @param type The `xpath_value_type` of the variable to create.
         * @param name The null-terminated C-style string name of the variable.
         * @return A pointer to the newly created `XPathVariable` on success, or `nullptr` on failure.
         * @warning The caller is responsible for freeing the allocated memory using
         *          `delete_xpath_variable`.
         */
        LUMEX_API
        inline XPathVariable *
        new_xpath_variable(xpath_value_type type, char_t const *name)
        {
          switch(type)
          {
          case xpath_type_node_set: return new_xpath_variable<xpath_variable_node_set>(name);
          case xpath_type_number: return new_xpath_variable<xpath_variable_number>(name);
          case xpath_type_string: return new_xpath_variable<xpath_variable_string>(name);
          case xpath_type_boolean: return new_xpath_variable<xpath_variable_boolean>(name);
          default: return nullptr;
          }
        }

        /**
         * @brief Frees the memory associated with a specific XPath variable type.
         * @details This templated function calls the destructor of the specific variable
         *          type `T` and then deallocates the memory previously allocated by
         *          `new_xpath_variable`.
         * @tparam T The specific derived `XPathVariable` type to delete.
         * @param var A pointer to the variable instance to delete.
         * @warning This function assumes `var` was allocated via `new_xpath_variable<T>`.
         */
        template <typename T>
        LUMEX_API inline void
        delete_xpath_variable(T *var)
        {
          var->~T();
          free(var); // NOLINT(cppcoreguidelines-owning-memory, cppcoreguidelines-no-malloc)
        }

        /**
         * @brief Frees the memory associated with an XPath variable based on its type.
         * @details This overloaded function dispatches to the appropriate templated
         *          `delete_xpath_variable` function based on the `xpath_value_type`.
         * @param type The `xpath_value_type` of the variable to delete.
         * @param var A pointer to the `XPathVariable` instance to delete.
         * @throws `LUMEX_ASSERT` if an invalid variable type is encountered.
         * @warning This function assumes `var` was allocated via `new_xpath_variable`.
         */
        LUMEX_API
        inline void
        delete_xpath_variable(xpath_value_type type, XPathVariable *var)
        {
          switch(type)
          {
          case xpath_type_node_set:
            delete_xpath_variable(static_cast< // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
                                  xpath_variable_node_set *>(var));
            break;
          case xpath_type_number:
            delete_xpath_variable(static_cast< // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
                                  xpath_variable_number *>(var));
            break;
          case xpath_type_string:
            delete_xpath_variable(static_cast< // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
                                  xpath_variable_string *>(var));
            break;
          case xpath_type_boolean:
            delete_xpath_variable(static_cast< // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
                                  xpath_variable_boolean *>(var));
            break;

          default: LUMEX_ASSERT(false && "Invalid variable type"); // unreachable
          }
        }

        /**
         * @brief Copies the value of one XPath variable to another.
         * @details This function performs a type-aware deep copy of the value from `rhs`
         *          to `lhs`. It handles the different underlying storage mechanisms for
         *          boolean, number, string, and node-set types.
         * @param lhs A pointer to the destination `XPathVariable` to which the value will be copied.
         * @param rhs A constant pointer to the source `XPathVariable` from which the value will be copied.
         * @return `true` if the copy was successful, `false` otherwise (e.g., memory allocation failure for
         * strings/node-sets).
         * @throws `LUMEX_ASSERT` if an invalid variable type is encountered.
         */
        LUMEX_API
        inline bool
        copy_xpath_variable(XPathVariable *lhs, XPathVariable const *rhs)
        {
          switch(rhs->type())
          {
          case xpath_type_node_set:
            return lhs->set(static_cast< // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
                              xpath_variable_node_set const *>(rhs)
                              ->value);
          case xpath_type_number:
            return lhs->set(static_cast< // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
                              xpath_variable_number const *>(rhs)
                              ->value);
          case xpath_type_string:
            return lhs->set(static_cast< // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
                              xpath_variable_string const *>(rhs)
                              ->value);
          case xpath_type_boolean:
            return lhs->set(static_cast< // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
                              xpath_variable_boolean const *>(rhs)
                              ->value);
          default:
            LUMEX_ASSERT(false && "Invalid variable type"); // unreachable
            return false;
          }
        }
      } // namespace Variable
    } // namespace XPath
  } // namespace Xml
} // namespace Lumex

#endif // !LUMEX_XML_XPATH_VARIABLE_HPP
