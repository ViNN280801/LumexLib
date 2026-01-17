/**
 * @file LumexOptional.hpp
 * @brief C++11 implementation of `std::optional` (C++17 feature) providing a type-safe nullable value container.
 * @details This header provides a comprehensive, cross-platform implementation of an
 *          `optional` type, mirroring the functionality of `std::optional` from C++17.
 *          It allows for safe encapsulation of a value that may or may not be present,
 *          thereby preventing common pitfalls associated with `nullptr` or sentinel values.
 *          The implementation adheres strictly to C++11 standards, utilizing advanced
 *          template metaprogramming (SFINAE, `std::aligned_storage`, `std::type_traits`)
 *          and move semantics for efficiency. It is designed to be exception-safe,
 *          providing a custom exception type `LumexBadOptionalAccess` for invalid access attempts.
 *          The class supports various constructors (default, copy, move, in-place, value-based),
 *          assignment operators, observers (`has_value`, `value`, `value_or`, `operator*`, `operator->`),
 *          and modifiers (`swap`, `reset`, `emplace`). Non-member comparison operators
 *          and `std::hash` specialization are also provided for full compatibility and usability.
 */
#ifndef LUMEX_OPTIONAL_HPP
#define LUMEX_OPTIONAL_HPP

#include <functional>       // For std::hash
#include <initializer_list> // For std::initializer_list
#include <stdexcept>        // For std::logic_error
#include <type_traits>      // For std::aligned_storage, std::is_*, std::decay
#include <utility>          // For std::forward, std::move, std::swap

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Core
  {
    namespace Optional
    {
      /**
       * @brief A tag type used to indicate the absence of a value in `LumexOptional`.
       * @details This structure serves a similar purpose to `std::nullopt_t` in C++17.
       *          It is an empty type whose sole purpose is to provide a distinct type
       *          for the `nullopt` constant, enabling clear and unambiguous
       *          initialization and assignment of an empty `LumexOptional`.
       */
      struct nullopt_t {
        /**
         * @brief Internal tag for explicit construction.
         * @details This nested struct is used to enforce explicit construction of `nullopt_t`,
         *          preventing accidental implicit conversions.
         */
        struct init_tag {};
        /**
         * @brief Explicit constructor for `nullopt_t`.
         * @param unused An unused `init_tag` parameter, enforcing explicit construction.
         */
        explicit constexpr nullopt_t(init_tag /*unused*/) {}
      };
      /**
       * @brief A constant of type `nullopt_t` used to represent an empty `LumexOptional`.
       * @details This global constant is the canonical way to initialize or assign
       *          an empty state to a `LumexOptional` object.
       * @code
       * LumexOptional<int> opt1 = Lumex::nullopt;
       * LumexOptional<std::string> opt2; // Also initializes as empty
       * opt2 = Lumex::nullopt;
       * @endcode
       */
      constexpr nullopt_t nullopt{nullopt_t::init_tag{}};

      /**
       * @brief A tag type used to indicate in-place construction of the contained value.
       * @details This structure mirrors `std::in_place_t` from C++17. It is used
       *          in `LumexOptional` constructors and `emplace` methods to signify
       *          that the contained object should be constructed directly inside
       *          the optional's storage using the provided arguments, rather than
       *          copying or moving an existing object. This avoids unnecessary
       *          temporary objects and can improve performance.
       */
      struct in_place_t {
        /**
         * @brief Internal tag for explicit construction.
         * @details Similar to `nullopt_t::init_tag`, this enforces explicit construction.
         */
        struct init_tag {};
        /**
         * @brief Explicit constructor for `in_place_t`.
         * @param unused An unused `init_tag` parameter, enforcing explicit construction.
         */
        explicit constexpr in_place_t(init_tag /*unused*/) {}
      };
      /**
       * @brief A constant of type `in_place_t` used for in-place construction.
       * @details This global constant is used as the first argument in `LumexOptional`
       *          constructors or `emplace` calls when direct construction of the
       *          contained value is desired.
       * @code
       * LumexOptional<MyClass> opt1(Lumex::in_place, arg1, arg2); // Constructs MyClass(arg1, arg2)
       * LumexOptional<std::vector<int>> opt2(Lumex::in_place, {1, 2, 3}); // Constructs std::vector<int>({1, 2, 3})
       * @endcode
       */
      constexpr in_place_t in_place{in_place_t::init_tag{}};

      /**
       * @brief Exception thrown when attempting to access the value of an empty `LumexOptional`.
       * @details This exception class is derived from `std::logic_error` and is thrown
       *          by the `value()` method of `LumexOptional` if `has_value()` is false.
       *          It indicates a programming error where an attempt was made to dereference
       *          an optional object that does not currently hold a value.
       * @note This exception is consistent with `std::bad_optional_access` in C++17.
       */
      class LumexBadOptionalAccess : public std::logic_error
      {
      public:
        /**
         * @brief Constructs a `LumexBadOptionalAccess` exception with a default message.
         */
        LumexBadOptionalAccess() : std::logic_error("LumexBadOptionalAccess: Bad optional access") {}
        /**
         * @brief Constructs a `LumexBadOptionalAccess` exception with a custom C-style string message.
         * @param what_arg A C-style string describing the error.
         */
        explicit LumexBadOptionalAccess(char const *what_arg) : std::logic_error(what_arg) {}
        /**
         * @brief Constructs a `LumexBadOptionalAccess` exception with a custom `std::string` message.
         * @param what_arg A `std::string` describing the error.
         */
        explicit LumexBadOptionalAccess(std::string const &what_arg) : std::logic_error(what_arg) {}
      };

      /**
       * @brief A C++11 implementation of `std::optional` for type-safe nullable values.
       * @details The `LumexOptional` template class provides a mechanism to encapsulate
       *          an optional value of type `T`. An instance of `LumexOptional` can either
       *          contain a value or be empty. This design avoids the pitfalls of `nullptr`
       *          and clearly expresses intent in function signatures and data structures.
       *
       *          It manages its internal storage using `std::aligned_storage` to
       *          construct `T` in-place, ensuring proper alignment and avoiding dynamic
       *          memory allocations for the contained value itself.
       *
       * @tparam T The type of the value to be held. `T` must be a non-reference, non-array,
       *           non-void type. Its constructor/destructor requirements are managed
       *           internally, leveraging `std::is_nothrow_copy_constructible` etc.
       *
       * @note This class adheres to the Rule of Five: it explicitly defines
       *       copy constructor, move constructor, copy assignment operator,
       *       move assignment operator, and destructor. Copy operations are
       *       deleted to ensure correct resource management and behavior.
       */
      template <typename T> class LumexOptional
      {
      public:
        /**
         * @brief Type alias for the contained value type.
         */
        using value_type = T;

      private:
        /**
         * @brief Aligned storage for the optional value.
         * @details Uses `std::aligned_storage` to reserve raw memory that is correctly
         *          aligned and sized for `T`, allowing for in-place construction
         *          and destruction of the `T` object without dynamic allocations.
         */
        typename std::aligned_storage<sizeof(T), alignof(T)>::type m_storage;
        /**
         * @brief Flag indicating whether the optional currently holds a value.
         * @details `true` if a value is present, `false` otherwise. This flag is
         *          essential for managing the lifecycle of the contained object.
         */
        bool m_has_value;

        /**
         * @brief Internal helper to get a pointer to the contained object.
         * @details Reinterprets the `m_storage` as a pointer to `T`.
         * @return A non-const pointer to the contained object.
         */
        T *
        get_ptr() noexcept
        {
          return reinterpret_cast<T *>( // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
            std::addressof(m_storage));
        }

        /**
         * @brief Internal helper to get a const pointer to the contained object.
         * @details Reinterprets the `m_storage` as a const pointer to `T`.
         * @return A const pointer to the contained object.
         */
        T const *
        get_ptr() const noexcept
        {
          return reinterpret_cast<T const *>( // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
            std::addressof(m_storage));
        }

// — Internal construction/destruction —
// suppress MSVC "unreachable code" in construct()
#ifdef _MSC_VER
  #pragma warning(push)
  #pragma warning(disable : 4702)
#endif
        /**
         * @brief Constructs the contained object in-place.
         * @details Uses placement new to construct an object of type `T` directly
         *          within `m_storage` using the provided arguments. Sets `m_has_value` to `true`.
         * @tparam Args Variadic template arguments for the constructor of `T`.
         * @param args Arguments to forward to the constructor of `T`.
         */
        template <typename... Args>
        void
        construct(Args &&...args)
        {
          new(get_ptr()) T(std::forward<Args>(args)...);
          m_has_value = true;
        }
#ifdef _MSC_VER
  #pragma warning(pop)
#endif

        /**
         * @brief Destroys the contained object if present.
         * @details Calls the destructor of the contained `T` object and sets `m_has_value` to `false`.
         *          This method is safe to call even if no value is present.
         */
        void
        destroy() noexcept
        {
          if(m_has_value)
          {
            get_ptr()->~T();
            m_has_value = false;
          }
        }

      public:
        // — Constructors —

        /**
         * @brief Default constructor. Creates an empty `LumexOptional`.
         * @details Initializes `m_has_value` to `false`, indicating no value is present.
         *          This constructor is `constexpr` for C++11 compatibility.
         */
        constexpr LumexOptional() noexcept : m_has_value(false) {}

        /**
         * @brief Constructs an empty `LumexOptional` from `nullopt`.
         * @details Allows explicit construction of an empty optional using the `nullopt` constant.
         * @param unused A `nullopt_t` constant (e.g., `Lumex::nullopt`).
         */
        constexpr LumexOptional(nullopt_t /*unused*/) noexcept : m_has_value(false) {}

        /**
         * @brief Copy constructor. Constructs a `LumexOptional` by copying another.
         * @details If `other` contains a value, a copy of that value is constructed
         *          in-place. If `other` is empty, this optional will also be empty.
         * @param other The `LumexOptional` object to copy from.
         * @note The `noexcept` specification depends on the copy constructibility of `T`.
         */
        LumexOptional(LumexOptional const &other) noexcept(std::is_nothrow_copy_constructible<T>::value)
            : m_has_value(false)
        {
          if(other.m_has_value) construct(*other.get_ptr());
        }

        /**
         * @brief Move constructor. Constructs a `LumexOptional` by moving from another.
         * @details If `other` contains a value, that value is moved into this optional,
         *          and `other` is left in a valid but unspecified (usually empty) state.
         *          If `other` is empty, this optional will also be empty.
         * @param other The `LumexOptional` object to move from.
         * @note The `noexcept` specification depends on the move constructibility of `T`.
         */
        LumexOptional(LumexOptional &&other) noexcept(std::is_nothrow_move_constructible<T>::value) : m_has_value(false)
        {
          if(other.m_has_value)
          {
            construct(std::move(*other.get_ptr()));
            other.destroy(); // Destroys the value in the source optional
          }
        }

        /**
         * @brief Value copy constructor. Constructs a `LumexOptional` with a copied value.
         * @details Creates a `LumexOptional` containing a copy of the provided `value`.
         * @param value The value of type `T` to copy.
         * @note The `noexcept` specification depends on the copy constructibility of `T`.
         */
        LumexOptional(T const &value) noexcept(std::is_nothrow_copy_constructible<T>::value) : m_has_value(false)
        {
          construct(value);
        }

        /**
         * @brief Value move constructor. Constructs a `LumexOptional` with a moved value.
         * @details Creates a `LumexOptional` containing a value moved from the provided `value`.
         * @param value The value of type `T` to move.
         * @note The `noexcept` specification depends on the move constructibility of `T`.
         */
        LumexOptional(T &&value) noexcept(std::is_nothrow_move_constructible<T>::value) : m_has_value(false)
        {
          construct(std::move(value));
        }

        /**
         * @brief In-place constructor. Constructs the contained value directly.
         * @details Allows constructing the contained object `T` directly within the
         *          optional's storage using the provided arguments, bypassing
         *          intermediate copy/move operations.
         * @tparam Args Variadic template arguments for the constructor of `T`.
         * @param unused An `in_place_t` constant (e.g., `Lumex::in_place`).
         * @param args Arguments to forward to the constructor of `T`.
         */
        template <typename... Args> explicit LumexOptional(in_place_t /*unused*/, Args &&...args) : m_has_value(false)
        {
          construct(std::forward<Args>(args)...);
        }

        /**
         * @brief In-place constructor with `std::initializer_list`.
         * @details Constructs the contained object `T` in-place, passing an `initializer_list`
         *          and additional arguments to its constructor.
         * @tparam U The type of elements in the `initializer_list`.
         * @tparam Args Variadic template arguments for the constructor of `T`.
         * @param unused An `in_place_t` constant.
         * @param ilist An `std::initializer_list` to pass to the constructor of `T`.
         * @param args Additional arguments to forward to the constructor of `T`.
         */
        template <typename U, typename... Args>
        explicit LumexOptional(in_place_t /*unused*/, std::initializer_list<U> ilist, Args &&...args)
            : m_has_value(false)
        {
          construct(ilist, std::forward<Args>(args)...);
        }

        // ============ Destructor ============
        /**
         * @brief Destructor. Destroys the contained value if present.
         * @details Calls `destroy()` to safely destruct the contained object (if any)
         *          and reset the `m_has_value` flag.
         */
        ~LumexOptional() { destroy(); }

        // ============ Assignment operators ============

        /**
         * @brief Assigns `nullopt` to the `LumexOptional`, making it empty.
         * @details If the optional currently holds a value, its destructor is called,
         *          and then `m_has_value` is set to `false`.
         * @param unused A `nullopt_t` constant.
         * @return A reference to `*this`.
         */
        LumexOptional &
        operator=(nullopt_t /*unused*/) noexcept
        {
          destroy();
          return *this;
        }

        /**
         * @brief Copy assignment operator. Assigns the value of another `LumexOptional`.
         * @details This operator handles various scenarios:
         *          - If `other` has a value: If `*this` also has a value, the old value
         *            is destroyed, and a new one is copy-constructed from `other`.
         *            If `*this` is empty, a new value is copy-constructed from `other`.
         *          - If `other` is empty: If `*this` has a value, its value is destroyed.
         *            If `*this` is already empty, nothing happens.
         * @param other The `LumexOptional` object to copy from.
         * @return A reference to `*this`.
         * @note The `noexcept` specification depends on the copy constructibility and
         *       copy assignability of `T`. Self-assignment is handled.
         */
        LumexOptional &
        operator=(LumexOptional const &other) noexcept(std::is_nothrow_copy_constructible<T>::value
                                                       && std::is_nothrow_copy_assignable<T>::value)
        {
          if(this != &other) // Handle self-assignment
          {
            if(other.m_has_value) // 'other' has a value
            {
              if(m_has_value) // '*this' also has a value: assign or destroy old, construct new
              {
                // If T is trivially copy assignable, we could assign directly.
                // For general case, destroy and reconstruct is safer for non-trivial types.
                // This implementation chooses safety over potential micro-optimizations.
                destroy();
                construct(*other.get_ptr());
              }
              else // '*this' is empty, 'other' has value: construct new
              {
                construct(*other.get_ptr());
              }
            }
            else // 'other' is empty
            {
              if(m_has_value) // '*this' has a value, 'other' is empty: destroy '*this' value
                destroy();
              // else: Both are empty, nothing to do.
            }
          }
          return *this;
        }

        /**
         * @brief Move assignment operator. Assigns the value of another `LumexOptional` by moving.
         * @details This operator handles various scenarios similar to copy assignment,
         *          but performs move operations for efficiency:
         *          - If `other` has a value: If `*this` also has a value, the old value
         *            is destroyed, and a new one is move-constructed from `other`.
         *            If `*this` is empty, a new value is move-constructed from `other`.
         *            `other` is then destroyed (left in an empty state).
         *          - If `other` is empty: If `*this` has a value, its value is destroyed.
         *            If `*this` is already empty, nothing happens.
         * @param other The `LumexOptional` object to move from.
         * @return A reference to `*this`.
         * @note The `noexcept` specification depends on the move constructibility and
         *       move assignability of `T`. Self-assignment is handled.
         */
        LumexOptional &
        operator=(LumexOptional &&other) noexcept(std::is_nothrow_move_constructible<T>::value
                                                  && std::is_nothrow_move_assignable<T>::value)
        {
          if(this != &other) // Handle self-assignment
          {
            if(other.m_has_value)
            {
              if(m_has_value)
              // If T is move assignable, we could assign directly: *get_ptr() = std::move(*other.get_ptr());
              // For general case, destroy and reconstruct is safer for non-trivial types.
              // This implementation chooses safety over potential micro-optimizations.
              {
                destroy();
                construct(std::move(*other.get_ptr()));
              }
              else
                construct(std::move(*other.get_ptr()));
              other.destroy(); // Ensure the source optional is empty after move
            }
            else { destroy(); } // If other is empty, ensure *this* is empty
          }
          return *this;
        }

        // ============ Value copy assignment ============
        /**
         * @brief Value assignment operator. Assigns a new value to the `LumexOptional`.
         * @details Assigns the provided `value` to the optional. If the optional
         *          already contains a value, that value is assigned to. If it's empty,
         *          a new value is constructed in-place. This operator uses perfect
         *          forwarding to accept both lvalues (copy) and rvalues (move) for `value`.
         * @tparam U A type convertible to `T`. `std::enable_if` is used to restrict
         *           this overload to cases where `U` (after decay) is the same type as `T`.
         * @param value The value to assign. It can be an lvalue (copied) or an rvalue (moved).
         * @return A reference to `*this`.
         */
        template <typename U = T,
                  typename   = typename std::enable_if<std::is_same<typename std::decay<U>::type, T>::value>::type>
        LumexOptional &
        operator=(U &&value)
        {
          if(m_has_value)
            *get_ptr() = std::forward<U>(value); // Use placement assignment
          else
            construct(std::forward<U>(value)); // Construct new value
          return *this;
        }

        // ============ Observers ============

        /**
         * @brief Dereferences the `LumexOptional` to access the contained value.
         * @details Provides access to the contained value as a const pointer.
         *          No bounds checking is performed; behavior is undefined if `has_value()` is false.
         * @return A const pointer to the contained object of type `T`.
         */
        T const *
        operator->() const noexcept
        {
          return get_ptr();
        }

        /**
         * @brief Dereferences the `LumexOptional` to access the contained value.
         * @details Provides access to the contained value as a non-const pointer.
         *          No bounds checking is performed; behavior is undefined if `has_value()` is false.
         * @return A non-const pointer to the contained object of type `T`.
         */
        T *
        operator->() noexcept
        {
          return get_ptr();
        }

        /**
         * @brief Dereferences the `LumexOptional` to access the contained value as a const lvalue reference.
         * @details Provides access to the contained value as a const lvalue reference.
         *          No bounds checking is performed; behavior is undefined if `has_value()` is false.
         * @return A const lvalue reference to the contained object of type `T`.
         */
        T const &
        operator*() const & noexcept
        {
          return *get_ptr();
        }

        /**
         * @brief Dereferences the `LumexOptional` to access the contained value as a non-const lvalue reference.
         * @details Provides access to the contained value as a non-const lvalue reference.
         *          No bounds checking is performed; behavior is undefined if `has_value()` is false.
         * @return A non-const lvalue reference to the contained object of type `T`.
         */
        T &
        operator*() & noexcept
        {
          return *get_ptr();
        }

        /**
         * @brief Dereferences the `LumexOptional` to access the contained value as a const rvalue reference.
         * @details Provides access to the contained value as a const rvalue reference,
         *          enabling move semantics for the contained value when the optional itself is an rvalue.
         *          No bounds checking is performed; behavior is undefined if `has_value()` is false.
         * @return A const rvalue reference to the contained object of type `T`.
         */
        T const &&
        operator*() const && noexcept
        {
          return std::move(*get_ptr());
        }

        /**
         * @brief Dereferences the `LumexOptional` to access the contained value as a non-const rvalue reference.
         * @details Provides access to the contained value as a non-const rvalue reference,
         *          enabling move semantics for the contained value when the optional itself is an rvalue.
         *          No bounds checking is performed; behavior is undefined if `has_value()` is false.
         * @return A non-const rvalue reference to the contained object of type `T`.
         */
        T &&
        operator*() && noexcept
        {
          return std::move(*get_ptr());
        }

        /**
         * @brief Explicit conversion to `bool`.
         * @details Allows `LumexOptional` to be used in boolean contexts (e.g., `if (opt)`).
         * @return `true` if the optional contains a value, `false` otherwise.
         */
        explicit
        operator bool() const noexcept
        {
          return m_has_value;
        }

        /**
         * @brief Checks if the `LumexOptional` contains a value.
         * @return `true` if a value is present, `false` otherwise.
         * @see `operator bool()`
         */
        bool
        has_value() const noexcept
        {
          return m_has_value;
        }

        /**
         * @brief Returns a non-const lvalue reference to the contained value.
         * @details If `has_value()` is `false`, throws `LumexBadOptionalAccess`.
         *          Otherwise, returns a reference to the contained value.
         * @return A non-const lvalue reference to the contained object of type `T`.
         * @throws LumexBadOptionalAccess If the optional does not contain a value.
         */
        T &
        value() &
        {
          if(!m_has_value) throw LumexBadOptionalAccess();
          return *get_ptr();
        }

        /**
         * @brief Returns a const lvalue reference to the contained value.
         * @details If `has_value()` is `false`, throws `LumexBadOptionalAccess`.
         *          Otherwise, returns a const reference to the contained value.
         * @return A const lvalue reference to the contained object of type `T`.
         * @throws LumexBadOptionalAccess If the optional does not contain a value.
         */
        T const &
        value() const &
        {
          if(!m_has_value) throw LumexBadOptionalAccess();
          return *get_ptr();
        }

        /**
         * @brief Returns a non-const rvalue reference to the contained value.
         * @details If `has_value()` is `false`, throws `LumexBadOptionalAccess`.
         *          Otherwise, returns an rvalue reference to the contained value,
         *          enabling moving out the value.
         * @return A non-const rvalue reference to the contained object of type `T`.
         * @throws LumexBadOptionalAccess If the optional does not contain a value.
         */
        T &&
        value() &&
        {
          if(!m_has_value) throw LumexBadOptionalAccess();
          return std::move(*get_ptr());
        }

        /**
         * @brief Returns a const rvalue reference to the contained value.
         * @details If `has_value()` is `false`, throws `LumexBadOptionalAccess`.
         *          Otherwise, returns a const rvalue reference to the contained value.
         * @return A const rvalue reference to the contained object of type `T`.
         * @throws LumexBadOptionalAccess If the optional does not contain a value.
         */
        T const &&
        value() const &&
        {
          if(!m_has_value) throw LumexBadOptionalAccess();
          return std::move(*get_ptr());
        }

        /**
         * @brief Returns the contained value or a specified default value.
         * @details If `has_value()` is `true`, returns a copy of the contained value.
         *          Otherwise, returns a copy of `default_value`.
         *          This overload is for const lvalue `LumexOptional`.
         * @tparam U A type convertible to `T`.
         * @param default_value The value to return if the optional is empty.
         * @return The contained value or `default_value`.
         */
        template <typename U>
        T
        value_or(U &&default_value) const &
        {
          return m_has_value ? **this : static_cast<T>(std::forward<U>(default_value));
        }

        /**
         * @brief Returns the contained value or a specified default value.
         * @details If `has_value()` is `true`, returns the contained value by moving it.
         *          Otherwise, returns a copy of `default_value`.
         *          This overload is for rvalue `LumexOptional`, enabling move semantics.
         * @tparam U A type convertible to `T`.
         * @param default_value The value to return if the optional is empty.
         * @return The contained value (moved) or `default_value`.
         */
        template <typename U>
        T
        value_or(U &&default_value) &&
        {
          return m_has_value ? std::move(**this) : static_cast<T>(std::forward<U>(default_value));
        }

        // — Modifiers —

        /**
         * @brief Swaps the contents of this `LumexOptional` with another.
         * @details This method provides an efficient, exception-safe swap operation.
         *          It handles all combinations of filled and empty optionals:
         *          - Both have values: swaps the contained values using `std::swap`.
         *          - One has a value, the other is empty: moves the value to the empty optional,
         *            leaving the source empty.
         *          - Both are empty: does nothing.
         * @param other The `LumexOptional` object to swap with.
         * @note The `noexcept` specification depends on the move constructibility
         *       and move assignability of `T`.
         */
        void
        swap(LumexOptional &other) noexcept(std::is_nothrow_move_constructible<T>::value
                                            && std::is_nothrow_move_assignable<T>::value)
        {
          if(m_has_value && other.m_has_value)
          {
            using std::swap;
            swap(**this, *other); // Swap contained values
          }
          else if(m_has_value)
          {
            // This has value, other is empty. Move this's value to other.
            other.construct(std::move(**this));
            destroy();
          }
          else if(other.m_has_value)
          {
            // Other has value, this is empty. Move other's value to this.
            construct(std::move(*other));
            other.destroy();
          }
          // else: Both are empty, nothing to do.
        }

        /**
         * @brief Resets the `LumexOptional` to an empty state.
         * @details If the optional contains a value, its destructor is called,
         *          and `m_has_value` is set to `false`. If it's already empty,
         *          this method does nothing.
         */
        void
        reset() noexcept
        {
          destroy();
        }

// emplace
#ifdef _MSC_VER
  #pragma warning(push)
  #pragma warning(disable : 4702)
#endif
        /**
         * @brief Constructs a new value in-place, destroying any existing value.
         * @details This method first destroys the currently contained value (if any),
         *          then constructs a new object of type `T` directly within the
         *          optional's storage using the provided arguments.
         * @tparam Args Variadic template arguments for the constructor of `T`.
         * @param args Arguments to forward to the constructor of `T`.
         * @return A non-const reference to the newly constructed contained object.
         */
        template <typename... Args>
        T &
        emplace(Args &&...args)
        {
          destroy();
          construct(std::forward<Args>(args)...);
          return **this;
        }

        // suppress MSVC "unreachable code" in emplace(initializer_list<>)
        /**
         * @brief Constructs a new value in-place using an `initializer_list`, destroying any existing value.
         * @details This method first destroys the currently contained value (if any),
         *          then constructs a new object of type `T` directly within the
         *          optional's storage, passing an `initializer_list` and additional
         *          arguments to its constructor.
         * @tparam U The type of elements in the `initializer_list`.
         * @tparam Args Variadic template arguments for the constructor of `T`.
         * @param ilist An `std::initializer_list` to pass to the constructor of `T`.
         * @param args Additional arguments to forward to the constructor of `T`.
         * @return A non-const reference to the newly constructed contained object.
         */
        template <typename U, typename... Args>
        T &
        emplace(std::initializer_list<U> ilist, Args &&...args)
        {
          destroy();
          construct(ilist, std::forward<Args>(args)...);
          return **this;
        }
#ifdef _MSC_VER
  #pragma warning(pop)
#endif
      };

      // — Non-member comparison operators —

      /**
       * @brief Equality comparison operator for two `LumexOptional` objects.
       * @details
       *   - Returns `true` if both optionals are empty.
       *   - Returns `false` if one is empty and the other has a value.
       *   - Returns `true` if both have values and their contained values are equal (`*lhs == *rhs`).
       * @tparam T Type of value in the left-hand side optional.
       * @tparam U Type of value in the right-hand side optional.
       * @param lhs The left-hand side `LumexOptional` object.
       * @param rhs The right-hand side `LumexOptional` object.
       * @return `true` if the optionals are equal, `false` otherwise.
       * @note This function is not `constexpr` due to C++14 extension limitations on `if` statements.
       */
      template <typename T, typename U>
      bool
      operator==(LumexOptional<T> const &lhs, LumexOptional<U> const &rhs)
      {
        if(lhs.has_value() != rhs.has_value()) return false;
        if(!lhs.has_value()) return true; // both are empty
        return *lhs == *rhs;
      }

      /**
       * @brief Inequality comparison operator for two `LumexOptional` objects.
       * @tparam T Type of value in the left-hand side optional.
       * @tparam U Type of value in the right-hand side optional.
       * @param lhs The left-hand side `LumexOptional` object.
       * @param rhs The right-hand side `LumexOptional` object.
       * @return `true` if the optionals are not equal, `false` otherwise.
       */
      template <typename T, typename U>
      constexpr bool
      operator!=(LumexOptional<T> const &lhs, LumexOptional<U> const &rhs)
      {
        return !(lhs == rhs);
      }

      /**
       * @brief Less-than comparison operator for two `LumexOptional` objects.
       * @details
       *   - Returns `false` if `rhs` is empty (nothing is less than empty).
       *   - Returns `true` if `lhs` is empty and `rhs` has a value (empty is less than any value).
       *   - Returns `true` if both have values and `*lhs < *rhs`.
       * @tparam T Type of value in the left-hand side optional.
       * @tparam U Type of value in the right-hand side optional.
       * @param lhs The left-hand side `LumexOptional` object.
       * @param rhs The right-hand side `LumexOptional` object.
       * @return `true` if `lhs` is less than `rhs`, `false` otherwise.
       * @note This function is not `constexpr` due to C++14 extension limitations on `if` statements.
       */
      template <typename T, typename U>
      bool
      operator<(LumexOptional<T> const &lhs, LumexOptional<U> const &rhs)
      {
        if(!rhs.has_value()) return false; // nothing is less than empty
        if(!lhs.has_value()) return true;  // empty is less than any value
        return *lhs < *rhs;
      }

      /**
       * @brief Less-than-or-equal-to comparison operator for two `LumexOptional` objects.
       * @tparam T Type of value in the left-hand side optional.
       * @tparam U Type of value in the right-hand side optional.
       * @param lhs The left-hand side `LumexOptional` object.
       * @param rhs The right-hand side `LumexOptional` object.
       * @return `true` if `lhs` is less than or equal to `rhs`, `false` otherwise.
       */
      template <typename T, typename U>
      constexpr bool
      operator<=(LumexOptional<T> const &lhs, LumexOptional<U> const &rhs)
      {
        return !(rhs < lhs);
      }

      /**
       * @brief Greater-than comparison operator for two `LumexOptional` objects.
       * @tparam T Type of value in the left-hand side optional.
       * @tparam U Type of value in the right-hand side optional.
       * @param lhs The left-hand side `LumexOptional` object.
       * @param rhs The right-hand side `LumexOptional` object.
       * @return `true` if `lhs` is greater than `rhs`, `false` otherwise.
       */
      template <typename T, typename U>
      constexpr bool
      operator>(LumexOptional<T> const &lhs, LumexOptional<U> const &rhs)
      {
        return rhs < lhs;
      }

      /**
       * @brief Greater-than-or-equal-to comparison operator for two `LumexOptional` objects.
       * @tparam T Type of value in the left-hand side optional.
       * @tparam U Type of value in the right-hand side optional.
       * @param lhs The left-hand side `LumexOptional` object.
       * @param rhs The right-hand side `LumexOptional` object.
       * @return `true` if `lhs` is greater than or equal to `rhs`, `false` otherwise.
       */
      template <typename T, typename U>
      constexpr bool
      operator>=(LumexOptional<T> const &lhs, LumexOptional<U> const &rhs)
      {
        return !(lhs < rhs);
      }

      // Comparison with nullopt
      /**
       * @brief Equality comparison between a `LumexOptional` and `nullopt`.
       * @tparam T Type of value in the optional.
       * @param opt The `LumexOptional` object.
       * @param unused `nullopt` constant.
       * @return `true` if the optional is empty, `false` otherwise.
       */
      template <typename T>
      constexpr bool
      operator==(LumexOptional<T> const &opt, nullopt_t /*unused*/) noexcept
      {
        return !opt.has_value();
      }

      /**
       * @brief Equality comparison between `nullopt` and a `LumexOptional`.
       * @tparam T Type of value in the optional.
       * @param unused `nullopt` constant.
       * @param opt The `LumexOptional` object.
       * @return `true` if the optional is empty, `false` otherwise.
       */
      template <typename T>
      constexpr bool
      operator==(nullopt_t /*unused*/, LumexOptional<T> const &opt) noexcept
      {
        return !opt.has_value();
      }

      /**
       * @brief Inequality comparison between a `LumexOptional` and `nullopt`.
       * @tparam T Type of value in the optional.
       * @param opt The `LumexOptional` object.
       * @param unused `nullopt` constant.
       * @return `true` if the optional has a value, `false` otherwise.
       */
      template <typename T>
      constexpr bool
      operator!=(LumexOptional<T> const &opt, nullopt_t /*unused*/) noexcept
      {
        return opt.has_value();
      }

      /**
       * @brief Inequality comparison between `nullopt` and a `LumexOptional`.
       * @tparam T Type of value in the optional.
       * @param unused `nullopt` constant.
       * @param opt The `LumexOptional` object.
       * @return `true` if the optional has a value, `false` otherwise.
       */
      template <typename T>
      constexpr bool
      operator!=(nullopt_t /*unused*/, LumexOptional<T> const &opt) noexcept
      {
        return opt.has_value();
      }

      /**
       * @brief Less-than comparison between a `LumexOptional` and `nullopt`.
       * @details An optional with a value is never less than `nullopt`.
       * @tparam T Type of value in the optional.
       * @param unused_opt The `LumexOptional` object.
       * @param unused_nullopt `nullopt` constant.
       * @return `false`.
       */
      template <typename T>
      constexpr bool
      operator<(LumexOptional<T> const & /*unused_opt*/, nullopt_t /*unused_nullopt*/) noexcept
      {
        return false;
      }

      /**
       * @brief Less-than comparison between `nullopt` and a `LumexOptional`.
       * @details `nullopt` is less than any `LumexOptional` that contains a value.
       * @tparam T Type of value in the optional.
       * @param unused_nullopt `nullopt` constant.
       * @param opt The `LumexOptional` object.
       * @return `true` if `opt` has a value, `false` otherwise.
       */
      template <typename T>
      constexpr bool
      operator<(nullopt_t /*unused_nullopt*/, LumexOptional<T> const &opt) noexcept
      {
        return opt.has_value();
      }

      /**
       * @brief Less-than-or-equal-to comparison between a `LumexOptional` and `nullopt`.
       * @details An optional with a value is never less than or equal to `nullopt`.
       * @tparam T Type of value in the optional.
       * @param opt The `LumexOptional` object.
       * @param unused_nullopt `nullopt` constant.
       * @return `true` if the optional is empty, `false` otherwise.
       */
      template <typename T>
      constexpr bool
      operator<=(LumexOptional<T> const &opt, nullopt_t /*unused_nullopt*/) noexcept
      {
        return !opt.has_value();
      }

      /**
       * @brief Less-than-or-equal-to comparison between `nullopt` and a `LumexOptional`.
       * @details `nullopt` is always less than or equal to any `LumexOptional`.
       * @tparam T Type of value in the optional.
       * @param unused_nullopt `nullopt` constant.
       * @param unused_opt The `LumexOptional` object.
       * @return `true`.
       */
      template <typename T>
      constexpr bool
      operator<=(nullopt_t /*unused_nullopt*/, LumexOptional<T> const & /*unused_opt*/) noexcept
      {
        return true;
      }

      /**
       * @brief Greater-than comparison between a `LumexOptional` and `nullopt`.
       * @details A `LumexOptional` with a value is always greater than `nullopt`.
       * @tparam T Type of value in the optional.
       * @param opt The `LumexOptional` object.
       * @param unused_nullopt `nullopt` constant.
       * @return `true` if the optional has a value, `false` otherwise.
       */
      template <typename T>
      constexpr bool
      operator>(LumexOptional<T> const &opt, nullopt_t /*unused_nullopt*/) noexcept
      {
        return opt.has_value();
      }

      /**
       * @brief Greater-than comparison between `nullopt` and a `LumexOptional`.
       * @details `nullopt` is never greater than any `LumexOptional`.
       * @tparam T Type of value in the optional.
       * @param unused_nullopt `nullopt` constant.
       * @param unused_opt The `LumexOptional` object.
       * @return `false`.
       */
      template <typename T>
      constexpr bool
      operator>(nullopt_t /*unused_nullopt*/, LumexOptional<T> const & /*unused_opt*/) noexcept
      {
        return false;
      }

      /**
       * @brief Greater-than-or-equal-to comparison between a `LumexOptional` and `nullopt`.
       * @details A `LumexOptional` is always greater than or equal to `nullopt`.
       * @tparam T Type of value in the optional.
       * @param unused_opt The `LumexOptional` object.
       * @param unused_nullopt `nullopt` constant.
       * @return `true`.
       */
      template <typename T>
      constexpr bool
      operator>=(LumexOptional<T> const & /*unused_opt*/, nullopt_t /*unused_nullopt*/) noexcept
      {
        return true;
      }

      /**
       * @brief Greater-than-or-equal-to comparison between `nullopt` and a `LumexOptional`.
       * @details `nullopt` is greater than or equal to a `LumexOptional` only if the latter is empty.
       * @tparam T Type of value in the optional.
       * @param unused_nullopt `nullopt` constant.
       * @param opt The `LumexOptional` object.
       * @return `true` if `opt` is empty, `false` otherwise.
       */
      template <typename T>
      constexpr bool
      operator>=(nullopt_t /*unused_nullopt*/, LumexOptional<T> const &opt) noexcept
      {
        return !opt.has_value();
      }

      // Comparison with values
      /**
       * @brief Equality comparison between a `LumexOptional` and a value.
       * @details Returns `true` if the optional has a value and that value is equal to `value`.
       * @tparam T Type of value in the optional.
       * @tparam U Type of the value to compare against.
       * @param opt The `LumexOptional` object.
       * @param value The value to compare with.
       * @return `true` if equal, `false` otherwise.
       */
      template <typename T, typename U>
      constexpr bool
      operator==(LumexOptional<T> const &opt, U const &value)
      {
        return opt.has_value() ? *opt == value : false;
      }

      /**
       * @brief Equality comparison between a value and a `LumexOptional`.
       * @details Returns `true` if the optional has a value and `value` is equal to that value.
       * @tparam T Type of the value to compare against.
       * @tparam U Type of value in the optional.
       * @param value The value to compare with.
       * @param opt The `LumexOptional` object.
       * @return `true` if equal, `false` otherwise.
       */
      template <typename T, typename U>
      constexpr bool
      operator==(T const &value, LumexOptional<U> const &opt)
      {
        return opt.has_value() ? value == *opt : false;
      }

      /**
       * @brief Inequality comparison between a `LumexOptional` and a value.
       * @details Returns `true` if the optional is empty, or if it has a value and that value is not equal to `value`.
       * @tparam T Type of value in the optional.
       * @tparam U Type of the value to compare against.
       * @param opt The `LumexOptional` object.
       * @param value The value to compare with.
       * @return `true` if not equal, `false` otherwise.
       */
      template <typename T, typename U>
      constexpr bool
      operator!=(LumexOptional<T> const &opt, U const &value)
      {
        return opt.has_value() ? *opt != value : true;
      }

      /**
       * @brief Inequality comparison between a value and a `LumexOptional`.
       * @details Returns `true` if the optional is empty, or if it has a value and `value` is not equal to that value.
       * @tparam T Type of the value to compare against.
       * @tparam U Type of value in the optional.
       * @param value The value to compare with.
       * @param opt The `LumexOptional` object.
       * @return `true` if not equal, `false` otherwise.
       */
      template <typename T, typename U>
      constexpr bool
      operator!=(T const &value, LumexOptional<U> const &opt)
      {
        return opt.has_value() ? value != *opt : true;
      }

      /**
       * @brief Less-than comparison between a `LumexOptional` and a value.
       * @details Returns `true` if the optional is empty (empty is less than any value),
       *          or if it has a value and that value is less than `value`.
       * @tparam T Type of value in the optional.
       * @tparam U Type of the value to compare against.
       * @param opt The `LumexOptional` object.
       * @param value The value to compare with.
       * @return `true` if less than, `false` otherwise.
       */
      template <typename T, typename U>
      constexpr bool
      operator<(LumexOptional<T> const &opt, U const &value)
      {
        return opt.has_value() ? *opt < value : true;
      }

      /**
       * @brief Less-than comparison between a value and a `LumexOptional`.
       * @details Returns `true` if the optional has a value and `value` is less than that value.
       *          Returns `false` if the optional is empty (nothing is less than empty).
       * @tparam T Type of the value to compare against.
       * @tparam U Type of value in the optional.
       * @param value The value to compare with.
       * @param opt The `LumexOptional` object.
       * @return `true` if less than, `false` otherwise.
       */
      template <typename T, typename U>
      constexpr bool
      operator<(T const &value, LumexOptional<U> const &opt)
      {
        return opt.has_value() ? value < *opt : false;
      }

      /**
       * @brief Less-than-or-equal-to comparison between a `LumexOptional` and a value.
       * @details Returns `true` if the optional is empty, or if it has a value and that value is less than or equal to
       * `value`.
       * @tparam T Type of value in the optional.
       * @tparam U Type of the value to compare against.
       * @param opt The `LumexOptional` object.
       * @param value The value to compare with.
       * @return `true` if less than or equal to, `false` otherwise.
       */
      template <typename T, typename U>
      constexpr bool
      operator<=(LumexOptional<T> const &opt, U const &value)
      {
        return opt.has_value() ? *opt <= value : true;
      }

      /**
       * @brief Less-than-or-equal-to comparison between a value and a `LumexOptional`.
       * @details Returns `true` if the optional has a value and `value` is less than or equal to that value.
       *          Returns `false` if the optional is empty.
       * @tparam T Type of the value to compare against.
       * @tparam U Type of value in the optional.
       * @param value The value to compare with.
       * @param opt The `LumexOptional` object.
       * @return `true` if less than or equal to, `false` otherwise.
       */
      template <typename T, typename U>
      constexpr bool
      operator<=(T const &value, LumexOptional<U> const &opt)
      {
        return opt.has_value() ? value <= *opt : false;
      }

      /**
       * @brief Greater-than comparison between a `LumexOptional` and a value.
       * @details Returns `true` if the optional has a value and that value is greater than `value`.
       *          Returns `false` if the optional is empty.
       * @tparam T Type of value in the optional.
       * @tparam U Type of the value to compare against.
       * @param opt The `LumexOptional` object.
       * @param value The value to compare with.
       * @return `true` if greater than, `false` otherwise.
       */
      template <typename T, typename U>
      constexpr bool
      operator>(LumexOptional<T> const &opt, U const &value)
      {
        return opt.has_value() ? *opt > value : false;
      }

      /**
       * @brief Greater-than comparison between a value and a `LumexOptional`.
       * @details Returns `true` if the optional is empty, or if it has a value and `value` is greater than that value.
       * @tparam T Type of the value to compare against.
       * @tparam U Type of value in the optional.
       * @param value The value to compare with.
       * @param opt The `LumexOptional` object.
       * @return `true` if greater than, `false` otherwise.
       */
      template <typename T, typename U>
      constexpr bool
      operator>(T const &value, LumexOptional<U> const &opt)
      {
        return opt.has_value() ? value > *opt : true;
      }

      /**
       * @brief Greater-than-or-equal-to comparison between a `LumexOptional` and a value.
       * @details Returns `true` if the optional has a value and that value is greater than or equal to `value`.
       *          Returns `false` if the optional is empty.
       * @tparam T Type of value in the optional.
       * @tparam U Type of the value to compare against.
       * @param opt The `LumexOptional` object.
       * @param value The value to compare with.
       * @return `true` if greater than or equal to, `false` otherwise.
       */
      template <typename T, typename U>
      constexpr bool
      operator>=(LumexOptional<T> const &opt, U const &value)
      {
        return opt.has_value() ? *opt >= value : false;
      }

      /**
       * @brief Greater-than-or-equal-to comparison between a value and a `LumexOptional`.
       * @details Returns `true` if the optional is empty, or if it has a value and `value` is greater than or equal to
       * that value.
       * @tparam T Type of the value to compare against.
       * @tparam U Type of value in the optional.
       * @param value The value to compare with.
       * @param opt The `LumexOptional` object.
       * @return `true` if greater than or equal to, `false` otherwise.
       */
      template <typename T, typename U>
      constexpr bool
      operator>=(T const &value, LumexOptional<U> const &opt)
      {
        return opt.has_value() ? value >= *opt : true;
      }

      // — Specialized algorithms —

      /**
       * @brief Swaps the contents of two `LumexOptional` objects.
       * @details This is a non-member `swap` function following the standard library
       *          pattern, which calls the member `swap` function.
       * @tparam T The type of value contained in the optionals.
       * @param lhs The first `LumexOptional` object.
       * @param rhs The second `LumexOptional` object.
       * @note The `noexcept` specification depends on the member `swap` function.
       */
      template <typename T>
      void
      swap(LumexOptional<T> &lhs, LumexOptional<T> &rhs) noexcept(noexcept(lhs.swap(rhs)))
      {
        lhs.swap(rhs);
      }

      /**
       * @brief Creates a `LumexOptional` object containing a copy or move of a value.
       * @details A factory function to conveniently construct a `LumexOptional` from an existing value.
       *          It uses `std::decay` to get the underlying type and `std::forward` for perfect forwarding.
       * @tparam T The type of the value to store in the optional.
       * @param value The value to initialize the optional with.
       * @return A `LumexOptional` containing the provided value.
       */
      template <typename T>
      constexpr LumexOptional<typename std::decay<T>::type>
      make_optional(T &&value)
      {
        return LumexOptional<typename std::decay<T>::type>(std::forward<T>(value));
      }

      /**
       * @brief Creates a `LumexOptional` object by in-place construction.
       * @details A factory function to conveniently construct a `LumexOptional`
       *          by directly constructing its contained value using the provided arguments.
       * @tparam T The type of the value to construct inside the optional.
       * @tparam Args Variadic template arguments for the constructor of `T`.
       * @param args Arguments to forward to the constructor of `T`.
       * @return A `LumexOptional` containing the newly constructed value.
       */
      template <typename T, typename... Args>
      constexpr LumexOptional<T>
      make_optional(Args &&...args)
      {
        return LumexOptional<T>(in_place, std::forward<Args>(args)...);
      }

      /**
       * @brief Creates a `LumexOptional` object by in-place construction with an initializer list.
       * @details A factory function to conveniently construct a `LumexOptional`
       *          by directly constructing its contained value using an initializer list
       *          and additional arguments.
       * @tparam T The type of the value to construct inside the optional.
       * @tparam U The type of elements in the `initializer_list`.
       * @tparam Args Variadic template arguments for the constructor of `T`.
       * @param ilist An `std::initializer_list` to pass to the constructor of `T`.
       * @param args Additional arguments to forward to the constructor of `T`.
       * @return A `LumexOptional` containing the newly constructed value.
       */
      template <typename T, typename U, typename... Args>
      constexpr LumexOptional<T>
      make_optional(std::initializer_list<U> ilist, Args &&...args)
      {
        return LumexOptional<T>(in_place, ilist, std::forward<Args>(args)...);
      }

    } // namespace Optional
  } // namespace Core
} // namespace Lumex

/**
 * @brief Global alias for `Lumex::Core::Optional::in_place`.
 * @details This allows `in_place` to be used without full namespace qualification,
 *          improving readability and mimicking `std::in_place`.
 */
using Lumex::Core::Optional::in_place;
/**
 * @brief Global alias for `Lumex::Core::Optional::LumexBadOptionalAccess`.
 * @details This allows `LumexBadOptionalAccess` to be used without full namespace qualification.
 */
using Lumex::Core::Optional::LumexBadOptionalAccess;
/**
 * @brief Global alias for `Lumex::Core::Optional::make_optional`.
 * @details This allows `make_optional` to be used without full namespace qualification,
 *          improving readability and mimicking `std::make_optional`.
 */
using Lumex::Core::Optional::make_optional;
/**
 * @brief Global alias for `Lumex::Core::Optional::nullopt`.
 * @details This allows `nullopt` to be used without full namespace qualification,
 *          improving readability and mimicking `std::nullopt`.
 */
using Lumex::Core::Optional::nullopt;

/**
 * @brief Global type alias for `Lumex::Core::Optional::LumexOptional<T>`.
 * @details This allows `LumexOptional` to be used without full namespace qualification,
 *          improving readability and making it behave more like `std::optional`.
 * @tparam T The type of the value to be held.
 */
template <typename T> using LumexOptional = Lumex::Core::Optional::LumexOptional<T>;

// — Hash support —
namespace std
{
  /**
   * @brief Specialization of `std::hash` for `LumexOptional`.
   * @details Enables `LumexOptional` to be used as a key in hash-based containers
   *          like `std::unordered_map` or `std::unordered_set`.
   *          The hash value is 0 if the optional is empty, otherwise it's the hash
   *          of the contained value.
   * @tparam T The type of the value contained in the `LumexOptional`. `std::hash<T>`
   *           must be defined for this specialization to work.
   */
  template <typename T> struct hash<LumexOptional<T>> {
    /**
     * @brief Computes the hash value for a `LumexOptional` object.
     * @param opt The `LumexOptional` object to hash.
     * @return The hash value.
     */
    size_t
    operator()(LumexOptional<T> const &opt) const
    {
      return opt.has_value() ? hash<T>{}(*opt) : 0;
    }
  };
} // namespace std

#endif // !LUMEX_OPTIONAL_HPP
