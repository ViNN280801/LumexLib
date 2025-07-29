/**
 * @file LumexStringify.hpp
 * @brief Provides cross-platform, variadic template functions for converting multiple arguments into a single string.
 * @details This header defines the `stringify` utility, a versatile function template that concatenates
 *          the string representations of an arbitrary number of arguments into a single `std::string`.
 *          It offers robust support across various C++ standards (C++11, C++14, C++17, C++20)
 *          by employing SFINAE, `if constexpr`, fold expressions, and C++20 Concepts
 *          to determine if types are "streamable" (i.e., can be written to an `std::ostream`).
 *          This utility is designed for debugging, logging, and general string formatting needs,
 *          providing a type-safe and convenient alternative to manual string concatenation or `sprintf`.
 *          It includes specializations for fundamental types and common smart pointers.
 */
#ifndef LUMEX_UTILITIES_HPP
#define LUMEX_UTILITIES_HPP

#include <memory>      // For std::unique_ptr, std::shared_ptr
#include <sstream>     // For std::ostringstream
#include <string>      // For std::string
#include <type_traits> // For std::decay_t, std::enable_if, std::is_same, std::declval, std::false_type, std::true_type
#include <utility>     // For std::forward

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace String
  {
    /**
     * @brief Provides string manipulation and utility functions.
     * @details This namespace contains various helpers related to string processing,
     *          including the `stringify` functionality for converting diverse types
     *          to their string representations.
     */
    namespace Utility
    {
      // ---------------------------------------------------------------------
      // C++11/C++14 compatibility layer
      // ---------------------------------------------------------------------

#if __cplusplus < 201703L
      /**
       * @brief C++11/C++14 compatible alias for `std::void_t`.
       * @details `std::void_t` is a C++17 feature. This alias provides a
       *          backwards-compatible equivalent, primarily used for SFINAE
       *          (Substitution Failure Is Not An Error) techniques to check
       *          for the validity of expressions (e.g., whether a type is streamable).
       * @tparam T A variadic pack of types. The type of `void_t` is always `void`.
       */
      template <typename... T> using void_t = void; // NOLINT(readability-identifier-naming)
#else
      /**
       * @brief Alias for `std::void_t` for C++17 and later.
       * @details Used for SFINAE in type trait metaprogramming.
       */
      using std::void_t;
#endif

      // ---------------------------------------------------------------------
      // C++20 concepts-based approach
      // ---------------------------------------------------------------------

#if __cplusplus >= 202002L

      /**
       * @brief C++20 Concept to check if a type is streamable to `std::ostream`.
       * @details This concept ensures that a type `T` can be written to an `std::ostream`
       *          using the `operator<<`. It is used by the `stringify` function
       *          to constrain its template arguments, providing clear compilation errors
       *          if a non-streamable type is passed.
       * @tparam T The type to check for streamability.
       */
      template <typename T>
      concept Streamable = requires(T &&type, std::ostream &ostream) { ostream << std::forward<T>(type); };

      /**
       * @brief C++20 Concept to check if all types in a variadic argument pack are streamable.
       * @details This concept uses a fold expression to ensure that every type in the
       *          `Args` pack satisfies the `Streamable` concept. This is crucial for
       *          the variadic `stringify` function, guaranteeing that all arguments
       *          can be correctly converted to string representations.
       * @tparam Args A variadic pack of types.
       */
      template <typename... Args>
      concept AllStreamable = (Streamable<std::decay_t<Args>> && ...);

#else

      // ---------------------------------------------------------------------
      // C++11/C++14/C++17 SFINAE-based approach
      // ---------------------------------------------------------------------

      /**
       * @brief Type trait to check if a type is streamable to `std::ostream` (SFINAE-based).
       * @details This primary template defaults to `std::false_type`. It is specialized
       *          below to `std::true_type` if the expression `std::declval<std::ostream &>() << std::declval<T>()`
       *          is well-formed, thereby identifying streamable types for older C++ standards.
       * @tparam T The type to check.
       * @tparam Enable A SFINAE parameter, typically `void`.
       */
      template <typename T, typename Enable = void> struct is_streamable : std::false_type {};

      /**
       * @brief Specialization of `is_streamable` for streamable types.
       * @details This specialization is enabled via SFINAE if `T` can be streamed
       *          to an `std::ostream`.
       * @tparam T The type to check.
       */
      template <typename T>
      struct is_streamable<T, void_t<decltype(std::declval<std::ostream &>() << std::declval<T>())>> : std::true_type {
      };

      /// @cond DO_NOT_DOCUMENT // Hide explicit specializations from Doxygen for brevity
      // Explicit specializations for fundamental types to ensure C++11 compatibility
      template <> struct is_streamable<bool> : std::true_type {};
      template <> struct is_streamable<char> : std::true_type {};
      template <> struct is_streamable<signed char> : std::true_type {};
      template <> struct is_streamable<unsigned char> : std::true_type {};
      template <> struct is_streamable<wchar_t> : std::true_type {};
      template <> struct is_streamable<short> : std::true_type {};
      template <> struct is_streamable<unsigned short> : std::true_type {};
      template <> struct is_streamable<int> : std::true_type {};
      template <> struct is_streamable<unsigned int> : std::true_type {};
      template <> struct is_streamable<long> : std::true_type {};
      template <> struct is_streamable<unsigned long> : std::true_type {};
      template <> struct is_streamable<long long> : std::true_type {};
      template <> struct is_streamable<unsigned long long> : std::true_type {};
      template <> struct is_streamable<float> : std::true_type {};
      template <> struct is_streamable<double> : std::true_type {};
      template <> struct is_streamable<long double> : std::true_type {};
      template <> struct is_streamable<char const *> : std::true_type {};
      template <> struct is_streamable<char *> : std::true_type {};
      template <> struct is_streamable<std::string> : std::true_type {};

      // Explicit specializations for smart pointer types
      template <typename T, typename D> struct is_streamable<std::unique_ptr<T, D>> : std::true_type {};
      template <typename T> struct is_streamable<std::shared_ptr<T>> : std::true_type {};
      /// @endcond

      /**
       * @brief Overload for streaming `std::unique_ptr` to an `std::ostream`.
       * @details This operator allows `std::unique_ptr` objects to be directly streamed.
       *          It prints the raw pointer value (address) of the managed object.
       * @tparam T The type managed by `std::unique_ptr`.
       * @tparam D The deleter type for `std::unique_ptr`.
       * @param ostream The output stream.
       * @param ptr The `std::unique_ptr` to stream.
       * @return A reference to the output stream.
       */
      template <typename T, typename D>
      std::ostream &
      operator<<(std::ostream &ostream, std::unique_ptr<T, D> const &ptr)
      {
        return ostream << ptr.get(); // Print raw pointer value
      }

      /**
       * @brief Overload for streaming `std::shared_ptr` to an `std::ostream`.
       * @details This operator allows `std::shared_ptr` objects to be directly streamed.
       *          It prints the raw pointer value (address) of the managed object.
       * @tparam T The type managed by `std::shared_ptr`.
       * @param ostream The output stream.
       * @param ptr The `std::shared_ptr` to stream.
       * @return A reference to the output stream.
       */
      template <typename T>
      std::ostream &
      operator<<(std::ostream &ostream, std::shared_ptr<T> const &ptr)
      {
        return ostream << ptr.get(); // Print raw pointer value
      }

  #if __cplusplus >= 201402L
      /**
       * @brief C++14 variable template for `is_streamable`.
       * @details Provides a convenient `_v` suffix helper for accessing the
       *          `value` member of the `is_streamable` type trait.
       * @tparam T The type to check.
       */
      template <typename T> constexpr bool is_streamable_v = is_streamable<std::decay_t<T>>::value;
  #endif

  #if __cplusplus >= 201703L
      /**
       * @brief C++17 variable template for `all_streamable` using fold expressions.
       * @details Checks if all types in a variadic argument pack are streamable.
       *          Utilizes C++17 fold expressions for concise implementation.
       * @tparam Args A variadic pack of types.
       */
      template <typename... Args> constexpr bool all_streamable_v = (is_streamable<std::decay_t<Args>>::value && ...);
  #else
      /**
       * @brief C++11/C++14 recursive type trait to check if all types in a pack are streamable.
       * @details This struct uses template recursion to check the `is_streamable`
       *          property for each type in a variadic template parameter pack.
       *          It serves the same purpose as the `AllStreamable` concept or
       *          C++17 fold expression `all_streamable_v` for older standards.
       * @tparam Args A variadic pack of types.
       */
      template <typename... Args> struct all_streamable;

      /**
       * @brief Recursive case for `all_streamable`.
       * @tparam First The first type in the pack.
       * @tparam Rest The remaining types in the pack.
       */
      template <typename First, typename... Rest> struct all_streamable<First, Rest...> {
        static constexpr bool value
          = is_streamable<typename std::decay<First>::type>::value && all_streamable<Rest...>::value;
      };

      /**
       * @brief Base case for `all_streamable` (empty pack).
       * @details An empty pack is considered "all streamable" as there are no non-streamable types.
       */
      template <> struct all_streamable<> {
        static constexpr bool value = true;
      };

    #if __cplusplus >= 201402L
      /**
       * @brief C++14 variable template for `all_streamable`.
       * @details Provides a convenient `_v` suffix helper for accessing the
       *          `value` member of the `all_streamable` type trait.
       * @tparam Args A variadic pack of types.
       */
      template <typename... Args> constexpr bool all_streamable_v = all_streamable<Args...>::value;
    #endif
  #endif

#endif // __cplusplus >= 202002L

      // ---------------------------------------------------------------------
      // Stringify implementation variants
      // ---------------------------------------------------------------------

#if __cplusplus >= 202002L

      /**
       * @brief Converts a variadic pack of arguments to a single string (C++20 Concepts version).
       * @details This function concatenates the string representation of all provided
       *          arguments into a single `std::string`. It leverages C++20 Concepts
       *          (`AllStreamable`) to ensure all arguments are streamable.
       *          Uses `if constexpr` for compile-time branching and fold expressions
       *          for efficient streaming of arguments.
       * @tparam Args A variadic pack of types, all of which must satisfy the `Streamable` concept.
       * @param args The arguments to be converted to string.
       * @return A `std::string` containing the concatenated string representations of the arguments.
       * @complexity O(N) where N is the total length of the string representations of all arguments.
       * @note Returns an empty string if no arguments are provided.
       */
      template <AllStreamable... Args>
      std::string
      stringify(Args &&...args)
      {
        if constexpr(sizeof...(args) == 0) { return ""; }
        else
        {
          std::ostringstream oss;
          ((oss << std::forward<Args>(args)), ...); // Fold expression to stream all arguments
          return oss.str();
        }
      }

#elif __cplusplus >= 201703L

      /**
       * @brief Converts a variadic pack of arguments to a single string (C++17 version).
       * @details This function concatenates the string representation of all provided
       *          arguments into a single `std::string`. It uses a `static_assert` with
       *          `all_streamable_v` to enforce streamability at compile time.
       *          Leverages `if constexpr` for empty argument pack optimization and
       *          fold expressions for efficient streaming.
       * @tparam Args A variadic pack of types. A compile-time error will occur if any type is not streamable.
       * @param args The arguments to be converted to string.
       * @return A `std::string` containing the concatenated string representations of the arguments.
       * @complexity O(N) where N is the total length of the string representations of all arguments.
       * @note Returns an empty string if no arguments are provided.
       */
      template <typename... Args>
      std::string
      stringify(Args &&...args)
      {
        static_assert(all_streamable_v<Args...>, "All arguments must be streamable");

        if constexpr(sizeof...(args) == 0) { return ""; }
        else
        {
          std::ostringstream oss;
          ((oss << std::forward<Args>(args)), ...); // Fold expression to stream all arguments
          return oss.str();
        }
      }

#elif __cplusplus >= 201402L

      /**
       * @brief Converts a variadic pack of arguments to a single string (C++14 version).
       * @details This function concatenates the string representation of all provided
       *          arguments into a single `std::string`. It uses a `static_assert` with
       *          `all_streamable_v` to enforce streamability at compile time.
       *          Argument expansion is performed using a `dummy` array trick.
       * @tparam Args A variadic pack of types. A compile-time error will occur if any type is not streamable.
       * @param args The arguments to be converted to string.
       * @return A `std::string` containing the concatenated string representations of the arguments.
       * @complexity O(N) where N is the total length of the string representations of all arguments.
       * @note This version does not have the `if constexpr` optimization for empty packs;
       *       use the `stringify()` no-argument overload for that.
       */
      template <typename... Args>
      std::string
      stringify(Args &&...args)
      {
        static_assert(all_streamable_v<Args...>, "All arguments must be streamable");

        std::ostringstream oss;
        int dummy[] = {0, ((void)(oss << std::forward<Args>(args)), 0)...}; // Pack expansion trick
        (void)dummy;                                                        // Suppress unused variable warning
        return oss.str();
      }

#else

      /**
       * @brief Converts a variadic pack of arguments to a single string (C++11 version).
       * @details This function concatenates the string representation of all provided
       *          arguments into a single `std::string`. It uses a `static_assert` with
       *          `all_streamable<Args...>::value` to enforce streamability at compile time.
       *          Argument expansion is performed using the `expander` array trick, typical for C++11 variadic
       * templates.
       * @tparam Args A variadic pack of types. A compile-time error will occur if any type is not streamable.
       * @param args The arguments to be converted to string.
       * @return A `std::string` containing the concatenated string representations of the arguments.
       * @complexity O(N) where N is the total length of the string representations of all arguments.
       * @note This version does not have an explicit `if constexpr` optimization for empty packs;
       *       use the `stringify()` no-argument overload for that.
       */
      template <typename... Args>
      std::string
      stringify(Args &&...args)
      {
        static_assert(all_streamable<Args...>::value, "All arguments must be streamable");

        std::ostringstream oss;
        using expander = int[];
        (void)expander{0, ((void)(oss << std::forward<Args>(args)), 0)...}; // Pack expansion trick
        return oss.str();
      }

#endif

      // ---------------------------------------------------------------------
      // Helper function for empty case optimization
      // ---------------------------------------------------------------------

      /**
       * @brief Overload of `stringify` for no arguments, returning an empty string.
       * @details This specialized overload handles the case where `stringify` is called
       *          without any arguments, providing an efficient `noexcept` way to return
       *          an empty string without creating an `ostringstream`.
       * @return An empty `std::string`.
       */
      inline std::string
      stringify() noexcept
      {
        return {};
      }

      // ---------------------------------------------------------------------
      // C++20+ requires clause version (alternative implementation)
      // ---------------------------------------------------------------------

#if __cplusplus >= 202002L

      /**
       * @brief Alternative `stringify` implementation using C++20 `requires` clause.
       * @details This version demonstrates the use of C++20 Concepts with a `requires` clause
       *          to constrain the template arguments, offering a more readable and direct
       *          way to express template constraints compared to SFINAE.
       *          It functions identically to the primary `stringify` overload for C++20.
       * @tparam Args A variadic pack of types, all of which must satisfy the `AllStreamable` concept.
       * @param args The arguments to be converted to string.
       * @return A `std::string` containing the concatenated string representations of the arguments.
       * @complexity O(N) where N is the total length of the string representations of all arguments.
       * @note Returns an empty string if no arguments are provided.
       */
      template <typename... Args>
        requires AllStreamable<Args...>
      std::string
      stringify_v2(Args &&...args)
      {
        if constexpr(sizeof...(args) == 0) { return {}; }
        else
        {
          std::ostringstream oss;
          ((oss << std::forward<Args>(args)), ...);
          return oss.str();
        }
      }

#endif

    } // namespace Utility
  } // namespace String
} // namespace Lumex

/**
 * @brief Global alias for `Lumex::String::Utility::stringify`.
 * @details This `using` declaration brings the `stringify` function template
 *          into the global namespace (or enclosing namespace where it's included),
 *          allowing for more convenient usage without full namespace qualification.
 */
using Lumex::String::Utility::stringify;

#endif // !LUMEX_UTILITIES_HPP
