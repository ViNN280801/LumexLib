#ifndef LUMEX_BAD_EXPECTED_ACCESS_HPP
#define LUMEX_BAD_EXPECTED_ACCESS_HPP

#include <exception>
#include <utility>

#include "lumex/core/utility/LumexKeywords.hpp"

// ====================== BadExpectedAccess класс (имитация C++23) ======================

namespace Lumex
{
  namespace Core
  {
    namespace Expected
    {
      /**
       * @brief Класс исключений, выбрасываемый при попытке доступа к отсутствующему значению или ошибке в `Expected`.
       * @details Это аналог `std::bad_expected_access` из C++23. Выбрасывается функциями `value()`
       *          или `error()` если `Expected` находится в неверном состоянии (например, вызов `value()`
       *          на объекте, содержащем ошибку).
       * @tparam ErrorType Тип ошибки, которая хранится и может быть извлечена из исключения.
       * @note Не является потокобезопасным, если `ErrorType` не является потокобезопасным.
       * @warning Создание `BadExpectedAccess` может быть дорогостоящим, если `ErrorType` имеет
       *          тяжелый конструктор или аллокации. Может быть выброшено функциями `value()` и `error()`.
       */
      template <typename ErrorType> class BadExpectedAccess : public std::exception
      {
      public:
        /**
         * @brief Конструктор.
         * @param[in] error Значение ошибки, которое будет храниться внутри исключения. Перемещается.
         * @note Гарантирует невыбрасывание исключений, если конструктор перемещения `ErrorType` не выбрасывает.
         */
        explicit BadExpectedAccess(ErrorType error) : m_error(std::move(error)) {}
        /**
         * @brief Возвращает строковое описание исключения.
         * @return C-строка с описанием "Bad expected access".
         * @note Гарантированно не выбрасывает исключений (`noexcept`).
         */
        char const *
        what() const LUMEX_NOEXCEPT_FUNCTION override
        {
          return "Bad expected access";
        }

        /**
         * @brief Возвращает изменяемую lvalue ссылку на хранимую ошибку.
         * @return Ссылка на ошибку типа `ErrorType`.
         * @note Эта функция не выбрасывает исключений.
         */
        ErrorType &
        error() &
        {
          return m_error;
        }

        /**
         * @brief Возвращает константную lvalue ссылку на хранимую ошибку.
         * @return Константная ссылка на ошибку типа `ErrorType`.
         * @note Эта функция не выбрасывает исключений.
         */
        ErrorType const &
        error() const &
        {
          return m_error;
        }

        /**
         * @brief Возвращает rvalue ссылку на хранимую ошибку.
         * @return rvalue ссылка на ошибку типа `ErrorType`.
         * @note Эта функция не выбрасывает исключений.
         */
        ErrorType &&
        error() &&
        {
          return std::move(m_error);
        }

        /**
         * @brief Возвращает константную rvalue ссылку на хранимую ошибку.
         * @return Константная rvalue ссылка на ошибку типа `ErrorType`.
         * @note Эта функция не выбрасывает исключений.
         */
        ErrorType const &&
        error() const &&
        {
          return std::move(m_error);
        }

      private:
        /**
         * @brief Хранимое значение ошибки.
         * @details Содержит объект типа `ErrorType`, который представляет собой информацию об ошибке.
         */
        ErrorType m_error;
      };
    } // namespace Expected
  } // namespace Core
} // namespace Lumex

#endif // !LUMEX_BAD_EXPECTED_ACCESS_HPP
