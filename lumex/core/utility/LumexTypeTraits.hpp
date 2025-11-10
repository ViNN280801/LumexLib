#ifndef LUMEX_UTILITY_LUMEXTYPETRAITS_HPP
#define LUMEX_UTILITY_LUMEXTYPETRAITS_HPP

#include <functional>
#include <type_traits>
#include <utility>

namespace Lumex
{
  namespace Utility
  {
    namespace TypeTraits
    {
      template <typename...> struct make_void {
        using type = void;
      };

      template <typename... Ts> using void_t = typename make_void<Ts...>::type;

      namespace detail
      {
        template <typename T> struct is_reference_wrapper : std::false_type {};

        template <typename U> struct is_reference_wrapper<std::reference_wrapper<U>> : std::true_type {};

        template <typename T> struct invoke_impl {
          template <typename Func, typename... Args>
          static auto
          call(Func &&func, Args &&...args) -> decltype(std::forward<Func>(func)(std::forward<Args>(args)...));
        };

        template <typename B, typename MT> struct invoke_impl<MT B::*> {
          template <typename T, typename Td = typename std::decay<T>::type,
                    typename = typename std::enable_if<std::is_base_of<B, Td>::value>::type>
          static auto get(T &&arg) -> T &&;

          template <typename T, typename Td = typename std::decay<T>::type,
                    typename = typename std::enable_if<is_reference_wrapper<Td>::value>::type>
          static auto get(T &&arg) -> decltype(arg.get());

          template <typename T, typename Td = typename std::decay<T>::type,
                    typename = typename std::enable_if<!std::is_base_of<B, Td>::value>::type,
                    typename = typename std::enable_if<!is_reference_wrapper<Td>::value>::type>
          static auto get(T &&arg) -> decltype(*std::forward<T>(arg));

          template <typename T, typename... Args, typename MT1,
                    typename = typename std::enable_if<std::is_function<MT1>::value>::type>
          static auto call(MT1 B::*pmf, T &&arg, Args &&...args)
            -> decltype((invoke_impl::get(std::forward<T>(arg)).*pmf)(std::forward<Args>(args)...));

          template <typename T>
          static auto call(MT B::*pmd, T &&arg) -> decltype(invoke_impl::get(std::forward<T>(arg)).*pmd);
        };

        template <typename Func, typename... Args, typename FuncDecayed = typename std::decay<Func>::type>
        auto INVOKE(Func &&func, Args &&...args)
          -> decltype(invoke_impl<FuncDecayed>::call(std::forward<Func>(func), std::forward<Args>(args)...));

        // SFINAE core: if INVOKE(...) is well-formed, expose ::type = decltype(INVOKE(...))
        template <typename AlwaysVoid, typename /*F*/, typename... /*Args*/>
        struct invoke_result_impl { /* no ::type when ill-formed */
        };

        template <typename F, typename... Args>
        struct invoke_result_impl<void_t<decltype(INVOKE(std::declval<F>(), std::declval<Args>()...))>, F, Args...> {
          using type = decltype(INVOKE(std::declval<F>(), std::declval<Args>()...));
        };
      } // namespace detail

      template <typename Func, typename... Args>
      struct invoke_result : detail::invoke_result_impl<void, Func, Args...> {};

      template <typename Func, typename... Args> using invoke_result_t = typename invoke_result<Func, Args...>::type;

      template <typename Sig> struct result_of; // not defined

      template <typename Func, typename... Args> struct result_of<Func(Args...)> : invoke_result<Func, Args...> {};

      template <typename Sig> using result_of_t = typename result_of<Sig>::type;

      template <typename T, typename = void> struct has_type : std::false_type {};

      template <typename T> struct has_type<T, void_t<typename T::type>> : std::true_type {};

      template <typename Func, typename... Args> struct is_invocable : has_type<invoke_result<Func, Args...>> {};

      template <typename Sig> struct is_callable;

      template <typename Func, typename... Args> struct is_callable<Func(Args...)> : is_invocable<Func, Args...> {};

      template <typename TypeToClean>
      using CleanType = typename std::remove_reference<typename std::remove_cv<TypeToClean>::type>::type;

      template <typename T> using CleanTypeOf = CleanType<T>;

      template <typename TargetType, typename SourceType>
      inline CleanType<TargetType>
      safe_cast(SourceType &&value) noexcept(
        noexcept(static_cast<CleanType<TargetType>>(std::forward<SourceType>(value))))
      {
        return static_cast<CleanType<TargetType>>(std::forward<SourceType>(value));
      }
    } // namespace TypeTraits
  } // namespace Utility
} // namespace Lumex

#endif // !LUMEX_UTILITY_LUMEXTYPETRAITS_HPP
