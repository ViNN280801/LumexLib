#ifndef LUMEX_UNEXPECTED_HPP
#define LUMEX_UNEXPECTED_HPP

#include <utility>

namespace Lumex
{
  namespace Core
  {
    namespace Expected
    {
      // ====================== Unexpected класс (имитация C++23) ======================
      /**
       * @brief Класс, инкапсулирующий значение ошибки для `Expected`.
       * @details Этот класс используется для создания объекта `Expected` в состоянии ошибки.
       *          Он является аналогом `std::unexpected` из C++23.
       * @tparam ErrorType Тип хранимого значения ошибки.
       * @note Объект `Unexpected` всегда находится в состоянии ошибки.
       */
      template <typename ErrorType> class Unexpected
      {
      public:
        /**
         * @brief Конструктор.
         * @param[in] error Константная ссылка на ошибку для хранения.
         * @note Гарантирует невыбрасывание исключений, если конструктор копирования `ErrorType` не выбрасывает.
         */
        explicit Unexpected(ErrorType const &error) : m_error(error) {}

        /**
         * @brief Конструктор перемещения.
         * @param[in] error rvalue ссылка на ошибку для хранения.
         * @note Гарантирует невыбрасывание исключений, если конструктор перемещения `ErrorType` не выбрасывает.
         */
        explicit Unexpected(ErrorType &&error) : m_error(std::move(error)) {}

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
        ErrorType m_error; ///< Хранимое значение ошибки.
      };
    } // namespace Expected
  } // namespace Core
} // namespace Lumex

#endif // !LUMEX_UNEXPECTED_HPP
